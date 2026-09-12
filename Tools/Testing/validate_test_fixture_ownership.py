#!/usr/bin/env python3
"""Checks that automation test fixtures maintain lifetime isolation and scoped ownership.

CFC-02A (ADR-0040, CFC-AF-02, CFC-AF-10):
Fixtures must never leave rooted GameInstance or UWorld objects, or un-restored global
test settings (such as EGV2ForgeryMode) behind for subsequent tests.
1. AddToRoot in test sources is restricted to approved scoped owners (such as
   FScopedTestWorldContext in GV2PresentationTestFixtures.h) and subsystem lifecycle
   test fixtures in GV2RuntimeSubsystemTests.cpp.
2. Direct mutation of global test settings (e.g., ModeForNextInstance() = ...) is forbidden;
   mutations must use RAII scoped helpers (FScopedForgeryMode).
"""

from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SOURCE_ROOT = REPO_ROOT / "Source"

SCANNED_SUFFIXES = (".h", ".cpp")

# Regex to detect AddToRoot calls
ADD_TO_ROOT_PATTERN = re.compile(r"\b(\w+)->AddToRoot\s*\(\s*\)")

# Regex to detect direct assignments to ModeForNextInstance
DIRECT_MODE_ASSIGNMENT_PATTERN = re.compile(
    r"\bModeForNextInstance\s*\([^)]*\)\s*="
)

# Regex to detect calls to SetModeForNextInstance
SET_MODE_PATTERN = re.compile(
    r"\bSetModeForNextInstance\s*\("
)

# Allowed files for AddToRoot in test sources
# Only FScopedTestWorldContext and GV2RuntimeSubsystemTests (subsystem lifecycle tests) are allowed.
ALLOWED_ADD_TO_ROOT_FILES = {
    "GV2PresentationTestFixtures.h",
    "GV2RuntimeSubsystemTests.cpp",
}

# Allowed files for SetModeForNextInstance
ALLOWED_SET_MODE_FILES = {
    "GV2ForgeryTestWidgets.h",
    "GV2ForgeryTestWidgets.cpp",
}


def strip_comments(source: str) -> str:
    """Preserve layout and line count while stripping comments."""
    source = re.sub(
        r"/\*.*?\*/",
        lambda match: "\n" * match.group(0).count("\n"),
        source,
        flags=re.DOTALL,
    )
    return re.sub(r"//[^\n]*", "", source)


def find_test_source_files(source_root: Path) -> list[Path]:
    """Find all C++ source and header files located within Tests directories."""
    test_files: list[Path] = []
    if not source_root.exists():
        return test_files

    for path in sorted(source_root.rglob("*")):
        if not path.is_file() or path.suffix not in SCANNED_SUFFIXES:
            continue
        # Check if "Tests" is in path components
        parts = path.relative_to(source_root).parts
        if "Tests" in parts:
            test_files.append(path)
    return test_files


def find_violations_in_file(path: Path) -> list[str]:
    violations: list[str] = []
    text = strip_comments(path.read_text(encoding="utf-8"))
    lines = text.splitlines()

    for line_no, line in enumerate(lines, start=1):
        # Check AddToRoot
        match_root = ADD_TO_ROOT_PATTERN.search(line)
        if match_root:
            if path.name not in ALLOWED_ADD_TO_ROOT_FILES:
                violations.append(
                    f"{path}:{line_no}: raw '{match_root.group(0)}' in test fixture is prohibited. "
                    "Use GV2PresentationTestFixtures::FScopedTestWorldContext for scoped lifetime."
                )
            elif path.name == "GV2PresentationTestFixtures.h":
                # Ensure it is inside FScopedTestWorldContext
                # Simple check: verify FScopedTestWorldContext appears in the file
                if "FScopedTestWorldContext" not in text:
                    violations.append(
                        f"{path}:{line_no}: AddToRoot in GV2PresentationTestFixtures.h outside FScopedTestWorldContext."
                    )

        # Check direct assignment to ModeForNextInstance
        if DIRECT_MODE_ASSIGNMENT_PATTERN.search(line):
            violations.append(
                f"{path}:{line_no}: direct assignment to ModeForNextInstance is prohibited. "
                "Use FScopedForgeryMode to guarantee RAII restoration of test settings."
            )

        # Check direct calls to SetModeForNextInstance
        if SET_MODE_PATTERN.search(line):
            if path.name not in ALLOWED_SET_MODE_FILES:
                violations.append(
                    f"{path}:{line_no}: direct call to SetModeForNextInstance is prohibited outside FScopedForgeryMode."
                )

    return violations


def validate_repository(source_root: Path | None = None) -> list[str]:
    root = source_root or SOURCE_ROOT
    violations: list[str] = []
    test_files = find_test_source_files(root)
    for test_file in test_files:
        violations.extend(find_violations_in_file(test_file))
    return violations


def run_self_test() -> bool:
    print("[*] Running validate_test_fixture_ownership self-test...")

    # 1. Repository check
    actual = validate_repository()
    if actual:
        print(
            "FAILED: current repository violates test fixture ownership contract:\n"
            + "\n".join(actual)
        )
        return False

    # 2. Synthetic negative & positive test fixtures
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_root = Path(tmpdir)
        test_dir = tmp_root / "Source" / "GV2" / "Private" / "Tests"
        test_dir.mkdir(parents=True)

        # Negative Fixture 1: raw AddToRoot in a UI test
        bad_ui_test = test_dir / "GV2BadUiTest.cpp"
        bad_ui_test.write_text(
            "void TestFn() {\n"
            "    UGameInstance* GI = NewObject<UGameInstance>();\n"
            "    GI->AddToRoot();\n"
            "}\n",
            encoding="utf-8",
        )
        v1 = validate_repository(tmp_root / "Source")
        if not any("GV2BadUiTest.cpp" in v and "raw 'GI->AddToRoot()' in test fixture is prohibited" in v for v in v1):
            print(f"FAILED: gate did not flag raw AddToRoot in test fixture: {v1}")
            return False
        bad_ui_test.unlink()

        # Negative Fixture 2: direct assignment to ModeForNextInstance
        bad_mode_test = test_dir / "GV2BadModeTest.cpp"
        bad_mode_test.write_text(
            "void TestMode() {\n"
            "    UGV2ForgeryEntryTestWidget::ModeForNextInstance() = EGV2ForgeryMode::DetachedRenderer;\n"
            "}\n",
            encoding="utf-8",
        )
        v2 = validate_repository(tmp_root / "Source")
        if not any("GV2BadModeTest.cpp" in v and "direct assignment to ModeForNextInstance is prohibited" in v for v in v2):
            print(f"FAILED: gate did not flag direct assignment to ModeForNextInstance: {v2}")
            return False
        bad_mode_test.unlink()

        # Negative Fixture 3: calling SetModeForNextInstance directly in a test
        bad_setter_test = test_dir / "GV2BadSetterTest.cpp"
        bad_setter_test.write_text(
            "void TestSetter() {\n"
            "    UGV2ForgeryEntryTestWidget::SetModeForNextInstance(EGV2ForgeryMode::DetachedRenderer);\n"
            "}\n",
            encoding="utf-8",
        )
        v3 = validate_repository(tmp_root / "Source")
        if not any("GV2BadSetterTest.cpp" in v and "direct call to SetModeForNextInstance is prohibited" in v for v in v3):
            print(f"FAILED: gate did not flag direct call to SetModeForNextInstance: {v3}")
            return False
        bad_setter_test.unlink()

        # Positive Fixture: proper scoped usage
        good_test = test_dir / "GV2GoodTest.cpp"
        good_test.write_text(
            "void TestGood() {\n"
            "    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;\n"
            "    FScopedForgeryMode ForgeryScope(EGV2ForgeryMode::NoOpConsumer);\n"
            "}\n",
            encoding="utf-8",
        )
        v_good = validate_repository(tmp_root / "Source")
        if v_good:
            print(f"FAILED: gate falsely flagged valid scoped fixture: {v_good}")
            return False
        good_test.unlink()

    print("SUCCESS: test fixture ownership validator correctly flags violations and accepts scoped owners.")
    return True


def main(argv: list[str]) -> int:
    if argv == ["--self-test"]:
        return 0 if run_self_test() else 1
    if argv:
        print("usage: validate_test_fixture_ownership.py [--self-test]", file=sys.stderr)
        return 2

    violations = validate_repository()
    if violations:
        print("\n".join(violations), file=sys.stderr)
        return 1
    print("SUCCESS: all test fixtures adhere to scoped ownership contract.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
