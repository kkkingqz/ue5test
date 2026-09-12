#!/usr/bin/env python3
"""Static ownership gate for registry lifecycle and sealing (CFC-05, plan SessionLifecycle, M1).

Validates that:
1. Scripts/bootstrap/registry_lifecycle.lua exclusively owns the closed DESCRIPTOR of all engine registries.
2. The DESCRIPTOR order strictly matches the contract sequence per RuntimeFacadeAndRegistries.md.
3. Every DESCRIPTOR participant defines seal and is_frozen.
4. No file outside registry_lifecycle.lua publishes or overwrites registry facade slots or creates ad hoc slots on game.
5. All registry factories (create_registry) in Scripts/runtime/ are registered in registry_lifecycle.lua.
"""

from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SCRIPTS_ROOT = REPO_ROOT / "Scripts"

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


def validate_descriptor(lifecycle_file: Path) -> list[str]:
    errors: list[str] = []
    if not lifecycle_file.exists():
        return [f"Missing required registry lifecycle file: {lifecycle_file}"]

    content = lifecycle_file.read_text(encoding="utf-8")
    stripped = strip_lua_comments(content)

    # Extract DESCRIPTOR block
    desc_start = stripped.find("DESCRIPTOR = {")
    if desc_start == -1:
        desc_start = stripped.find("local DESCRIPTOR = {")
    if desc_start == -1:
        return [f"{lifecycle_file}: could not find DESCRIPTOR table definition"]

    # Match all facade_path entries in DESCRIPTOR
    desc_entries = DESCRIPTOR_ENTRY_PATTERN.findall(stripped[desc_start:])
    if not desc_entries:
        return [f"{lifecycle_file}: no facade_path entries found in DESCRIPTOR"]

    if desc_entries != CONTRACT_SEQUENCE:
        errors.append(
            f"{lifecycle_file}: DESCRIPTOR order mismatch!\n"
            f"  Expected: {CONTRACT_SEQUENCE}\n"
            f"  Actual:   {desc_entries}"
        )

    # Check that each entry block contains seal and is_frozen
    # Split by entries
    entry_chunks = stripped[desc_start:].split("facade_path = ")
    for chunk in entry_chunks[1:]:
        # Get path name
        m_name = re.match(r'"([^"]+)"', chunk.strip())
        path_name = m_name.group(1) if m_name else "unknown"
        if "seal =" not in chunk and "seal=" not in chunk:
            errors.append(f"{lifecycle_file}: participant '{path_name}' missing seal method in DESCRIPTOR")
        if "is_frozen =" not in chunk and "is_frozen=" not in chunk:
            errors.append(f"{lifecycle_file}: participant '{path_name}' missing is_frozen predicate in DESCRIPTOR")

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

    # Check that all create_registry factories in runtime are accounted for in lifecycle_file
    lifecycle_file = scripts_root / lifecycle_rel
    lifecycle_text = lifecycle_file.read_text(encoding="utf-8") if lifecycle_file.exists() else ""

    runtime_dir = scripts_root / "runtime"
    if runtime_dir.exists():
        for lua_path in sorted(runtime_dir.rglob("*.lua")):
            content = strip_lua_comments(lua_path.read_text(encoding="utf-8"))
            if CREATE_REGISTRY_PATTERN.search(content):
                module_name = lua_path.stem
                # Must be required in registry_lifecycle
                if module_name not in lifecycle_text:
                    errors.append(
                        f"{lua_path.relative_to(scripts_root)} defines create_registry() factory "
                        f"but is not referenced in {lifecycle_rel}."
                    )

    return errors


def validate_repository(scripts_root: Path | None = None) -> list[str]:
    root = scripts_root if scripts_root is not None else SCRIPTS_ROOT
    lifecycle_file = root / "bootstrap" / "registry_lifecycle.lua"
    errors = validate_descriptor(lifecycle_file)
    errors.extend(validate_scripts_ownership(root))
    return errors


def run_self_test() -> bool:
    print("[*] Running validate_registry_lifecycle_ownership self-test...")

    clean_errors = validate_repository()
    if clean_errors:
        print("FAILED: Clean repository has registry lifecycle errors:")
        for err in clean_errors:
            print(f"  {err}")
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
        if not any("defines create_registry() factory but is not referenced" in e for e in errs):
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
