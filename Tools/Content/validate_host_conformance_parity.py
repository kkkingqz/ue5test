#!/usr/bin/env python3
"""Validates that no host-local duplicate conformance/self-test functions exist
and that all portable conformance entry points are invoked by both hosts (Headless and UE).
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SOURCE_DIR = REPO_ROOT / "Source"
HEADLESS_DIR = REPO_ROOT / "Headless"
UE_TESTS_DIR = SOURCE_DIR / "GV2" / "Private" / "Tests"


def find_conformance_entry_points() -> list[str]:
    """Finds all Run*Conformance functions declared in portable testing headers."""
    entry_points: list[str] = []
    pattern = re.compile(r"\bRun([A-Za-z0-9_]+Conformance)\s*\(")
    for header in SOURCE_DIR.glob("*/Public/*/Testing/*.h"):
        content = header.read_text(encoding="utf-8")
        for match in pattern.finditer(content):
            fn_name = "Run" + match.group(1)
            if fn_name not in entry_points:
                entry_points.append(fn_name)
    return sorted(entry_points)


def validate_headless(entry_points: list[str]) -> list[str]:
    """Ensures Headless does not define local self-tests and calls all conformance entry points."""
    errors: list[str] = []
    main_cpp = HEADLESS_DIR / "Source" / "main.cpp"
    if not main_cpp.exists():
        errors.append(f"Missing {main_cpp}")
        return errors

    content = main_cpp.read_text(encoding="utf-8")

    # Forbid host-local self-test definitions (e.g. RunContentCore*SelfTest)
    forbidden_pattern = re.compile(r"\bbool\s+Run[A-Za-z0-9_]*SelfTest\s*\(")
    for match in forbidden_pattern.finditer(content):
        errors.append(
            f"{main_cpp}: Found forbidden host-local self-test function '{match.group(0).strip()}'. "
            "All conformance suites must be defined as portable entry points in Source/*/Public/*/Testing/."
        )

    # Ensure all portable conformance entry points are invoked
    for ep in entry_points:
        if ep not in content:
            errors.append(
                f"{main_cpp}: Missing call to portable conformance entry point '{ep}'."
            )

    return errors


def validate_ue_tests(entry_points: list[str]) -> list[str]:
    """Ensures UE automation tests invoke all portable conformance entry points."""
    errors: list[str] = []
    if not UE_TESTS_DIR.exists():
        errors.append(f"Missing UE tests directory: {UE_TESTS_DIR}")
        return errors

    all_ue_test_content = ""
    for test_file in UE_TESTS_DIR.glob("*.cpp"):
        all_ue_test_content += test_file.read_text(encoding="utf-8") + "\n"

    for ep in entry_points:
        if ep not in all_ue_test_content:
            errors.append(
                f"Source/GV2/Private/Tests: Missing UE Automation Test wrapper for '{ep}'."
            )

    return errors


def is_test_file(path: Path) -> bool:
    """Identifies test/conformance fixtures where embedded Lua test sources are allowed."""
    if "Conformance" in path.name:
        return True
    if "Testing" in path.parts or "Tests" in path.parts:
        return True
    if "ThirdParty" in path.parts:
        return True
    if path.name in ("RepresentativeCore.cpp", "Json5Conformance.cpp"):
        return True
    return False


RAW_STRING_PATTERN = re.compile(r'R\"([a-zA-Z0-9_]*)\((.*?)\)\1\"', re.DOTALL)
LUA_CONSTRUCT_PATTERN = re.compile(r'(?:\blocal\s+[a-zA-Z_]|\bfunction\s*[a-zA-Z0-9_.]*\s*\(|\bend\b|\bif\s+.*?\bthen\b|\bvalidate_state|\bcanonical_sections)', re.DOTALL)
LUA_INLINE_STATEMENT_PATTERN = re.compile(r'\"([^\"\\\n]|\\.)*(?:\blocal\s+[a-zA-Z_0-9]+|\bfunction\s*[a-zA-Z0-9_.]*\s*\([^)]*\)|\bfor\s+[a-zA-Z_0-9,\s]+\s+in\s+pairs\b)(?:[^\"\\\n]|\\.)*\"')

# TAS-14: ADR-0024 forbids a NEW C++ conformance entry point from checking
# a rule expressed in Lua — that belongs to a Tests/Lua/ spec instead. Two
# explicit, reviewed categories may still embed Lua inside a *Conformance.cpp
# file; anything else embedding Lua there fails the gate. Paths are relative
# to Source/.
LEGACY_LUA_RULE_CONFORMANCE_FILES = {
    # Pre-ADR-0024: test a Lua gameplay rule via a verbatim embedded copy of
    # the real Scripts/ module(s) they exercise. Grandfathered, not
    # expanded — migrate to Tests/Lua/ specs when next touched (TAS-12/13
    # migrated the world/domain-object and command-validator suites this
    # way already; these two remain).
    "GV2RuntimeCore/Private/GV2LuaRepositoryConformance.cpp",
    "GV2RuntimeCore/Private/GV2ValidatorRegistryConformance.cpp",
}
MECHANISM_LUA_FIXTURE_CONFORMANCE_FILES = {
    # Tests a C++ mechanism (the Lua spec runner itself, TAS-02), not a
    # gameplay rule: its embedded Lua is synthetic fixture data with no
    # counterpart under Scripts/, not a duplicated production module.
    "GV2RuntimeCore/Private/GV2LuaSpecRunnerConformance.cpp",
    # Tests ReplayRunManifest() determinism/fault mechanics. Its embedded
    # Lua is a trivial synthetic stub (dispatch_command that only echoes
    # request.sequence) with no counterpart under Scripts/, not a gameplay
    # rule.
    "GV2RuntimeCore/Private/GV2RunReplayConformance.cpp",
    # Tests FRuntimeSession::StartFromSave's C++ orchestration (SAV-12/17,
    # plan SaveAndLoad): tree from decode_and_prepare instead of defaults,
    # restore_instances firing, state assigned only after every stage
    # succeeds, slot-read-before-VM-creation on failure. Its embedded
    # save/load/state_validator modules are deliberately trivial synthetic
    # stubs, not copies of the real Scripts/ rule logic — the real
    # preflight/integrity/redirect/safe-point rules are covered by
    # Tests/Lua/save/{canonical_codec,save_path,load_path}.lua instead.
    "GV2RuntimeCore/Private/GV2ColdStartLoadConformance.cpp",
}


def validate_no_embedded_lua_in_production() -> list[str]:
    """Ensures production C++ files do not contain embedded Lua code, chunks, or gameplay rules."""
    errors: list[str] = []
    for path in sorted(SOURCE_DIR.rglob("*")):
        if not path.is_file() or path.suffix not in (".cpp", ".h"):
            continue
        if is_test_file(path):
            continue

        content = path.read_text(encoding="utf-8")

        for match in RAW_STRING_PATTERN.finditer(content):
            delim = match.group(1)
            raw_body = match.group(2)
            if delim == "lua" or LUA_CONSTRUCT_PATTERN.search(raw_body):
                snippet = match.group(0).replace("\n", " ")[:80]
                errors.append(
                    f"{path}: Found forbidden embedded Lua raw string literal: {snippet}... "
                    "(All gameplay logic and state validation rules must reside in Scripts/ and not in production C++)."
                )

        for match in LUA_INLINE_STATEMENT_PATTERN.finditer(content):
            snippet = match.group(0).replace("\n", " ")[:80]
            errors.append(
                f"{path}: Found forbidden embedded Lua statement literal: {snippet}... "
                "(All gameplay logic and state validation rules must reside in Scripts/ and not in production C++)."
            )

    return errors


def validate_no_new_lua_rule_conformance() -> list[str]:
    """TAS-14: a new C++ conformance entry point checking a Lua gameplay
    rule via embedded Lua source is forbidden (ADR-0024). Existing files
    are grandfathered explicitly by relative path; a C++ mechanism test
    that legitimately needs synthetic Lua fixture data is allowed only via
    the same explicit, reviewed opt-in. Anything else matching
    *Conformance.cpp that embeds Lua fails."""
    errors: list[str] = []
    allowed = LEGACY_LUA_RULE_CONFORMANCE_FILES | MECHANISM_LUA_FIXTURE_CONFORMANCE_FILES
    for path in sorted(SOURCE_DIR.rglob("*Conformance.cpp")):
        rel = path.relative_to(SOURCE_DIR).as_posix()
        if rel in allowed:
            continue

        content = path.read_text(encoding="utf-8")
        found = False
        for match in RAW_STRING_PATTERN.finditer(content):
            delim = match.group(1)
            raw_body = match.group(2)
            if delim == "lua" or LUA_CONSTRUCT_PATTERN.search(raw_body):
                snippet = match.group(0).replace("\n", " ")[:80]
                errors.append(
                    f"{path}: new C++ conformance entry point embeds Lua source: {snippet}... "
                    "A rule expressed in Lua must be checked by a Tests/Lua/ spec (ADR-0024), not a "
                    "new C++ conformance entry point. If this genuinely tests a C++ mechanism (not a "
                    "gameplay rule), add it to MECHANISM_LUA_FIXTURE_CONFORMANCE_FILES explicitly in "
                    "validate_host_conformance_parity.py."
                )
                found = True
                break
        if found:
            continue
        for match in LUA_INLINE_STATEMENT_PATTERN.finditer(content):
            snippet = match.group(0).replace("\n", " ")[:80]
            errors.append(
                f"{path}: new C++ conformance entry point embeds a Lua statement literal: {snippet}... "
                "A rule expressed in Lua must be checked by a Tests/Lua/ spec (ADR-0024), not a new "
                "C++ conformance entry point."
            )
            break

    return errors


def main() -> int:
    entry_points = find_conformance_entry_points()
    if not entry_points:
        print("ERROR: No portable conformance entry points found in Source/*/Public/*/Testing/*.h", file=sys.stderr)
        return 1

    errors: list[str] = []
    errors.extend(validate_headless(entry_points))
    errors.extend(validate_ue_tests(entry_points))
    errors.extend(validate_no_embedded_lua_in_production())
    errors.extend(validate_no_new_lua_rule_conformance())

    if errors:
        print(f"FAILED: Host conformance parity validation failed with {len(errors)} error(s):", file=sys.stderr)
        for err in errors:
            print(f"  - {err}", file=sys.stderr)
        return 1

    print(f"SUCCESS: Validated {len(entry_points)} conformance entry points across Headless and UE hosts without host-local duplication and with zero embedded Lua in production C++.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
