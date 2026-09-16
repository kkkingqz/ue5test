#!/usr/bin/env python3
"""Static ownership gate for canonical state composition (CFC-05A, INV-013).

Validates that:
1. Canonical state composition belongs exclusively to Lua (Scripts/runtime/state_composition.lua).
2. The runtime session implementation (Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp)
   does not contain schema-shaped section keys:
   ("meta", "mods", "player", "actors", "item_instances", "quests", "definitions")
   or nested keys ("instance_counters", "prng", "time", "schema_version", "save_version", "save_id").
3. C++ source (Source/GV2RuntimeCore/) does not contain native merge routines
   (MergeStateContribution, IsCanonicalStateSection) or schema-shaped traversal over canonical state.
4. Scripts/runtime/state_composition.lua exists and exports compose_default_state and merge_contribution.
5. Scripts/bootstrap/manifest.lua and Scripts/bootstrap/main.lua properly wire the module.
6. Tests/Lua/lifecycle/state_contributions.lua exists.
"""

from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SOURCE_ROOT = REPO_ROOT / "Source" / "GV2RuntimeCore"
SESSION_IMPL = SOURCE_ROOT / "Private" / "GV2RuntimeSession.cpp"
SCRIPTS_ROOT = REPO_ROOT / "Scripts"
TESTS_ROOT = REPO_ROOT / "Tests"

# Fixed boundary handles:
# - Global facade "game", boundary assignment slot "state", facade sub-tables, bootstrap globals.
FIXED_BOUNDARY_HANDLES = {
    "game",
    "state",
    "runtime",
    "ui",
    "debug",
    "null",
    "repository",
    "save_slots",
    "require",
    "require_base",
    "next",
}

# Generic Lua metamethods:
GENERIC_METAMETHODS = {
    "__index",
    "__newindex",
    "__metatable",
    "__pairs",
}

# Generic type, diagnostic, session, fault, and package lifetime fields:
GENERIC_TYPE_AND_LIFETIME = {
    "code",
    "message",
    "error",
    "phase",
    "package_id",
    "packages",
    "script_set_hash",
    "session_generation",
    "seed_hex",
    "canonical_id",
    "requested_id",
    "registry_path",
}

# Module manifest & dependency graph fields:
MODULE_MANIFEST_AND_SPEC = {
    "modules",
    "dependencies",
    "replaceable",
    "authoring",
    "module_id",
    "module",
}

# Canonical core module IDs:
FIXED_MODULE_IDS = {
    "core:module.authoring.context",
    "core:module.bootstrap.main",
    "core:module.bootstrap.registry_lifecycle",
    "core:module.runtime.load",
    "core:module.runtime.migrate",
    "core:module.runtime.mutation_window",
    "core:module.runtime.state_composition",
    "core:module.runtime.state_validator",
}

# Canonical lifecycle entry point functions:
FIXED_LIFECYCLE_ENTRY_POINTS = {
    "compose_default_state",
    "create_authoring_environment",
    "decode_and_prepare",
    "get_canonical_state_hash",
    "guard_state",
    "migrate_state",
    "restore_instances",
    "seal",
    "stop",
    "take_pending_screen",
    # PEP-03 (ADR-0047): mirrors take_pending_screen's own entry above -- the effect
    # queue's Lua-pull entry point.
    "take_pending_effects",
    "unregister",
    "validate_state",
    "validate_state_tree",
    "verify_complete",
}

# Fixed repository/save service API methods:
FIXED_SERVICE_API_METHODS = {
    "get",
    "require",
    "list",
    "exists",
    "write",
}

# Presentation document deserialization fields:
PRESENTATION_DOCUMENT_FIELDS = {
    "route",
    "screen_id",
    "layer",
    "instance_key",
    "fields",
    "value",
    "modals",
    "overlays",
    "revision",
    "schema_id",
    "ui_instance_id",
    # PEP-03 (ADR-0047): one-shot presentation effect DTO fields -- effect_id/target/args
    # mirror screen_id/route/fields' own deserialization-field status above; none of
    # these three names a canonical state section (see FORBIDDEN_SCHEMA_TRAVERSAL_LITERALS
    # below), so they belong here, not there.
    "effect_id",
    "target",
    "args",
}

# Host control, bridge and safe-point save entry points:
HOST_CONTROL_AND_BRIDGE_FIELDS = {
    "bridge",
    "save_to_slot",
    "preflight_save_bytes",
    "take_pending_requests",
    "kind",
    "slot_id",
}

CLASSIFIED_ALLOWED_LITERALS = (
    FIXED_BOUNDARY_HANDLES
    | GENERIC_METAMETHODS
    | GENERIC_TYPE_AND_LIFETIME
    | MODULE_MANIFEST_AND_SPEC
    | FIXED_MODULE_IDS
    | FIXED_LIFECYCLE_ENTRY_POINTS
    | FIXED_SERVICE_API_METHODS
    | PRESENTATION_DOCUMENT_FIELDS
    | HOST_CONTROL_AND_BRIDGE_FIELDS
)

# Known canonical state section and subkey names (schema-shaped traversal is strictly forbidden in C++):
FORBIDDEN_SCHEMA_TRAVERSAL_LITERALS = {
    "meta",
    "mods",
    "player",
    "actors",
    "item_instances",
    "quests",
    "definitions",
    "instance_counters",
    "prng",
    "time",
    "schema_version",
    "save_version",
    "save_id",
}

FORBIDDEN_CPP_IDENTIFIERS = [
    "MergeStateContribution",
    "IsCanonicalStateSection",
]


def strip_cpp_comments_and_raw_strings(content: str) -> str:
    """Removes comments and raw string literals from C++ code."""
    # Raw string literals: R"delim(...)delim"
    content = re.sub(r'R"([a-zA-Z0-9_]*)\(.*?\)\1"', '""', content, flags=re.DOTALL)
    # Multi-line comments: /* ... */
    content = re.sub(r"/\*.*?\*/", "", content, flags=re.DOTALL)
    # Single-line comments: // ...
    content = re.sub(r"//[^\n]*", "", content)
    return content


def extract_lua_c_api_accesses(source: str) -> list[tuple[int, str, str]]:
    """Extracts (line_number, api_function, literal_arg) for all Lua table/field access calls."""
    lines = source.splitlines()
    accesses = []
    field_pattern = re.compile(
        r'\b(lua_getfield|lua_setfield)\s*\(\s*[^,]+,\s*[^,]+,\s*"([^"]+)"\s*\)'
    )
    global_pattern = re.compile(
        r'\b(lua_getglobal|lua_setglobal)\s*\(\s*[^,]+,\s*"([^"]+)"\s*\)'
    )
    for idx, line in enumerate(lines, start=1):
        stripped = line.strip()
        if stripped.startswith("//") or stripped.startswith("*"):
            continue
        for match in field_pattern.finditer(line):
            accesses.append((idx, match.group(1), match.group(2)))
        for match in global_pattern.finditer(line):
            accesses.append((idx, match.group(1), match.group(2)))
    return accesses


def validate_session_implementation(session_file: Path) -> list[str]:
    errors: list[str] = []
    if not session_file.exists():
        return [f"Session implementation file not found: {session_file}"]

    content = session_file.read_text(encoding="utf-8")
    stripped = strip_cpp_comments_and_raw_strings(content)

    rel_path = (
        session_file.relative_to(REPO_ROOT)
        if session_file.is_relative_to(REPO_ROOT)
        else session_file.name
    )

    accesses = extract_lua_c_api_accesses(stripped)
    for line_idx, api_call, literal in accesses:
        # Check 1: schema-shaped traversal / canonical state section access
        if literal in FORBIDDEN_SCHEMA_TRAVERSAL_LITERALS:
            errors.append(
                f"{rel_path}:{line_idx}: forbidden schema-shaped state access literal '{literal}' in {api_call}(). "
                f"Canonical state composition and traversal belong exclusively to Lua."
            )
            continue

        # Check 2: "state" must only be assigned via lua_setfield (game.state = opaque_tree)
        # Any lua_getfield("state") is an attempt to read/traverse canonical state from C++
        if literal == "state" and api_call != "lua_setfield":
            errors.append(
                f"{rel_path}:{line_idx}: forbidden reading of canonical state handle via {api_call}('state'). "
                f"Canonical state boundary handle may only be assigned via lua_setfield."
            )
            continue

        # Check 3: Every literal must belong to classified allowed categories
        if literal not in CLASSIFIED_ALLOWED_LITERALS:
            errors.append(
                f"{rel_path}:{line_idx}: unclassified Lua C API access literal '{literal}' in {api_call}(). "
                f"All semantic state and boundary accesses in session implementation must be classified."
            )

    # Check 4: Global presence of forbidden schema literals as raw tokens
    for literal in FORBIDDEN_SCHEMA_TRAVERSAL_LITERALS:
        pattern = rf'"{re.escape(literal)}"'
        if re.search(pattern, stripped):
            errors.append(
                f"{rel_path}: forbidden schema-shaped literal '\"{literal}\"' found in session implementation"
            )

    # Check 5: No forbidden native merge identifiers
    for identifier in FORBIDDEN_CPP_IDENTIFIERS:
        if re.search(r"\b" + re.escape(identifier) + r"\b", stripped):
            errors.append(
                f"{rel_path}: forbidden native state merge identifier '{identifier}' found in session implementation"
            )

    # Check 6: the save container stays opaque to C++ (ADR-0021). The actual set here is every
    # use of the container-bytes parameter itself, not a list of function names: a use is
    # classified by the token that follows it, and only forwarding, dereferencing and null
    # comparison are accepted. Member access, subscripting or any call ON the bytes is an
    # inspection of gameplay-owned encoding and turns this red -- which is how a seed, a
    # version or any other field gets read back out of the container.
    errors.extend(validate_container_bytes_opacity(stripped, rel_path))

    return errors


CONTAINER_BYTES_IDENTIFIER = "LoadContainerBytes"


def validate_container_bytes_opacity(stripped: str, rel_path) -> list[str]:
    errors: list[str] = []
    for match in re.finditer(r"\b" + re.escape(CONTAINER_BYTES_IDENTIFIER) + r"\b", stripped):
        tail = stripped[match.end():].lstrip()
        line_idx = stripped.count("\n", 0, match.start()) + 1
        if tail.startswith("->") or tail.startswith("[") or (tail.startswith(".") and not tail.startswith("...")):
            errors.append(
                f"{rel_path}:{line_idx}: '{CONTAINER_BYTES_IDENTIFIER}' is inspected, not forwarded. "
                f"The save container is opaque bytes in C++ (ADR-0021); any field inside it -- seed, "
                f"version, section -- is read by Lua, which owns the encoding."
            )
    # A dereferenced form reaches members without touching the identifier's own tail.
    for match in re.finditer(r"\(\s*\*\s*" + re.escape(CONTAINER_BYTES_IDENTIFIER) + r"\s*\)\s*(\.|->|\[)", stripped):
        line_idx = stripped.count("\n", 0, match.start()) + 1
        errors.append(
            f"{rel_path}:{line_idx}: dereferenced '{CONTAINER_BYTES_IDENTIFIER}' is inspected, not forwarded. "
            f"The save container is opaque bytes in C++ (ADR-0021)."
        )
    return errors


def validate_cpp_module_sources(source_dir: Path) -> list[str]:
    errors: list[str] = []
    if not source_dir.exists():
        return [f"Source directory not found: {source_dir}"]

    # Check that no C++ file in the module contains forbidden merge routines
    for path in sorted(source_dir.rglob("*")):
        if path.suffix not in (".cpp", ".h", ".inl"):
            continue
        if "ThirdParty" in path.parts:
            continue

        content = path.read_text(encoding="utf-8")
        stripped = strip_cpp_comments_and_raw_strings(content)

        rel_path = path.relative_to(REPO_ROOT) if path.is_relative_to(REPO_ROOT) else path.name
        for identifier in FORBIDDEN_CPP_IDENTIFIERS:
            if re.search(r"\b" + re.escape(identifier) + r"\b", stripped):
                errors.append(
                    f"{rel_path}: forbidden native state merge identifier '{identifier}' found in C++ source"
                )

    return errors


def validate_lua_state_composition(scripts_root: Path, tests_root: Path) -> list[str]:
    errors: list[str] = []
    comp_file = scripts_root / "runtime" / "state_composition.lua"
    if not comp_file.exists():
        return [f"Missing required state composition module: {comp_file}"]

    content = comp_file.read_text(encoding="utf-8")
    if 'id = "core:module.runtime.state_composition"' not in content:
        errors.append(f"{comp_file}: module ID must be core:module.runtime.state_composition")

    if "compose_default_state" not in content:
        errors.append(f"{comp_file}: missing compose_default_state function")

    if "merge_contribution" not in content:
        errors.append(f"{comp_file}: missing merge_contribution function")

    # Check manifest.lua
    manifest_file = scripts_root / "bootstrap" / "manifest.lua"
    if manifest_file.exists():
        manifest_text = manifest_file.read_text(encoding="utf-8")
        if "core:module.runtime.state_composition" not in manifest_text:
            errors.append(f"{manifest_file}: core:module.runtime.state_composition not registered in manifest")

    # Check main.lua
    main_file = scripts_root / "bootstrap" / "main.lua"
    if main_file.exists():
        main_text = main_file.read_text(encoding="utf-8")
        if "state_composition" not in main_text:
            errors.append(f"{main_file}: state_composition not imported/exported in main.lua")

    # Check spec file
    spec_file = tests_root / "Lua" / "lifecycle" / "state_contributions.lua"
    if not spec_file.exists():
        errors.append(f"Missing required lifecycle spec: {spec_file}")

    return errors


def run_self_test() -> int:
    print("Running validate_state_composition_ownership.py --self-test...")

    # 1. Clean run on repository must pass
    session_errors = validate_session_implementation(SESSION_IMPL)
    module_errors = validate_cpp_module_sources(SOURCE_ROOT)
    lua_errors = validate_lua_state_composition(SCRIPTS_ROOT, TESTS_ROOT)
    all_errors = session_errors + module_errors + lua_errors
    if all_errors:
        print(f"Self-test failed on actual repo:\n" + "\n".join(all_errors))
        return 1

    # 2. Negative mutations
    with tempfile.TemporaryDirectory() as tmp_dir:
        tmp_path = Path(tmp_dir)

        # Mutation A: forbidden section literal in session file
        bad_session = tmp_path / "GV2RuntimeSession.cpp"
        bad_session.write_text('const char* sec = "instance_counters";', encoding="utf-8")
        errors = validate_session_implementation(bad_session)
        if not any('"instance_counters"' in e for e in errors):
            print("Self-test failed: did not detect forbidden subkey '\"instance_counters\"'")
            return 1

        # Mutation B: forbidden identifier in session file
        bad_session.write_text("void MergeStateContribution() {}", encoding="utf-8")
        errors = validate_session_implementation(bad_session)
        if not any("MergeStateContribution" in e for e in errors):
            print("Self-test failed: did not detect forbidden identifier MergeStateContribution")
            return 1

        # Mutation C: forbidden section name "meta" in session file
        bad_session.write_text('if (k == "meta") {}', encoding="utf-8")
        errors = validate_session_implementation(bad_session)
        if not any('"meta"' in e for e in errors):
            print("Self-test failed: did not detect forbidden section literal '\"meta\"'")
            return 1

        # Mutation D: unclassified new section name in lua_getfield
        bad_session.write_text('void ReadSec(lua_State* L) { lua_getfield(L, -1, "inventory"); }', encoding="utf-8")
        errors = validate_session_implementation(bad_session)
        if not any("unclassified Lua C API access literal 'inventory'" in e for e in errors):
            print("Self-test failed: did not detect unclassified Lua C API access literal 'inventory'")
            return 1

        # Mutation E: forbidden reading of canonical state via lua_getfield("state")
        bad_session.write_text('void ReadState(lua_State* L) { lua_getfield(L, -1, "state"); }', encoding="utf-8")
        errors = validate_session_implementation(bad_session)
        if not any("forbidden reading of canonical state handle" in e for e in errors):
            print("Self-test failed: did not detect forbidden reading of canonical state via lua_getfield('state')")
            return 1

        # Mutation G: container bytes inspected instead of forwarded (three shapes), and the
        # legitimate forwarding/null-check shapes must stay accepted -- otherwise the rule
        # would just be "never mention the parameter".
        for bad_use, label in (
            ('if (LoadContainerBytes->find("8:seed_hexs16:") != npos) {}', "arrow member access"),
            ('const char C = LoadContainerBytes[0];', "subscript"),
            ('if ((*LoadContainerBytes).starts_with("SYNTHETIC")) {}', "dereferenced member access"),
        ):
            bad_session.write_text(f"void Use(const std::string* LoadContainerBytes) {{ {bad_use} }}", encoding="utf-8")
            errors = validate_session_implementation(bad_session)
            if not any("is inspected, not forwarded" in e for e in errors):
                print(f"Self-test failed: did not detect container bytes inspection ({label})")
                return 1

        good_session_uses = (
            "bool Forward(const std::string* LoadContainerBytes) {\n"
            "    if (LoadContainerBytes == nullptr) { return false; }\n"
            "    return Decode(*LoadContainerBytes);\n"
            "}\n"
        )
        bad_session.write_text(good_session_uses, encoding="utf-8")
        errors = validate_session_implementation(bad_session)
        if any("is inspected, not forwarded" in e for e in errors):
            print("Self-test failed: rejected legitimate forwarding/null-check of container bytes")
            return 1

        # Mutation F: missing compose_default_state in Lua module
        mut_scripts_dir = tmp_path / "Scripts"
        (mut_scripts_dir / "runtime").mkdir(parents=True)
        bad_lua = mut_scripts_dir / "runtime" / "state_composition.lua"
        bad_lua.write_text('local M = { id = "core:module.runtime.state_composition" }\nreturn M', encoding="utf-8")
        errors = validate_lua_state_composition(mut_scripts_dir, TESTS_ROOT)
        if not any("compose_default_state" in e for e in errors):
            print("Self-test failed: did not detect missing compose_default_state")
            return 1

    print("All validate_state_composition_ownership self-tests PASSED.")
    return 0


def main() -> int:
    if "--self-test" in sys.argv:
        return run_self_test()

    errors: list[str] = []
    errors.extend(validate_session_implementation(SESSION_IMPL))
    errors.extend(validate_cpp_module_sources(SOURCE_ROOT))
    errors.extend(validate_lua_state_composition(SCRIPTS_ROOT, TESTS_ROOT))

    if errors:
        print("State composition ownership validation FAILED:", file=sys.stderr)
        for err in errors:
            print(f"  ERROR: {err}", file=sys.stderr)
        return 1

    print("State composition ownership validation PASSED.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
