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

FORBIDDEN_SESSION_SECTION_LITERALS = [
    '"meta"',
    '"mods"',
    '"player"',
    '"actors"',
    '"item_instances"',
    '"quests"',
    '"definitions"',
]

FORBIDDEN_SESSION_SUBKEY_LITERALS = [
    '"instance_counters"',
    '"prng"',
    '"time"',
    '"schema_version"',
    '"save_version"',
    '"save_id"',
]

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


def validate_session_implementation(session_file: Path) -> list[str]:
    errors: list[str] = []
    if not session_file.exists():
        return [f"Session implementation file not found: {session_file}"]

    content = session_file.read_text(encoding="utf-8")
    stripped = strip_cpp_comments_and_raw_strings(content)

    rel_path = session_file.relative_to(REPO_ROOT) if session_file.is_relative_to(REPO_ROOT) else session_file.name

    for literal in FORBIDDEN_SESSION_SECTION_LITERALS:
        if literal in stripped:
            errors.append(
                f"{rel_path}: forbidden schema-shaped section literal {literal} found in session implementation"
            )

    for literal in FORBIDDEN_SESSION_SUBKEY_LITERALS:
        if literal in stripped:
            errors.append(
                f"{rel_path}: forbidden schema-shaped subkey literal {literal} found in session implementation"
            )

    for identifier in FORBIDDEN_CPP_IDENTIFIERS:
        if re.search(r"\b" + re.escape(identifier) + r"\b", stripped):
            errors.append(
                f"{rel_path}: forbidden native state merge identifier '{identifier}' found in session implementation"
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

        # Mutation D: missing compose_default_state in Lua module
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
