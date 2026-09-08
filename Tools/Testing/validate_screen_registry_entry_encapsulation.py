#!/usr/bin/env python3
"""Checks that FGV2ScreenRegistryEntry is never named in production code outside the
registry that owns it.

PAH-02 (ADR-0042, preamble): a permitted object must not be obtainable except through the
check that permits it. FGV2ScreenRegistryEntry is the raw, unvalidated authoring row --
Build()/Resolve() on UGV2ScreenRegistry are the only path that turns it into a resolved,
placement-checked FGV2ResolvedScreenDescriptor. Any production code naming the entry type
directly could read an unvalidated Layer/WidgetClass and bypass that check entirely. This
scans Source/GV2/{Public,Private}, not a fixed file list, so a future reintroduction of the
pattern is caught regardless of which file it lands in.

Tests are deliberately out of scope: a test constructing a raw entry to exercise Build()'s
own validation is not the bypass this gate forbids.

PSC-08 (ADR-0043 D1, PAH-R4): the primary guarantee that a resolved screen class cannot be
obtained without Resolve() now lives in the consumer's own control flow --
FGV2TabContainerTabsPropertyConsumer::Prepare has no generic-class fallback left to reach
for, structurally, not by convention. This gate stays as a SECOND, independent line of
defense against the specific bypass PAH-R4 named (naming the raw, unvalidated authoring
row directly, instead of going through Build()/Resolve()) -- it was never the primary
guarantee and does not become one; it is unaffected by, and does not supersede, PSC-08's
own fix.
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
OWNING_FILE_NAMES = {"GV2ScreenRegistry.h", "GV2ScreenRegistry.cpp"}
SCANNED_SUFFIXES = (".h", ".cpp")
ENTRY_TYPE_TOKEN = re.compile(r"\bFGV2ScreenRegistryEntry\b")


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
        if path.name in OWNING_FILE_NAMES:
            continue
        if EXCLUDED_DIR_NAME in path.relative_to(root).parts:
            continue

        text = strip_comments(path.read_text(encoding="utf-8"))
        for line_no, line in enumerate(text.splitlines(), start=1):
            match = ENTRY_TYPE_TOKEN.search(line)
            if match:
                violations.append(
                    f"{path}:{line_no}: FGV2ScreenRegistryEntry named outside the registry that "
                    "owns it -- resolve through UGV2ScreenRegistry::Build()/Resolve() instead"
                )
    return violations


def validate_repository() -> list[str]:
    violations: list[str] = []
    for root in SOURCE_ROOTS:
        violations.extend(find_violations(root))
    return violations


def run_self_test() -> bool:
    print("[*] Running validate_screen_registry_entry_encapsulation self-test...")
    actual = validate_repository()
    if actual:
        print(
            "FAILED: current repository already names FGV2ScreenRegistryEntry outside the "
            "registry in production code:\n" + "\n".join(actual)
        )
        return False

    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_root = Path(tmpdir)

        production_dir = tmp_root / "Source" / "GV2" / "Private" / "Runtime"
        production_dir.mkdir(parents=True)
        (production_dir / "Synthetic.cpp").write_text(
            "UClass* ResolveSynthetic(const FGV2ScreenRegistryEntry& Entry)\n"
            "{\n"
            "    return Entry.WidgetClass.LoadSynchronous();\n"
            "}\n",
            encoding="utf-8",
        )

        registry_dir = tmp_root / "Source" / "GV2" / "Public" / "UI"
        registry_dir.mkdir(parents=True)
        (registry_dir / "GV2ScreenRegistry.h").write_text(
            "struct FGV2ScreenRegistryEntry { FString ScreenId; };\n",
            encoding="utf-8",
        )

        test_dir = tmp_root / "Source" / "GV2" / "Private" / "Tests"
        test_dir.mkdir(parents=True)
        (test_dir / "SyntheticTest.cpp").write_text(
            "FGV2ScreenRegistryEntry BadEntry;\n",
            encoding="utf-8",
        )

        found_production = find_violations(production_dir.parent.parent)
        if not any("Synthetic.cpp" in violation for violation in found_production):
            print(f"FAILED: gate did not flag synthetic reference in production code: {found_production}")
            return False

        found_registry = find_violations(registry_dir.parent.parent)
        if any("GV2ScreenRegistry.h" in violation for violation in found_registry):
            print(f"FAILED: gate incorrectly flagged the registry's own header: {found_registry}")
            return False

        found_tests = find_violations(test_dir.parent)
        if any("SyntheticTest.cpp" in violation for violation in found_tests):
            print(f"FAILED: gate incorrectly flagged a reference inside Tests/ as production code: {found_tests}")
            return False

    print("SUCCESS: gate flags FGV2ScreenRegistryEntry references outside the registry and excludes Tests/")
    return True


def main(argv: list[str]) -> int:
    if argv == ["--self-test"]:
        return 0 if run_self_test() else 1
    if argv:
        print("usage: validate_screen_registry_entry_encapsulation.py [--self-test]", file=sys.stderr)
        return 2

    violations = validate_repository()
    if violations:
        print("\n".join(violations), file=sys.stderr)
        return 1
    print("SUCCESS: FGV2ScreenRegistryEntry is named only inside the registry that owns it")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
