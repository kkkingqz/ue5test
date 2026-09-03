#!/usr/bin/env python3
"""Checks that production Source/GV2 code never hardcodes a '/Game/' asset path.

DCA-03: a class element hardcoded behind LoadClass/FindObject with a fixed path is a
hidden, backwards dependency of code on content layout -- it survives a rename of the
asset silently and wrong. The class element belongs on the declaring asset itself
(Designer), never in C++. This scans Source/, not a fixed list of files, so a future
resurrection of the pattern is caught regardless of which file it lands in.

Tests and fixtures are deliberately out of scope: a test that loads a real asset by path
to exercise production code against it is not the dependency this gate forbids.
"""

from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SOURCE_ROOTS = [
    REPO_ROOT / "Source" / "GV2" / "Public",
    REPO_ROOT / "Source" / "GV2" / "Private",
]
EXCLUDED_DIR_NAME = "Tests"
SCANNED_SUFFIXES = (".h", ".cpp")
GAME_PATH_LITERAL = re.compile(r'"[^"]*/Game/[^"]*"')


def strip_comments(source: str) -> str:
    """Preserve layout while excluding braces and semicolons in comments."""

    source = re.sub(r"/\*.*?\*/", lambda match: "\n" * match.group(0).count("\n"), source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", source)


def find_violations(root: Path) -> list[str]:
    if not root.exists():
        return [f"{root}: source root not found"]

    violations: list[str] = []
    for path in sorted(root.rglob("*")):
        if not path.is_file() or path.suffix not in SCANNED_SUFFIXES:
            continue
        if EXCLUDED_DIR_NAME in path.relative_to(root).parts:
            continue

        text = strip_comments(path.read_text(encoding="utf-8"))
        for line_no, line in enumerate(text.splitlines(), start=1):
            match = GAME_PATH_LITERAL.search(line)
            if match:
                violations.append(
                    f"{path}:{line_no}: hardcoded '/Game/' asset path literal in production code: {match.group(0)}"
                )
    return violations


def validate_repository() -> list[str]:
    violations: list[str] = []
    for root in SOURCE_ROOTS:
        violations.extend(find_violations(root))
    return violations


def run_self_test() -> bool:
    print("[*] Running validate_no_hardcoded_asset_paths self-test...")
    actual = validate_repository()
    if actual:
        print(
            "FAILED: current repository already contains hardcoded '/Game/' literals "
            "in production code:\n" + "\n".join(actual)
        )
        return False

    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_root = Path(tmpdir)

        production_dir = tmp_root / "Source" / "GV2" / "Public"
        production_dir.mkdir(parents=True)
        (production_dir / "Synthetic.h").write_text(
            "TSubclassOf<UUserWidget> ResolveSynthetic() const\n"
            "{\n"
            '    return LoadClass<UUserWidget>(nullptr, TEXT("/Game/UI/Widgets/WBP_Synthetic.WBP_Synthetic_C"));\n'
            "}\n",
            encoding="utf-8",
        )

        private_root = tmp_root / "Source" / "GV2" / "Private"
        test_dir = private_root / "Tests"
        test_dir.mkdir(parents=True)
        (test_dir / "SyntheticTest.cpp").write_text(
            'UClass* Class = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UI/Widgets/WBP_Synthetic.WBP_Synthetic_C"));\n',
            encoding="utf-8",
        )

        found_production = find_violations(production_dir)
        if not any("Synthetic.h" in violation for violation in found_production):
            print(f"FAILED: gate did not flag synthetic '/Game/' literal in production header: {found_production}")
            return False

        found_private = find_violations(private_root)
        if any("SyntheticTest.cpp" in violation for violation in found_private):
            print(f"FAILED: gate incorrectly flagged a '/Game/' literal inside Tests/ as production code: {found_private}")
            return False

    print("SUCCESS: gate flags hardcoded '/Game/' literals in production code and excludes Tests/")
    return True


def main(argv: list[str]) -> int:
    if argv == ["--self-test"]:
        return 0 if run_self_test() else 1
    if argv:
        print("usage: validate_no_hardcoded_asset_paths.py [--self-test]", file=sys.stderr)
        return 2

    violations = validate_repository()
    if violations:
        print("\n".join(violations), file=sys.stderr)
        return 1
    print("SUCCESS: no hardcoded '/Game/' asset path literals found in production Source/GV2 code")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
