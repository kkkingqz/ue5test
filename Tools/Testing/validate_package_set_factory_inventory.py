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

DESCRIPTOR_PRODUCER_PATTERN = re.compile(
    r"std::optional\s*<(?P<inner>[^;{}]*FPackageDescriptor[^;{}]*)>\s*"
    r"(?P<name>[A-Za-z_]\w*)\s*\((?P<params>[^;{}]*)\)\s*;",
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


def production_sources() -> dict[Path, str]:
    files: dict[Path, str] = {}
    for dir_name in SCANNED_DIRS:
        root = REPO_ROOT / dir_name
        if not root.exists():
            continue
        for path in sorted(root.rglob("*.cpp")):
            if "Tests" in path.parts or "Test" in path.parts:
                continue
            files[path] = path.read_text(encoding="utf-8")
    return files


def strip_dev_automation_regions(source: str) -> str:
    """Project a source file to code that may exist in a non-test UE build.

    Unknown preprocessor conditions are kept conservatively. Only conditions whose
    WITH_DEV_AUTOMATION_TESTS value is unambiguous are evaluated; this prevents an
    inverse condition or a production #else branch from becoming an inventory blind
    spot. Line count is preserved for diagnostics.
    """
    def production_condition(kind: str, expression: str) -> bool | None:
        compact = re.sub(r"\s+", "", expression)
        if kind == "ifdef" and compact == "WITH_DEV_AUTOMATION_TESTS":
            return True  # UE defines the macro in every configuration; its value is 0/1.
        if kind == "ifndef" and compact == "WITH_DEV_AUTOMATION_TESTS":
            return False
        if kind != "if":
            return None
        if compact in {"WITH_DEV_AUTOMATION_TESTS", "(WITH_DEV_AUTOMATION_TESTS)"}:
            return False
        if compact in {"!WITH_DEV_AUTOMATION_TESTS", "!(WITH_DEV_AUTOMATION_TESTS)"}:
            return True
        if compact in {"WITH_DEV_AUTOMATION_TESTS==0", "0==WITH_DEV_AUTOMATION_TESTS"}:
            return True
        if compact in {"WITH_DEV_AUTOMATION_TESTS!=0", "0!=WITH_DEV_AUTOMATION_TESTS"}:
            return False
        return None

    output: list[str] = []
    stack: list[dict[str, bool]] = []
    excluded = False
    for line in source.splitlines(keepends=True):
        directive = re.match(r"\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)", line)
        if directive:
            kind, expression = directive.group(1), directive.group(2)
            if kind in {"if", "ifdef", "ifndef"}:
                value = production_condition(kind, expression)
                stack.append({
                    "parent_excluded": excluded,
                    "known": value is not None,
                    "taken": value is True,
                })
                excluded = excluded or value is False
            elif kind == "elif" and stack:
                frame = stack[-1]
                if not frame["known"]:
                    excluded = frame["parent_excluded"]
                elif frame["taken"]:
                    excluded = True
                else:
                    value = production_condition("if", expression)
                    frame["known"] = value is not None
                    frame["taken"] = value is True
                    excluded = frame["parent_excluded"] or value is False
            elif kind == "else" and stack:
                frame = stack[-1]
                if frame["known"]:
                    branch_active = not frame["taken"]
                    frame["taken"] = True
                    excluded = frame["parent_excluded"] or not branch_active
                else:
                    excluded = frame["parent_excluded"]
            elif kind == "endif":
                excluded = stack.pop()["parent_excluded"] if stack else False
            output.append("\n" if line.endswith("\n") else "")
        else:
            output.append(("\n" if line.endswith("\n") else "") if excluded else line)
    return "".join(output)


def package_producer_names(headers: dict[Path, str]) -> set[str]:
    names: set[str] = set()
    for source in headers.values():
        masked = mask_comments_and_literals(source)
        names |= {match.group("name") for match in DECLARATION_PATTERN.finditer(masked)}
        names |= {match.group("name") for match in DESCRIPTOR_PRODUCER_PATTERN.finditer(masked)}
    return names


def is_host_bootstrap_layer(path: Path) -> bool:
    try:
        rel = path.relative_to(REPO_ROOT).as_posix()
    except ValueError:
        rel = path.as_posix()
    return (
        rel.startswith("Source/GV2ContentHostSupport/Private/")
        or rel.startswith("Source/GV2ContentAuthoring/")
        or rel.startswith("Source/GV2ContentEditor/")
        or rel.startswith("Source/GV2/Private/Runtime/")
        or rel.startswith("Headless/Source/")
        or rel.startswith("Tools/Content/Source/")
        or "Conformance" in path.name
    )


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
    headers = header_files()
    return find_violations(headers) + find_downstream_rediscovery_violations(
        production_sources(), package_producer_names(headers))


def find_downstream_rediscovery_violations(
    source_files: dict[Path, str], producer_names: set[str] | None = None) -> list[str]:
    """Reject package discovery/construction calls outside host bootstrap layers.

    Names are derived from return-type declarations. A newly named producer therefore
    enters the actual set automatically; the independent oracle is the caller's layer.
    """
    names = package_producer_names(header_files()) if producer_names is None else producer_names
    if not names:
        return ["the package producer declaration enumerator produced an empty set"]
    call = re.compile(r"\b(?:[A-Za-z_]\w*::)*(?P<name>" + "|".join(sorted(map(re.escape, names))) + r")\s*\(")
    violations: list[str] = []
    for path, raw_source in source_files.items():
        if is_host_bootstrap_layer(path):
            continue
        source = mask_comments_and_literals(strip_dev_automation_regions(raw_source))
        for match in call.finditer(source):
            line = raw_source.count("\n", 0, match.start()) + 1
            violations.append(
                f"{path}:{line}: downstream layer calls package producer '{match.group('name')}'; "
                "resolve the package set once in a host bootstrap layer and pass the value"
            )
    return violations


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

    # The function name is intentionally novel: the gate must derive package-set
    # construction/discovery calls from declarations and reject them by architectural
    # layer, not grow a list of remembered helper names.
    downstream = {
        REPO_ROOT / "Source/GV2/Private/Application/SyntheticCandidate.cpp": (
            "auto BuildCandidate() { return ResolvePackageSetFromDirectories(Roots, Diagnostics); }\n"
        ),
    }
    errors = find_downstream_rediscovery_violations(
        downstream, {"ResolvePackageSetFromDirectories"})
    if not errors:
        print("FAILED: gate accepted package-set rediscovery in a downstream Application layer")
        return False

    # A test-only branch may be ignored, but its production #else branch and an explicitly
    # non-test branch remain part of the production call inventory.
    preprocessor_probe = {
        REPO_ROOT / "Source/GV2/Private/Application/SyntheticConditional.cpp": (
            "#if WITH_DEV_AUTOMATION_TESTS\n"
            "auto TestOnly() { return ResolvePackageSetFromDirectories(TestRoots, Diagnostics); }\n"
            "#else\n"
            "auto ProductionElse() { return ResolvePackageSetFromDirectories(Roots, Diagnostics); }\n"
            "#endif\n"
            "#if !WITH_DEV_AUTOMATION_TESTS\n"
            "auto ProductionNegative() { return ResolvePackageSetFromDirectories(Roots, Diagnostics); }\n"
            "#endif\n"
        ),
    }
    errors = find_downstream_rediscovery_violations(
        preprocessor_probe, {"ResolvePackageSetFromDirectories"})
    if len(errors) != 2:
        print(f"FAILED: production conditional branches escaped package-set inventory: {errors}")
        return False

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
