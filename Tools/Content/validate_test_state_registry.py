#!/usr/bin/env python3
"""Validates that every mutable module-level Lua local in an infrastructure
module is registered with core:module.runtime.test_state_registry.

Rule ID: TEST_STATE_REGISTRY_RULE (GAP-003, EngineGapClosure EGC-04)

Scripts/runtime/test_state_registry.lua is where infrastructure modules
declare their own cross-spec-case transient state (snapshot/restore/reset),
replacing a hand-written enumeration that lived in
Scripts/authoring/context.lua and silently dropped a fifth variable added
anywhere. This script is the enumerator half of that decision: it statically
derives each checked module's actual top-level `local` declarations that are
later reassigned (i.e. genuinely mutable state, not a `require()`d dependency
or a write-once constant) and fails if any of them is not referenced inside a
`test_state_registry.register_participant(...)` call in the same file.

A true runtime enumerator (walking each module's live upvalues) is not
possible here: the Lua sandbox this project's modules execute in
(Source/GV2RuntimeCore/.../OpenEnvironment) opens only base/table/string/
math/utf8 -- no `debug`, no `io`, no `load` -- so no module, including a test
helper, can inspect another module's upvalues at runtime. This script reads
the same .lua source files statically instead, from outside that sandbox.

CHECKED_MODULES is a list of files, not of variable names: a file's own
locals are discovered from its text, so a new transient variable added to an
already-listed file is caught without editing this script. Only adding a
*new infrastructure module* with isolable state requires appending its path
here -- expected, since nothing scans the whole repository for "any Lua file
with a mutable local" (that would flag ordinary non-infrastructure helper
state that nothing claims needs cross-case isolation).
"""

from __future__ import annotations

import argparse
import re
import sys
import tempfile
from pathlib import Path
from typing import NamedTuple, Optional

REPO_ROOT = Path(__file__).resolve().parent.parent.parent

CHECKED_MODULES = [
    Path("Scripts") / "authoring" / "context.lua",
]

REGISTER_CALL_RE = re.compile(r"test_state_registry\s*\.\s*register_participant\s*\(")
TOP_LEVEL_LOCAL_RE = re.compile(r"^local\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*)$")
REASSIGNMENT_RE_TEMPLATE = r"(?<![.\w])(?<!local\s){name}\s*=(?!=)"


def strip_lua_comments(source: str) -> str:
    """Preserve layout and line count while stripping comments (mirrors
    validate_core_decoupling.strip_code_comments for .lua)."""
    source = re.sub(r"--\[\[.*?\]\]", lambda m: "\n" * m.group(0).count("\n"), source, flags=re.DOTALL)
    return re.sub(r"--[^\n]*", "", source)


def extract_balanced_call_bodies(text: str, call_re: re.Pattern) -> list[str]:
    """For every match of call_re (which must end just after an opening '('),
    return the substring up to the matching closing ')'."""
    bodies = []
    for m in call_re.finditer(text):
        depth = 1
        i = m.end()
        start = i
        while i < len(text) and depth > 0:
            if text[i] == "(":
                depth += 1
            elif text[i] == ")":
                depth -= 1
            i += 1
        bodies.append(text[start : i - 1])
    return bodies


def find_mutable_top_level_locals(source: str) -> list[str]:
    """Top-level (column 0) `local NAME = ...` declarations whose RHS is not a
    `require(...)` call or a function literal, and which are reassigned
    (`NAME = ...`, not `local NAME = ...`) somewhere else in the file -- the
    reassignment is what makes it transient state rather than a constant."""
    names: list[str] = []
    for line in source.splitlines():
        m = TOP_LEVEL_LOCAL_RE.match(line)
        if not m:
            continue
        name, rhs = m.group(1), m.group(2).strip()
        if rhs.startswith("require(") or rhs.startswith("function"):
            continue
        names.append(name)

    mutable = []
    for name in names:
        reassign_re = re.compile(REASSIGNMENT_RE_TEMPLATE.format(name=re.escape(name)))
        occurrences = len(reassign_re.findall(source))
        # The declaration itself (`local NAME = ...`) does not match
        # REASSIGNMENT_RE_TEMPLATE because of the negative lookbehind on
        # "local ", so any match here is a later reassignment.
        if occurrences > 0:
            mutable.append(name)
    return mutable


class ModuleResult(NamedTuple):
    path: Path
    unregistered: list[str]


def check_module(repo_root: Path, rel_path: Path) -> ModuleResult:
    full_path = repo_root / rel_path
    source = strip_lua_comments(full_path.read_text(encoding="utf-8"))

    mutable_locals = find_mutable_top_level_locals(source)
    register_bodies = extract_balanced_call_bodies(source, REGISTER_CALL_RE)

    unregistered = []
    for name in mutable_locals:
        word_re = re.compile(r"\b" + re.escape(name) + r"\b")
        covered = any(word_re.search(body) for body in register_bodies)
        if not covered:
            unregistered.append(name)
    return ModuleResult(rel_path, unregistered)


def run_validation(repo_root: Path, modules: Optional[list[Path]] = None) -> list[str]:
    violations = []
    for rel_path in modules if modules is not None else CHECKED_MODULES:
        result = check_module(repo_root, rel_path)
        for name in result.unregistered:
            violations.append(
                f"{result.path}: error [TEST_STATE_REGISTRY_RULE]: module-level local '{name}' is "
                f"reassigned (transient state) but not referenced in any test_state_registry.register_participant(...) "
                f"call in this file."
            )
    return violations


def run_self_test() -> bool:
    print("[*] Running validate_test_state_registry self-test...")

    actual_violations = run_validation(REPO_ROOT)
    if actual_violations:
        print("FAILED: expected the real codebase to be clean, found violations:\n" + "\n".join(actual_violations))
        return False
    print("  PASS: Actual authoring/context.lua is fully registered.")

    with tempfile.TemporaryDirectory() as tmpdir:
        fake_root = Path(tmpdir)
        fake_module = Path("probe.lua")
        full_path = fake_root / fake_module
        full_path.parent.mkdir(parents=True, exist_ok=True)

        # A mutable module-level local with no registration at all: must be flagged.
        full_path.write_text(
            "local leak_counter = 0\n"
            "\n"
            "local M = {}\n"
            "function M.bump()\n"
            "    leak_counter = leak_counter + 1\n"
            "    return leak_counter\n"
            "end\n"
            "return M\n",
            encoding="utf-8",
        )
        violations = run_validation(fake_root, [fake_module])
        if len(violations) != 1 or "leak_counter" not in violations[0]:
            print(f"FAILED: expected exactly one violation naming 'leak_counter', got: {violations}")
            return False
        print("  PASS: unregistered mutable local is flagged.")

        # The same local, now referenced inside a test_state_registry.register_participant(...) call: must clear.
        full_path.write_text(
            "local leak_counter = 0\n"
            "\n"
            "local test_state_registry = require(\"core:module.runtime.test_state_registry\")\n"
            "test_state_registry.register_participant(\"probe.leak_counter\", {\n"
            "    snapshot = function() return leak_counter end,\n"
            "    restore = function(v) leak_counter = v end,\n"
            "    reset = function() leak_counter = 0 end,\n"
            "})\n"
            "\n"
            "local M = {}\n"
            "function M.bump()\n"
            "    leak_counter = leak_counter + 1\n"
            "    return leak_counter\n"
            "end\n"
            "return M\n",
            encoding="utf-8",
        )
        violations_after = run_validation(fake_root, [fake_module])
        if violations_after:
            print(f"FAILED: expected 0 violations once registered, got: {violations_after}")
            return False
        print("  PASS: registered mutable local clears the gate.")

        # A require()'d dependency and a write-once constant must never be flagged,
        # even though neither is registered.
        full_path.write_text(
            "local mutation_window = require(\"core:module.runtime.mutation_window\")\n"
            "local SENTINEL = setmetatable({}, {})\n"
            "\n"
            "local M = {}\n"
            "function M.is_sentinel(v) return v == SENTINEL end\n"
            "return M\n",
            encoding="utf-8",
        )
        violations_const = run_validation(fake_root, [fake_module])
        if violations_const:
            print(f"FAILED: require()'d dependencies and write-once constants must not be flagged, got: {violations_const}")
            return False
        print("  PASS: require()'d dependencies and write-once constants are not flagged.")

    print("ALL TEST STATE REGISTRY SELF-TESTS PASSED SUCCESSFULLY!")
    return True


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="Validate that infrastructure modules register their transient state with test_state_registry."
    )
    parser.add_argument("--self-test", action="store_true", help="Run self-tests, including a negative fixture.")
    parser.add_argument("--repo-root", type=Path, default=REPO_ROOT, help="Override repository root path.")
    args = parser.parse_args(argv)

    if args.self_test:
        return 0 if run_self_test() else 1

    violations = run_validation(args.repo_root)
    if violations:
        print(f"Test state registry validation failed ({len(violations)} violations):", file=sys.stderr)
        for violation in violations:
            print(f"  {violation}", file=sys.stderr)
        return 1

    print(f"Test state registry validation passed: {len(CHECKED_MODULES)} infrastructure module(s) fully registered.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
