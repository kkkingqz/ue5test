#!/usr/bin/env python3
"""Static ownership gate for registry lifecycle and sealing (CFC-05, plan SessionLifecycle, M1).

Validates that:
1. Scripts/bootstrap/registry_lifecycle.lua exclusively owns the closed DESCRIPTOR of all engine registries.
2. The DESCRIPTOR order strictly matches the contract sequence per RuntimeFacadeAndRegistries.md.
3. Every DESCRIPTOR participant defines seal and is_frozen.
4. No file outside registry_lifecycle.lua publishes or overwrites registry facade slots or creates ad hoc slots on game.
5. All registry factories (create_registry) in Scripts/runtime/ are registered in registry_lifecycle.lua.
6. The native lifecycle consumes the single SealRegistries result in a fail-closed guard.
"""

from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SCRIPTS_ROOT = REPO_ROOT / "Scripts"
GAMEDATA_ROOT = REPO_ROOT / "GameData"
RUNTIME_SESSION_PATH = REPO_ROOT / "Source" / "GV2RuntimeCore" / "Private" / "GV2RuntimeSession.cpp"

CONTRACT_SEQUENCE = [
    "authoring",
    "services",
    "actions",
    "entity_extensions",
    "commands.validators",
    "commands.handlers",
    "events.subscribers",
    "events",
    "instances.actors",
    "instances",
    "presentation",
    "state_validator",
]

ALLOWED_TOPLEVEL_GAME_ASSIGNMENTS = {
    "state",
    "runtime",
}

FORBIDDEN_REGISTRY_SLOT_ASSIGNMENTS = [
    re.compile(r"\bgame\.(services|actions|entity_extensions|commands|events|instances|presentation)\s*="),
    re.compile(r"\bgame\.commands\.(validators|handlers|enqueue|clear_queue|get_queue_length|drain_queue)\s*="),
    re.compile(r"\bgame\.events\.(subscribers|enqueue|emit|subscribe|freeze|is_frozen)\s*="),
    re.compile(r"\bgame\.instances\.(actors|world|register_kind|create|freeze)\s*="),
    re.compile(r"\bgame\[.*?\]\s*="),
    re.compile(r"\b_G\.game\.(services|actions|entity_extensions|commands|events|instances|presentation)\s*="),
    re.compile(r"\b_G\.game\[.*?\]\s*="),
    re.compile(r"\brawset\s*\(\s*(?:_G\.)?game\s*,"),
    re.compile(r"\brawset\s*\(\s*(?:_G\.)?game\.[a-zA-Z0-9_.]+\s*,"),
    re.compile(r"\brawset\s*\(\s*(?:_G\.)?game\[.*?\]\s*,"),
]

CREATE_REGISTRY_PATTERN = re.compile(r"\bfunction\s+(?:M\.)?create_registry\s*\(")
DESCRIPTOR_ENTRY_PATTERN = re.compile(r'facade_path\s*=\s*"([^"]+)"')


def strip_lua_comments(content: str) -> str:
    """Removes single-line and multi-line comments from Lua code."""
    # Multi-line comments: --[[ ... ]] or --[=[ ... ]=]
    content = re.sub(r"--\[(=*)\[.*?\]\1\]", "", content, flags=re.DOTALL)
    # Single-line comments
    content = re.sub(r"--[^\n]*", "", content)
    return content


def strip_cpp_comments(content: str) -> str:
    """Remove comments while preserving executable call syntax."""
    content = re.sub(r"/\*.*?\*/", "", content, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", content)


def validate_native_seal_result_source(content: str, label: str) -> list[str]:
    """Inventory the definition and sole call, then require fail-closed consumption."""
    stripped = strip_cpp_comments(content)
    sites = list(re.finditer(r"\bSealRegistries\s*\(", stripped))
    guard_sites = list(
        re.finditer(
            r"\bif\s*\(\s*!\s*SealRegistries\s*\(\s*OutFault\s*\)\s*\)",
            stripped,
        )
    )

    errors: list[str] = []
    if len(sites) != 2:
        errors.append(
            f"{label}: SealRegistries inventory must contain exactly its definition and one production call; "
            f"found {len(sites)} token sites"
        )
    if len(guard_sites) != 1:
        errors.append(
            f"{label}: SealRegistries return value must be consumed exactly once by "
            "if (!SealRegistries(OutFault)); ignored or duplicated sealing is forbidden"
        )
    return errors


def validate_native_seal_result(path: Path = RUNTIME_SESSION_PATH) -> list[str]:
    if not path.exists():
        return [f"Missing native runtime session source: {path}"]
    return validate_native_seal_result_source(
        path.read_text(encoding="utf-8"),
        str(path.relative_to(REPO_ROOT)) if path.is_relative_to(REPO_ROOT) else str(path),
    )


ALLOWED_INSTALL_NON_REGISTRY_SLOTS = {
    "runtime",
    "runtime.phase",
    "commands.enqueue",
    "commands.clear_queue",
    "commands.get_queue_length",
    "commands.drain_queue",
    "random",
}


def validate_descriptor(lifecycle_file: Path) -> tuple[list[str], list[str]]:
    errors: list[str] = []
    desc_entries: list[str] = []
    if not lifecycle_file.exists():
        return [f"Missing required registry lifecycle file: {lifecycle_file}"], []

    content = lifecycle_file.read_text(encoding="utf-8")
    stripped = strip_lua_comments(content)

    # Extract DESCRIPTOR block
    desc_start = stripped.find("DESCRIPTOR = {")
    if desc_start == -1:
        desc_start = stripped.find("local DESCRIPTOR = {")
    if desc_start == -1:
        return [f"{lifecycle_file}: could not find DESCRIPTOR table definition"], []

    desc_end = stripped.find("M.descriptor = DESCRIPTOR", desc_start)
    if desc_end == -1:
        desc_end = len(stripped)
    desc_block = stripped[desc_start:desc_end]

    # Match all facade_path entries in DESCRIPTOR
    desc_entries = DESCRIPTOR_ENTRY_PATTERN.findall(desc_block)
    if not desc_entries:
        return [f"{lifecycle_file}: no facade_path entries found in DESCRIPTOR"], []

    if desc_entries != CONTRACT_SEQUENCE:
        errors.append(
            f"{lifecycle_file}: DESCRIPTOR order mismatch!\n"
            f"  Expected: {CONTRACT_SEQUENCE}\n"
            f"  Actual:   {desc_entries}"
        )

    # Check that each entry block contains seal, is_frozen, and create (if path is present)
    entry_chunks = desc_block.split("facade_path = ")
    for chunk in entry_chunks[1:]:
        m_name = re.match(r'"([^"]+)"', chunk.strip())
        path_name = m_name.group(1) if m_name else "unknown"
        if not re.search(r"\bseal\s*=", chunk):
            errors.append(f"{lifecycle_file}: participant '{path_name}' missing seal method in DESCRIPTOR")
        if not re.search(r"\bis_frozen\s*=", chunk):
            errors.append(f"{lifecycle_file}: participant '{path_name}' missing is_frozen predicate in DESCRIPTOR")
        if re.search(r"\bpath\s*=", chunk):
            if not re.search(r"\b(?:create|factory)\s*=", chunk):
                errors.append(f"{lifecycle_file}: participant '{path_name}' with facade slot missing create factory in DESCRIPTOR")

    return errors, desc_entries


def validate_install_against_descriptor(lifecycle_file: Path, desc_entries: list[str]) -> list[str]:
    errors: list[str] = []
    if not lifecycle_file.exists():
        return [f"Missing required registry lifecycle file: {lifecycle_file}"]

    content = lifecycle_file.read_text(encoding="utf-8")
    stripped = strip_lua_comments(content)

    install_match = re.search(r"function\s+M\.install\s*\(\)(.*?)end\s*(?:function|\Z)", stripped, re.DOTALL)
    if not install_match:
        return [f"{lifecycle_file}: could not find M.install() function definition"]

    install_body = install_match.group(1)

    # 1. Must derive registry installation from DESCRIPTOR
    if "DESCRIPTOR" not in install_body or ("ipairs(DESCRIPTOR)" not in install_body and "pairs(DESCRIPTOR)" not in install_body):
        errors.append(f"{lifecycle_file}: install() must derive registry installation by iterating DESCRIPTOR")

    # 2. Extract and validate all rawset calls
    rawset_calls = re.findall(r"rawset\s*\((.*?)\)", install_body)
    for call in rawset_calls:
        args = [a.strip() for a in call.split(",", 2)]
        if len(args) < 2:
            errors.append(f"{lifecycle_file}: malformed rawset call in install(): 'rawset({call})'")
            continue

        target, key = args[0], args[1]
        # Loop variables in descriptor traversal
        if key in ("seg", "leaf"):
            continue

        # String literal key
        m_key = re.match(r'^"([^"]+)"$', key)
        if not m_key:
            errors.append(f"{lifecycle_file}: unknown assignment form in install(): dynamic rawset key '{key}' in 'rawset({call})'")
            continue

        slot_name = m_key.group(1)
        if target in ("game_ref", "game", "cur", "_G.game"):
            full_slot = slot_name
        elif target in ("runtime", "commands", "events", "instances"):
            full_slot = f"{target}.{slot_name}"
        else:
            full_slot = f"{target}.{slot_name}"

        if full_slot not in ALLOWED_INSTALL_NON_REGISTRY_SLOTS and full_slot not in desc_entries:
            errors.append(
                f"{lifecycle_file}: registry slot '{full_slot}' is installed in install() but missing from DESCRIPTOR."
            )

    # 3. Extract and validate all dot-assignments like game_ref.slot = ... or target.slot = ...
    dot_assigns = re.findall(r"\b(game_ref|game|_G\.game|commands|events|instances|runtime)\.([a-zA-Z0-9_]+)\s*=", install_body)
    for target, slot_name in dot_assigns:
        if target in ("game_ref", "game", "_G.game"):
            full_slot = slot_name
        else:
            full_slot = f"{target}.{slot_name}"

        if full_slot not in ALLOWED_INSTALL_NON_REGISTRY_SLOTS and full_slot not in desc_entries:
            errors.append(
                f"{lifecycle_file}: registry slot '{full_slot}' is installed in install() but missing from DESCRIPTOR."
            )

    return errors


def validate_scripts_ownership(scripts_root: Path) -> list[str]:
    errors: list[str] = []
    lifecycle_rel = Path("bootstrap") / "registry_lifecycle.lua"

    for lua_path in sorted(scripts_root.rglob("*.lua")):
        rel = lua_path.relative_to(scripts_root)
        if rel == lifecycle_rel:
            continue

        raw_content = lua_path.read_text(encoding="utf-8")
        clean_content = strip_lua_comments(raw_content)

        # Check for forbidden registry slot assignments
        for line_no, line in enumerate(clean_content.splitlines(), start=1):
            line_str = line.strip()
            if not line_str:
                continue

            for pattern in FORBIDDEN_REGISTRY_SLOT_ASSIGNMENTS:
                if pattern.search(line_str):
                    errors.append(
                        f"{rel}:{line_no}: unauthorized registry slot assignment: '{line_str}'. "
                        f"All engine registry slots are owned and installed exclusively by {lifecycle_rel}."
                    )
                    break

            # Check for ad-hoc top-level game.<slot> = ...
            adhoc_match = re.search(r"\bgame\.([a-zA-Z0-9_]+)\s*=", line_str)
            if adhoc_match:
                slot_name = adhoc_match.group(1)
                if slot_name not in ALLOWED_TOPLEVEL_GAME_ASSIGNMENTS:
                    errors.append(
                        f"{rel}:{line_no}: unauthorized ad hoc game facade assignment 'game.{slot_name} = ...'. "
                        f"Only {ALLOWED_TOPLEVEL_GAME_ASSIGNMENTS} may be assigned outside {lifecycle_rel}."
                    )

    # Check that all create_registry factories in runtime are registered inside DESCRIPTOR of lifecycle_file
    lifecycle_file = scripts_root / lifecycle_rel
    desc_text = ""
    if lifecycle_file.exists():
        lifecycle_content = strip_lua_comments(lifecycle_file.read_text(encoding="utf-8"))
        desc_start = lifecycle_content.find("DESCRIPTOR = {")
        if desc_start == -1:
            desc_start = lifecycle_content.find("local DESCRIPTOR = {")
        desc_end = lifecycle_content.find("M.descriptor = DESCRIPTOR", desc_start)
        if desc_start != -1 and desc_end != -1:
            desc_text = lifecycle_content[desc_start:desc_end]
        elif desc_start != -1:
            desc_text = lifecycle_content[desc_start:]

    runtime_dir = scripts_root / "runtime"
    if runtime_dir.exists():
        for lua_path in sorted(runtime_dir.rglob("*.lua")):
            content = strip_lua_comments(lua_path.read_text(encoding="utf-8"))
            if CREATE_REGISTRY_PATTERN.search(content):
                module_name = lua_path.stem
                # Must be registered in DESCRIPTOR
                if module_name not in desc_text:
                    errors.append(
                        f"{lua_path.relative_to(scripts_root)} defines create_registry() factory "
                        f"but is not registered in DESCRIPTOR of {lifecycle_rel}."
                    )

    return errors


def validate_package_ownership(gamedata_root: Path) -> list[str]:
    errors: list[str] = []
    for lua_path in sorted(gamedata_root.rglob("*.lua")):
        rel = lua_path.relative_to(gamedata_root)
        raw_content = lua_path.read_text(encoding="utf-8")
        clean_content = strip_lua_comments(raw_content)

        for line_no, line in enumerate(clean_content.splitlines(), start=1):
            line_str = line.strip()
            if not line_str:
                continue

            for pattern in FORBIDDEN_REGISTRY_SLOT_ASSIGNMENTS:
                if pattern.search(line_str):
                    errors.append(
                        f"GameData/{rel}:{line_no}: package unauthorized registry slot assignment: '{line_str}'. "
                        f"Packages are strictly forbidden from modifying or rawsetting registry facade slots."
                    )
                    break

            adhoc_match = re.search(r"\bgame\.([a-zA-Z0-9_]+)\s*=", line_str)
            if adhoc_match:
                slot_name = adhoc_match.group(1)
                if slot_name not in ALLOWED_TOPLEVEL_GAME_ASSIGNMENTS:
                    errors.append(
                        f"GameData/{rel}:{line_no}: package unauthorized ad hoc game facade assignment 'game.{slot_name} = ...'. "
                        f"Packages are strictly forbidden from mutating the game facade."
                    )
    return errors


def validate_repository(scripts_root: Path | None = None, gamedata_root: Path | None = None) -> list[str]:
    root = scripts_root if scripts_root is not None else SCRIPTS_ROOT
    lifecycle_file = root / "bootstrap" / "registry_lifecycle.lua"
    desc_errors, desc_entries = validate_descriptor(lifecycle_file)
    errors = list(desc_errors)
    errors.extend(validate_install_against_descriptor(lifecycle_file, desc_entries))
    errors.extend(validate_scripts_ownership(root))

    data_root = gamedata_root if gamedata_root is not None else GAMEDATA_ROOT
    if data_root.exists():
        errors.extend(validate_package_ownership(data_root))
    errors.extend(validate_native_seal_result())
    return errors


def run_self_test() -> bool:
    print("[*] Running validate_registry_lifecycle_ownership self-test...")

    clean_errors = validate_repository()
    if clean_errors:
        print("FAILED: Clean repository has registry lifecycle errors:")
        for err in clean_errors:
            print(f"  {err}")
        return False

    runtime_session_source = RUNTIME_SESSION_PATH.read_text(encoding="utf-8")
    ignored_seal_result = runtime_session_source.replace(
        "if (!SealRegistries(OutFault))",
        "SealRegistries(OutFault);\n        if (false)",
        1,
    )
    seal_errors = validate_native_seal_result_source(
        ignored_seal_result,
        "Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp",
    )
    if not any("return value must be consumed" in error for error in seal_errors):
        print("FAILED Native Negative 1: failed to catch ignored SealRegistries result")
        return False

    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_scripts = Path(tmpdir) / "Scripts"
        import shutil
        shutil.copytree(SCRIPTS_ROOT, tmp_scripts)

        lifecycle_file = tmp_scripts / "bootstrap" / "registry_lifecycle.lua"
        orig_lifecycle_text = lifecycle_file.read_text(encoding="utf-8")

        # Negative 1: Unauthorized registry slot assignment outside lifecycle
        test_file = tmp_scripts / "runtime" / "synthetic_publisher.lua"
        test_file.write_text("local M = {}\nfunction M.register() game.services = {} end\nreturn M\n")
        errs = validate_repository(tmp_scripts)
        if not any("unauthorized registry slot assignment" in e for e in errs):
            print("FAILED Negative 1: failed to catch unauthorized registry slot assignment")
            return False
        test_file.unlink()

        # Negative 2: Ad-hoc game slot assignment
        test_file.write_text("local M = {}\nfunction M.register() game.unauthorized_slot = {} end\nreturn M\n")
        errs = validate_repository(tmp_scripts)
        if not any("unauthorized ad hoc game facade assignment" in e for e in errs):
            print("FAILED Negative 2: failed to catch ad hoc game slot assignment")
            return False
        test_file.unlink()

        # Negative 3: Dynamic slot assignment
        test_file.write_text("local M = {}\nfunction M.register() game['services'] = {} end\nreturn M\n")
        errs = validate_repository(tmp_scripts)
        if not any("unauthorized registry slot assignment" in e for e in errs):
            print("FAILED Negative 3: failed to catch dynamic slot assignment")
            return False
        test_file.unlink()

        # Negative 4: Unregistered create_registry factory in runtime/
        test_file.write_text("local M = {}\nfunction M.create_registry() return {} end\nreturn M\n")
        errs = validate_repository(tmp_scripts)
        if not any("defines create_registry() factory but is not registered in DESCRIPTOR" in e for e in errs):
            print("FAILED Negative 4: failed to catch unreferenced registry factory")
            return False
        test_file.unlink()

        # Negative 5: Wrong DESCRIPTOR order (swap services and actions)
        swapped_text = orig_lifecycle_text.replace(
            'facade_path = "services"', 'facade_path = "TEMP_SWAP"'
        ).replace(
            'facade_path = "actions"', 'facade_path = "services"'
        ).replace(
            'facade_path = "TEMP_SWAP"', 'facade_path = "actions"'
        )
        lifecycle_file.write_text(swapped_text, encoding="utf-8")
        errs = validate_repository(tmp_scripts)
        if not any("DESCRIPTOR order mismatch" in e for e in errs):
            print("FAILED Negative 5: failed to catch DESCRIPTOR order mismatch")
            return False
        lifecycle_file.write_text(orig_lifecycle_text, encoding="utf-8")

        # Negative 6: Missing participant in DESCRIPTOR
        missing_text = orig_lifecycle_text.replace('facade_path = "presentation"', 'facade_path = "invalid_path"')
        lifecycle_file.write_text(missing_text, encoding="utf-8")
        errs = validate_repository(tmp_scripts)
        if not any("DESCRIPTOR order mismatch" in e for e in errs):
            print("FAILED Negative 6: failed to catch missing participant in DESCRIPTOR")
            return False
        lifecycle_file.write_text(orig_lifecycle_text, encoding="utf-8")

        # Negative 7: Missing is_frozen predicate
        missing_frozen = orig_lifecycle_text.replace('is_frozen = function(reg)', '-- omitted is_frozen')
        lifecycle_file.write_text(missing_frozen, encoding="utf-8")
        errs = validate_repository(tmp_scripts)
        if not any("missing is_frozen predicate" in e for e in errs):
            print("FAILED Negative 7: failed to catch missing is_frozen predicate")
            return False
        lifecycle_file.write_text(orig_lifecycle_text, encoding="utf-8")

        # Negative 8: Registry slot assigned in install() but missing from DESCRIPTOR
        bad_install = orig_lifecycle_text.replace(
            "function M.install()",
            'function M.install()\n    game_ref.rogue_unsealed_registry = {}\n',
        )
        lifecycle_file.write_text(bad_install, encoding="utf-8")
        errs = validate_repository(tmp_scripts)
        if not any("is installed in install() but missing from DESCRIPTOR" in e for e in errs):
            print("FAILED Negative 8: failed to catch unsealed registry slot installed in install()")
            return False
        lifecycle_file.write_text(orig_lifecycle_text, encoding="utf-8")

        # Negative 9: Factory required in lifecycle file header but omitted from DESCRIPTOR (the B2 loophole)
        rogue_factory = tmp_scripts / "runtime" / "rogue_subsystem_registry.lua"
        rogue_factory.write_text("local M = {}\nfunction M.create_registry() return {} end\nreturn M\n")
        header_require = 'local rogue = require("runtime.rogue_subsystem_registry")\n' + orig_lifecycle_text
        lifecycle_file.write_text(header_require, encoding="utf-8")
        errs = validate_repository(tmp_scripts)
        if not any("defines create_registry() factory but is not registered in DESCRIPTOR" in e for e in errs):
            print("FAILED Negative 9: failed to catch factory required at top of file but omitted from DESCRIPTOR")
            return False
        lifecycle_file.write_text(orig_lifecycle_text, encoding="utf-8")
        rogue_factory.unlink()

        # Negative 10: Participant in DESCRIPTOR with facade slot missing create factory
        missing_create = orig_lifecycle_text.replace(
            'create = function()\n            return service_registry.create_registry()\n        end,',
            '-- omitted create factory',
        )
        lifecycle_file.write_text(missing_create, encoding="utf-8")
        errs = validate_repository(tmp_scripts)
        if not any("missing create factory in DESCRIPTOR" in e for e in errs):
            print("FAILED Negative 10: failed to catch missing create factory in DESCRIPTOR")
            return False
        lifecycle_file.write_text(orig_lifecycle_text, encoding="utf-8")

        # Negative 11: rawset(game, ...) in a runtime script outside lifecycle
        test_file = tmp_scripts / "runtime" / "rogue_rawset_service.lua"
        test_file.write_text('local M = {}\nfunction M.hack() rawset(game, "services", {}) end\nreturn M\n')
        errs = validate_repository(tmp_scripts)
        if not any("unauthorized registry slot assignment" in e for e in errs):
            print("FAILED Negative 11: failed to catch rawset(game, ...) outside lifecycle")
            return False
        test_file.unlink()

        # Negative 12: rawset(game.commands, ...) in a runtime script outside lifecycle
        test_file.write_text('local M = {}\nfunction M.hack() rawset(game.commands, "handlers", {}) end\nreturn M\n')
        errs = validate_repository(tmp_scripts)
        if not any("unauthorized registry slot assignment" in e for e in errs):
            print("FAILED Negative 12: failed to catch rawset(game.commands, ...) outside lifecycle")
            return False
        test_file.unlink()

        # Negative 13: rawset(_G.game, ...) in a runtime script outside lifecycle
        test_file.write_text('local M = {}\nfunction M.hack() rawset(_G.game, "actions", {}) end\nreturn M\n')
        errs = validate_repository(tmp_scripts)
        if not any("unauthorized registry slot assignment" in e for e in errs):
            print("FAILED Negative 13: failed to catch rawset(_G.game, ...) outside lifecycle")
            return False
        test_file.unlink()

        # Negative 14: rawset in a GameData package script
        tmp_gamedata = Path(tmpdir) / "GameData"
        (tmp_gamedata / "sample_pkg" / "scripts").mkdir(parents=True, exist_ok=True)
        pkg_file = tmp_gamedata / "sample_pkg" / "scripts" / "rogue.lua"
        pkg_file.write_text('local M = {}\nfunction M.hack() rawset(game, "services", {}) end\nreturn M\n')
        errs = validate_repository(tmp_scripts, tmp_gamedata)
        if not any("package unauthorized registry slot assignment" in e for e in errs):
            print("FAILED Negative 14: failed to catch rawset in package script")
            return False

        # Negative 15: dot-assignment in a GameData package script
        pkg_file.write_text('local M = {}\nfunction M.hack() game.services = {} end\nreturn M\n')
        errs = validate_repository(tmp_scripts, tmp_gamedata)
        if not any("package unauthorized registry slot assignment" in e for e in errs):
            print("FAILED Negative 15: failed to catch game.services assignment in package script")
            return False
        pkg_file.unlink()

    print("[+] All negative mutations caught successfully. Self-test passed.")
    return True


def main() -> int:
    if "--self-test" in sys.argv:
        return 0 if run_self_test() else 1

    errors = validate_repository()
    if errors:
        print("validate_registry_lifecycle_ownership: FAILED with errors:", file=sys.stderr)
        for e in errors:
            print(f"  {e}", file=sys.stderr)
        return 1

    print("validate_registry_lifecycle_ownership: PASSED (clean ownership and contract sequence).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
