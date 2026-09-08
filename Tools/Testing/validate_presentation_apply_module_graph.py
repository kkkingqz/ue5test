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
APPLY_BUILD_CS = REPO_ROOT / "Source" / "GV2PresentationApply" / "GV2PresentationApply.Build.cs"
GV2_BUILD_CS = REPO_ROOT / "Source" / "GV2" / "GV2.Build.cs"

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


def validate_repository() -> list[str]:
    violations: list[str] = []
    if not APPLY_BUILD_CS.exists():
        return [f"{APPLY_BUILD_CS}: not found"]
    violations.extend(find_violations(APPLY_BUILD_CS.read_text(encoding="utf-8"), str(APPLY_BUILD_CS)))

    if not GV2_BUILD_CS.exists():
        violations.append(f"{GV2_BUILD_CS}: not found")
    else:
        violations.extend(find_forward_edge_violation(GV2_BUILD_CS.read_text(encoding="utf-8")))
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
