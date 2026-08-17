#!/usr/bin/env python3
"""Validates that engine/core code (Scripts/, GameData/core/, Source/)
contains no reverse dependencies / back-references to the game package 'rh:' namespace.
Rule ID: CORE_DECOUPLING_RULE
"""

from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
TARGET_DIRECTORIES = [
    REPO_ROOT / "Scripts",
    REPO_ROOT / "GameData" / "core",
    REPO_ROOT / "Source",
]

FORBIDDEN_PATTERN = re.compile(r"\b(rh|sample):")
CHECKED_EXTENSIONS = {
    ".lua",
    ".json5",
    ".json",
    ".po",
    ".pot",
    ".h",
    ".hpp",
    ".cpp",
    ".c",
    ".cs",
    ".py",
    ".txt",
    ".md",
}


def scan_directory(dir_path: Path) -> list[str]:
    violations: list[str] = []
    if not dir_path.exists():
        return violations

    for file_path in sorted(dir_path.rglob("*")):
        if not file_path.is_file():
            continue
        if file_path.suffix.lower() not in CHECKED_EXTENSIONS:
            continue

        try:
            content = file_path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue

        for line_num, line in enumerate(content.splitlines(), start=1):
            if FORBIDDEN_PATTERN.search(line):
                rel_path = file_path.relative_to(REPO_ROOT) if file_path.is_relative_to(REPO_ROOT) else file_path
                violations.append(
                    f"{rel_path}:{line_num}: error [CORE_DECOUPLING_RULE]: "
                    f"Engine/Core must not reference game package 'rh:' namespace. Found: {line.strip()}"
                )
    return violations


def run_validation(directories: list[Path]) -> list[str]:
    all_violations: list[str] = []
    for directory in directories:
        all_violations.extend(scan_directory(directory))
    return all_violations


def run_self_test() -> bool:
    print("[*] Running validate_core_decoupling self-test...")
    # 1. Positive test against actual codebase
    actual_violations = run_validation(TARGET_DIRECTORIES)
    if actual_violations:
        print(f"FAILED: expected clean codebase, found violations:\n" + "\n".join(actual_violations))
        return False

    # 2. Negative test against synthetic violation
    with tempfile.TemporaryDirectory() as tmpdir:
        fake_root = Path(tmpdir)
        fake_scripts = fake_root / "Scripts"
        fake_scripts.mkdir(parents=True)
        probe_file = fake_scripts / "probe.lua"
        probe_file.write_text('local target = "rh:location.city.market"\n', encoding="utf-8")

        fake_violations = scan_directory(fake_scripts)
        if not fake_violations:
            print("FAILED: scanner did not catch forbidden 'rh:' reference in probe file")
            return False

        if "CORE_DECOUPLING_RULE" not in fake_violations[0] or "probe.lua:1:" not in fake_violations[0]:
            print(f"FAILED: violation format mismatch: {fake_violations[0]}")
            return False

    print("  PASS: validate_core_decoupling self-test succeeded.")
    return True


def main() -> int:
    if "--self-test" in sys.argv:
        return 0 if run_self_test() else 1

    violations = run_validation(TARGET_DIRECTORIES)
    if violations:
        print(f"Core decoupling validation failed ({len(violations)} violations):", file=sys.stderr)
        for violation in violations:
            print(violation, file=sys.stderr)
        return 1

    print("Core decoupling validation passed: no 'rh:' references in Scripts/, GameData/core/, or Source/.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
