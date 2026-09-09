#!/usr/bin/env python3
"""Validates GV2PresentationApply's own Build.cs dependency list against the exact
allowlist ADR-0043 D2/D4 names, and that GV2 declares the one allowed forward edge onto
it.

PSC-09A (ADR-0043 D2/D4, Payload.md M3): the dependency list in
Source/GV2PresentationApply/GV2PresentationApply.Build.cs IS the primary guarantee that
physical Apply cannot reach a content/authority type -- a module that never lists
GV2/GV2ContentCore/GV2ContentHostSupport/GV2RuntimeCore/DeveloperSettings/AssetRegistry/
ImageCore or any filesystem/content-authoring module cannot link code from it, whatever a
future authority type is named. Source-scanning stays a second, weaker rubric (ADR-0043's
own words -- the exact anti-pattern PAH-08's hand-listed authority accessors already
proved insufficient once, see AuditFindings.md). This gate reads the declaration
directly, not a hand-maintained expectation of what it "should" contain beyond the
allowlist itself.
"""

from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SOURCE_ROOT = REPO_ROOT / "Source"
APPLY_BUILD_CS = SOURCE_ROOT / "GV2PresentationApply" / "GV2PresentationApply.Build.cs"
GV2_BUILD_CS = SOURCE_ROOT / "GV2" / "GV2.Build.cs"
APPLY_MODULE = "GV2PresentationApply"

# PSC-11: the CMake side of the same claim. The portable/Headless build must neither compile
# nor link this module -- it is a UE-only physical layer, and a portable target that pulled
# it in would make the Headless run depend on UMG.
CMAKE_FILES = [
    REPO_ROOT / "CMakeLists.txt",
    SOURCE_ROOT / "CMakeLists.txt",
    REPO_ROOT / "Headless" / "CMakeLists.txt",
    REPO_ROOT / "Tools" / "Content" / "CMakeLists.txt",
]

# ADR-0043 D2: "Разрешённый allowlist зависимостей -- ровно Core, CoreUObject, Engine,
# UMG, CommonUI, Slate, SlateCore".
ALLOWED_MODULES = {
    "Core",
    "CoreUObject",
    "Engine",
    "UMG",
    "CommonUI",
    "Slate",
    "SlateCore",
}

# Explicit denylist named in ADR-0043 D2 and SystemContextAndComponents.md -- reported by
# name when found, even though "not in ALLOWED_MODULES" alone would already catch it, so
# a violation message names the actual rule broken instead of just "not permitted".
NAMED_DENYLIST = {
    "GV2",
    "GV2ContentCore",
    "GV2ContentHostSupport",
    "GV2RuntimeCore",
    "DeveloperSettings",
    "AssetRegistry",
    "ImageCore",
}

DEPENDENCY_LIST_PATTERN = re.compile(
    r"(Public|Private)DependencyModuleNames\s*\.\s*(?:AddRange\s*\(\s*new\s+string\s*\[\s*\]\s*\{(?P<range>[^}]*)\}|Add\s*\(\s*(?P<single>\"[^\"]*\")\s*\))",
    re.DOTALL,
)
STRING_LITERAL_PATTERN = re.compile(r'"([^"]*)"')


def extract_dependency_modules(source: str) -> list[str]:
    modules: list[str] = []
    for match in DEPENDENCY_LIST_PATTERN.finditer(source):
        body = match.group("range") if match.group("range") is not None else match.group("single")
        modules.extend(STRING_LITERAL_PATTERN.findall(body))
    return modules


def find_violations(build_cs_text: str, build_cs_label: str) -> list[str]:
    violations: list[str] = []
    modules = extract_dependency_modules(build_cs_text)
    if not modules:
        return [f"{build_cs_label}: no PublicDependencyModuleNames/PrivateDependencyModuleNames declaration found"]

    for module in modules:
        if module in ALLOWED_MODULES:
            continue
        if module in NAMED_DENYLIST:
            violations.append(
                f"{build_cs_label}: '{module}' is on ADR-0043 D2's explicit denylist -- "
                "GV2PresentationApply must never depend on it"
            )
        else:
            violations.append(
                f"{build_cs_label}: '{module}' is not in ADR-0043 D2's allowlist "
                f"({sorted(ALLOWED_MODULES)}) -- classify it there or remove the dependency"
            )
    return violations


def find_forward_edge_violation(gv2_build_cs_text: str) -> list[str]:
    modules = extract_dependency_modules(gv2_build_cs_text)
    if "GV2PresentationApply" not in modules:
        return [
            f"{GV2_BUILD_CS}: GV2.Build.cs does not declare a dependency on "
            "'GV2PresentationApply' -- ADR-0043 D2's one allowed forward edge (GV2 -> "
            "GV2PresentationApply) is missing"
        ]
    return []


def all_build_cs() -> dict[str, str]:
    """Every module declaration in the tree, by directory walk -- the actual UBT graph, not
    a list of the two files this gate used to read."""
    return {
        path.name[: -len(".Build.cs")]: path.read_text(encoding="utf-8")
        for path in sorted(SOURCE_ROOT.rglob("*.Build.cs"))
    }


def find_conditional_dependency_violations(source: str, label: str) -> list[str]:
    """A dependency added inside a conditional is a dependency the graph does not state.

    ADR-0043 D2's guarantee is that UBT CANNOT link an authority type here. An edge added
    only for Editor targets, or behind any other condition, turns that into a claim about
    which configuration was inspected.
    """
    errors: list[str] = []
    stripped = strip_comments(source)
    for match in re.finditer(r"\b(?:if|else|switch|\?)\b", stripped):
        tail = stripped[match.start():]
        block_end = tail.find("\n        }")
        block = tail[: block_end if block_end > 0 else 400]
        if "DependencyModuleNames" in block:
            line = stripped.count("\n", 0, match.start()) + 1
            errors.append(f"{label}:{line}: conditional dependency edge -- the graph must be unconditional")
    return errors


def find_cmake_violations() -> list[str]:
    """The portable/CMake graph must not name the Apply module as a target or a source."""
    errors: list[str] = []
    for path in CMAKE_FILES:
        if not path.exists():
            continue
        source = path.read_text(encoding="utf-8")
        text = re.sub(r"#[^\n]*", "", source)
        for match in re.finditer(re.escape(APPLY_MODULE), text):
            line = text.count("\n", 0, match.start()) + 1
            errors.append(
                f"{path.relative_to(REPO_ROOT)}:{line}: the portable/Headless CMake graph names "
                f"{APPLY_MODULE}; it must neither compile nor link it"
            )
    return errors


def strip_comments(source: str) -> str:
    def blank(match: re.Match[str]) -> str:
        return "".join("\n" if c == "\n" else " " for c in match.group(0))

    source = re.sub(r"/\*.*?\*/", blank, source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", blank, source)


def find_reverse_edge_violations(modules: dict[str, str]) -> list[str]:
    """No module the Apply module is forbidden to depend on may be reachable FROM it, and
    the Apply module itself must be depended on by GV2 alone -- a second consumer would be
    a second place the boundary has to hold."""
    errors: list[str] = []
    consumers = [
        name for name, source in modules.items()
        if name != APPLY_MODULE and APPLY_MODULE in extract_dependency_modules(source)
    ]
    if consumers != ["GV2"]:
        errors.append(
            f"{APPLY_MODULE} must be consumed by GV2 alone; actual consumers: {sorted(consumers)}"
        )
    return errors


def validate_repository() -> list[str]:
    violations: list[str] = []
    if not APPLY_BUILD_CS.exists():
        return [f"{APPLY_BUILD_CS}: not found"]
    violations.extend(find_violations(APPLY_BUILD_CS.read_text(encoding="utf-8"), str(APPLY_BUILD_CS)))

    if not GV2_BUILD_CS.exists():
        violations.append(f"{GV2_BUILD_CS}: not found")
    else:
        violations.extend(find_forward_edge_violation(GV2_BUILD_CS.read_text(encoding="utf-8")))

    # PSC-11: the actual graph, derived from every module declaration in the tree.
    modules = all_build_cs()
    if not modules:
        return violations + ["the module enumerator produced an empty set; the derivation is broken"]
    violations.extend(find_conditional_dependency_violations(modules[APPLY_MODULE], str(APPLY_BUILD_CS)))
    violations.extend(find_reverse_edge_violations(modules))
    violations.extend(find_cmake_violations())
    return violations


def run_self_test() -> bool:
    print("[*] Running validate_presentation_apply_module_graph self-test...")
    if errors := validate_repository():
        print("FAILED: current repository already violates the gate:\n" + "\n".join(errors))
        return False

    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_path = Path(tmpdir) / "Synthetic.Build.cs"

        # A denylisted module must be flagged, named specifically.
        tmp_path.write_text(
            'PublicDependencyModuleNames.AddRange(new string[] { "Core", "GV2ContentCore" });\n',
            encoding="utf-8",
        )
        errors = find_violations(tmp_path.read_text(encoding="utf-8"), str(tmp_path))
        if not any("GV2ContentCore" in error and "denylist" in error for error in errors):
            print(f"FAILED: gate did not flag a denylisted dependency: {errors}")
            return False

        # An unknown, unlisted module must also be flagged (not silently allowed just
        # because it isn't on the named denylist).
        tmp_path.write_text(
            'PublicDependencyModuleNames.AddRange(new string[] { "Core", "SomeFutureModule" });\n',
            encoding="utf-8",
        )
        errors = find_violations(tmp_path.read_text(encoding="utf-8"), str(tmp_path))
        if not any("SomeFutureModule" in error for error in errors):
            print(f"FAILED: gate did not flag an unclassified dependency: {errors}")
            return False

        # A single .Add("X") call (not AddRange) must be parsed too.
        tmp_path.write_text(
            'PublicDependencyModuleNames.AddRange(new string[] { "Core" });\n'
            'PrivateDependencyModuleNames.Add("AssetRegistry");\n',
            encoding="utf-8",
        )
        errors = find_violations(tmp_path.read_text(encoding="utf-8"), str(tmp_path))
        if not any("AssetRegistry" in error and "denylist" in error for error in errors):
            print(f"FAILED: gate did not flag a denylisted single .Add(...) dependency: {errors}")
            return False

        # Only the allowlist must produce zero violations.
        tmp_path.write_text(
            'PublicDependencyModuleNames.AddRange(new string[]\n'
            '{\n'
            '    "Core",\n'
            '    "CoreUObject",\n'
            '    "Engine",\n'
            '    "UMG",\n'
            '    "CommonUI",\n'
            '    "Slate",\n'
            '    "SlateCore"\n'
            '});\n',
            encoding="utf-8",
        )
        errors = find_violations(tmp_path.read_text(encoding="utf-8"), str(tmp_path))
        if errors:
            print(f"FAILED: gate rejected an entirely allowlisted dependency set: {errors}")
            return False

        # A GV2.Build.cs missing the forward edge must be flagged.
        errors = find_forward_edge_violation('PublicDependencyModuleNames.AddRange(new string[] { "Core" });\n')
        if not any("GV2PresentationApply" in error for error in errors):
            print(f"FAILED: gate did not flag a missing GV2 -> GV2PresentationApply forward edge: {errors}")
            return False

        # A GV2.Build.cs that does declare it must not be flagged.
        errors = find_forward_edge_violation(
            'PublicDependencyModuleNames.AddRange(new string[] { "Core", "GV2PresentationApply" });\n'
        )
        if errors:
            print(f"FAILED: gate rejected a GV2.Build.cs that does declare the forward edge: {errors}")
            return False

    # PSC-11: a dependency added behind a condition is a dependency the graph does not
    # state -- the guarantee has to hold for every target, not the one that was inspected.
    conditional = (
        "public class GV2PresentationApply : ModuleRules\n{\n"
        "    public GV2PresentationApply(ReadOnlyTargetRules Target) : base(Target)\n    {\n"
        "        PublicDependencyModuleNames.AddRange(new string[] { \"Core\" });\n"
        "        if (Target.Type == TargetType.Editor)\n        {\n"
        "            PublicDependencyModuleNames.Add(\"AssetRegistry\");\n        }\n    }\n}\n"
    )
    if not find_conditional_dependency_violations(conditional, "synthetic"):
        print("FAILED: gate accepted a conditional dependency edge")
        return False
    unconditional = (
        "public class GV2PresentationApply : ModuleRules\n{\n"
        "    public GV2PresentationApply(ReadOnlyTargetRules Target) : base(Target)\n    {\n"
        "        PublicDependencyModuleNames.AddRange(new string[] { \"Core\" });\n    }\n}\n"
    )
    if find_conditional_dependency_violations(unconditional, "synthetic"):
        print("FAILED: gate flagged an unconditional dependency list")
        return False

    # A second consumer is a second place the boundary has to hold.
    two_consumers = {
        "GV2PresentationApply": unconditional,
        "GV2": 'PublicDependencyModuleNames.AddRange(new string[] { "GV2PresentationApply" });',
        "GV2ContentEditor": 'PublicDependencyModuleNames.AddRange(new string[] { "GV2PresentationApply" });',
    }
    if not find_reverse_edge_violations(two_consumers):
        print("FAILED: gate accepted a second consumer of the Apply module")
        return False
    one_consumer = dict(two_consumers)
    del one_consumer["GV2ContentEditor"]
    if find_reverse_edge_violations(one_consumer):
        print("FAILED: gate flagged the single allowed consumer")
        return False

    print("SUCCESS: GV2PresentationApply's dependency list stays within the ADR-0043 D2 allowlist")
    return True


def main(argv: list[str]) -> int:
    if argv == ["--self-test"]:
        return 0 if run_self_test() else 1
    if argv:
        print("usage: validate_presentation_apply_module_graph.py [--self-test]", file=sys.stderr)
        return 2

    errors = validate_repository()
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("SUCCESS: GV2PresentationApply's dependency list stays within the ADR-0043 D2 allowlist")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
