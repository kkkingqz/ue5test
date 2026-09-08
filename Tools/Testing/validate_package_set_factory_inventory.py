#!/usr/bin/env python3
"""Enumerates every function declaration that returns FResolvedPackageSet and requires
each one to be a blessed host-bootstrap entry point.

PSC-02 (ADR-0043 D1/D5, PackageSet.md): FResolvedPackageSet is the single portable
package-set type shared by repository build, Lua/schema source loading, and UE
presentation candidate build. It must be constructed in exactly two places --
GV2ContentHostSupport::ResolvePackageSetFromContainer and
::ResolvePackageSetFromDirectories -- so that every consumer differs only in which of
these it calls and with what input, never in a separate discovery path of its own
(PAH-R3: a second, independent construction of the same set is a second authority,
even when it returns the same value today).

The inventory is derived from *declarations* (headers), matching PackageSet.md's own
"Готово" wording: "число мест создания set выводится по функциям, возвращающим
FResolvedPackageSet, из declarations". A function defined only inside an anonymous
namespace in a .cpp file (e.g. a host's own private conversion helper) has no header
declaration and is not itself a new construction surface reachable from outside its
translation unit -- it is intentionally out of scope; what this gate defends against
is a SECOND PUBLICLY DECLARED factory anywhere in the tree.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SCANNED_DIRS = ["Source", "Headless", "Tools"]

ALLOWED_FACTORIES = {
    "ResolvePackageSetFromContainer",
    "ResolvePackageSetFromDirectories",
}

# A declaration: some return-type text containing FResolvedPackageSet, then a name,
# then a parenthesized parameter list, then ';' (never '{' -- a definition is out of
# scope for this scan; PackageDiscovery.cpp's own definitions are checked implicitly
# because every definition of a publicly reachable factory also has a declaration).
DECLARATION_PATTERN = re.compile(
    r"(?P<rettype>[\w:<>,\s\*&]*FResolvedPackageSet[\w:<>,\s\*&]*)"
    r"\b(?P<name>[A-Za-z_]\w*)\s*\((?P<params>[^;{}]*)\)\s*;",
    re.DOTALL,
)


def mask_comments_and_literals(source: str) -> str:
    masked = list(source)
    index = 0
    while index < len(source):
        if source.startswith("//", index):
            end = source.find("\n", index)
            end = len(source) if end < 0 else end
            for i in range(index, end):
                masked[i] = " "
            index = end
            continue
        if source.startswith("/*", index):
            end = source.find("*/", index + 2)
            end = len(source) if end < 0 else end + 2
            for i in range(index, end):
                if masked[i] != "\n":
                    masked[i] = " "
            index = end
            continue
        if source[index] in {'"', "'"}:
            quote = source[index]
            end = index + 1
            while end < len(source):
                if source[end] == "\\":
                    end += 2
                    continue
                if source[end] == quote:
                    end += 1
                    break
                end += 1
            for i in range(index, min(end, len(source))):
                if masked[i] != "\n":
                    masked[i] = " "
            index = end
            continue
        index += 1
    return "".join(masked)


def header_files() -> dict[Path, str]:
    files: dict[Path, str] = {}
    for dir_name in SCANNED_DIRS:
        root = REPO_ROOT / dir_name
        if not root.exists():
            continue
        for path in sorted(root.rglob("*.h")):
            if path.is_file():
                files[path] = path.read_text(encoding="utf-8")
    return files


def find_violations(source_files: dict[Path, str]) -> list[str]:
    violations: list[str] = []
    for path, source in source_files.items():
        masked = mask_comments_and_literals(source)
        for match in DECLARATION_PATTERN.finditer(masked):
            name = match.group("name")
            if name in ALLOWED_FACTORIES:
                continue
            line_number = source.count("\n", 0, match.start()) + 1
            violations.append(
                f"{path}:{line_number}: '{name}' is declared returning FResolvedPackageSet "
                "but is not one of the two blessed host-bootstrap factories "
                f"({', '.join(sorted(ALLOWED_FACTORIES))}) -- a second construction surface "
                "for the package set is not allowed (PSC-02, PAH-R3)"
            )
    return violations


def validate_repository() -> list[str]:
    return find_violations(header_files())


def run_self_test() -> bool:
    print("[*] Running validate_package_set_factory_inventory self-test...")
    if errors := validate_repository():
        print("FAILED: current production headers violate the gate:\n" + "\n".join(errors))
        return False

    files = header_files()
    synthetic_path = REPO_ROOT / "Source" / "GV2ContentHostSupport" / "Public" / "__Synthetic.h"

    files[synthetic_path] = (
        "namespace GV2ContentHostSupport\n"
        "{\n"
        "std::optional<FResolvedPackageSet> SneakyPackageSetFactory(\n"
        "    const std::filesystem::path& Root,\n"
        "    std::vector<GV2ContentCore::FDiagnostic>& OutDiagnostics);\n"
        "}\n"
    )
    errors = find_violations(files)
    if not any("SneakyPackageSetFactory" in error for error in errors):
        print(f"FAILED: gate accepted an unblessed FResolvedPackageSet factory declaration: {errors}")
        return False

    files[synthetic_path] = (
        "namespace GV2ContentHostSupport\n"
        "{\n"
        "std::optional<FResolvedPackageSet> ResolvePackageSetFromDirectories(\n"
        "    const std::vector<std::filesystem::path>& PackageRoots,\n"
        "    std::vector<GV2ContentCore::FDiagnostic>& OutDiagnostics);\n"
        "}\n"
    )
    errors = find_violations(files)
    if any("ResolvePackageSetFromDirectories" in error for error in errors):
        print(f"FAILED: gate rejected a blessed factory re-declaration: {errors}")
        return False

    files.pop(synthetic_path)

    print("SUCCESS: FResolvedPackageSet is constructible only by the two blessed factories")
    return True


def main(argv: list[str]) -> int:
    if "--self-test" in argv:
        return 0 if run_self_test() else 1

    if errors := validate_repository():
        print("FAILED:\n" + "\n".join(errors))
        return 1

    print("SUCCESS: FResolvedPackageSet is constructible only by the two blessed factories")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
