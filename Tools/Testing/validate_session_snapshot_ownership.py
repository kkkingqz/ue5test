#!/usr/bin/env python3
"""Validates that FGV2SessionContentSnapshot and FGV2ResolvedScreenRegistry own independent,
immutable resolved values and do not retain pointers/references to the authoring UGV2ScreenRegistry DataAsset.

CFC-04A (ADR-0043 D1, SNAP-AF-01, STATUS-020):
1. FGV2SessionContentSnapshot and FGV2ResolvedScreenRegistry must not store pointers or references
   (raw, smart, TStrongObjectPtr, TWeakObjectPtr, TObjectPtr) to UGV2ScreenRegistry.
2. UGV2ScreenRegistry is strictly an authoring DataAsset: it must not contain mutable runtime caches
   (ResolvedByScreenId, bBuilt), mutating Build() methods, or Resolve() member functions.
3. FGV2ResolvedScreenRegistry must hold GC-safe strong references (TStrongObjectPtr<UClass>)
   to resolved widget classes.
4. FGV2SessionContentSnapshot member declarations are strictly inventoried and classified.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SNAPSHOT_HEADER_PATH = (
    REPO_ROOT / "Source" / "GV2" / "Private" / "Application" / "GV2SessionContentSnapshot.h"
)
SNAPSHOT_IMPL_PATH = (
    REPO_ROOT / "Source" / "GV2" / "Private" / "Application" / "GV2SessionContentSnapshot.cpp"
)
REGISTRY_HEADER_PATH = (
    REPO_ROOT / "Source" / "GV2" / "Public" / "UI" / "GV2ScreenRegistry.h"
)
REGISTRY_IMPL_PATH = (
    REPO_ROOT / "Source" / "GV2" / "Private" / "UI" / "GV2ScreenRegistry.cpp"
)

EXPECTED_SNAPSHOT_MEMBERS = {
    "Repository": "GV2ContentCore::FRepositoryReadHandle",
    "OrderedPackageIds": "TArray<FString>",
    "LuaSources": "std::vector<GV2RuntimeCore::FRuntimeSource>",
    "ScriptSetHash": "FString",
    "SchemaCache": "TSharedPtr<FGV2UiSchemaCache>",
    "ScreenRegistry": "FGV2ResolvedScreenRegistry",
    "ImageCatalog": "FGV2ResolvedImageCatalog",
    "Theme": "FGV2ResolvedUiTheme",
    "GameShellClass": "TStrongObjectPtr<UClass>",
    "RepositoryContentHash": "FString",
    "PackageSetFingerprint": "FString",
    "PresentationHash": "FString",
    "SessionContentId": "FString",
}

FORBIDDEN_REGISTRY_MUTABLE_FIELDS = [
    "ResolvedByScreenId",
    "bBuilt",
]


def strip_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", lambda match: "\n" * match.group(0).count("\n"), source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", source)


def extract_class_body(source: str, class_name: str) -> str | None:
    signature_pattern = re.compile(
        rf"(?<!friend\s)\bclass\s+(?:[A-Z0-9_]+_API\s+)?{re.escape(class_name)}\b(?!\s*;)"
    )
    match = signature_pattern.search(source)
    if not match:
        return None

    open_brace = source.find("{", match.end())
    if open_brace < 0:
        return None

    depth = 0
    for index in range(open_brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[open_brace : index + 1]
    return None


def extract_private_section(body: str) -> str:
    marker = body.find("private:")
    if marker < 0:
        return ""
    return body[marker:]


FIELD_LINE_PATTERN = re.compile(r"^\s*(?P<type>[\w:<>,\*&]+(?:\s+[\w:<>,\*&]+)*)\s+(?P<name>[A-Za-z_]\w*)\s*;\s*$")


def extract_member_fields(section: str) -> dict[str, str]:
    fields = {}
    for line in section.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("friend "):
            continue
        match = FIELD_LINE_PATTERN.match(line)
        if match is None:
            continue
        fields[match.group("name")] = match.group("type")
    return fields


def find_snapshot_ownership_violations(
    snapshot_header: str,
    registry_header: str,
    registry_impl: str,
) -> list[str]:
    violations: list[str] = []

    clean_snapshot_header = strip_comments(snapshot_header)
    clean_registry_header = strip_comments(registry_header)
    clean_registry_impl = strip_comments(registry_impl)

    # 1. Inspect FGV2SessionContentSnapshot
    snapshot_body = extract_class_body(clean_snapshot_header, "FGV2SessionContentSnapshot")
    if not snapshot_body:
        return ["Could not find 'class FGV2SessionContentSnapshot' in header source"]

    # Check for raw/smart pointers to UGV2ScreenRegistry in snapshot body
    ugv2_ptr_pattern = re.compile(
        r"(?:TStrongObjectPtr|TWeakObjectPtr|TObjectPtr|TSoftObjectPtr)\s*<\s*UGV2ScreenRegistry\s*>"
        r"|\bUGV2ScreenRegistry\s*\*"
    )
    if ugv2_ptr_pattern.search(snapshot_body):
        violations.append(
            "FGV2SessionContentSnapshot retains a pointer or reference to UGV2ScreenRegistry. "
            "It must exclusively own an independent FGV2ResolvedScreenRegistry value."
        )

    # Check getter returning UGV2ScreenRegistry
    if re.search(r"\bUGV2ScreenRegistry\s*[*&]\s*Get", snapshot_body):
        violations.append(
            "FGV2SessionContentSnapshot declares a getter returning UGV2ScreenRegistry pointer/reference."
        )

    # Check member inventory and types of FGV2SessionContentSnapshot
    private_section = extract_private_section(snapshot_body)
    members = extract_member_fields(private_section)
    if not members:
        violations.append("FGV2SessionContentSnapshot has no private members parsed; extraction failed")
    else:
        for name, expected_type in EXPECTED_SNAPSHOT_MEMBERS.items():
            if name not in members:
                violations.append(f"FGV2SessionContentSnapshot is missing expected member '{name}'")
            elif members[name] != expected_type:
                violations.append(
                    f"FGV2SessionContentSnapshot::{name} has type '{members[name]}', expected '{expected_type}'"
                )

        for member_name in members:
            if member_name not in EXPECTED_SNAPSHOT_MEMBERS:
                violations.append(
                    f"FGV2SessionContentSnapshot::{member_name} is unclassified in EXPECTED_SNAPSHOT_MEMBERS"
                )

    # 2. Inspect FGV2ResolvedScreenRegistry
    resolved_body = extract_class_body(clean_registry_header, "FGV2ResolvedScreenRegistry")
    if not resolved_body:
        violations.append("Could not find 'class FGV2ResolvedScreenRegistry' in registry header")
    else:
        if ugv2_ptr_pattern.search(resolved_body):
            violations.append(
                "FGV2ResolvedScreenRegistry retains a pointer/reference to UGV2ScreenRegistry."
            )
        # Ensure FResolvedScreenRow uses TStrongObjectPtr<UClass>
        if "TStrongObjectPtr<UClass>" not in resolved_body:
            violations.append(
                "FGV2ResolvedScreenRegistry must hold resolved widget classes via TStrongObjectPtr<UClass> for GC safety."
            )
        # Ensure Resolve is a const method on FGV2ResolvedScreenRegistry
        if not re.search(r"bool\s+Resolve\s*\([^)]*\)\s*const\s*;", resolved_body):
            violations.append(
                "FGV2ResolvedScreenRegistry must declare 'bool Resolve(...) const;'."
            )

    # 3. Inspect UGV2ScreenRegistry
    ugv2_body = extract_class_body(clean_registry_header, "UGV2ScreenRegistry")
    if not ugv2_body:
        violations.append("Could not find 'class UGV2ScreenRegistry' in registry header")
    else:
        for forbidden_field in FORBIDDEN_REGISTRY_MUTABLE_FIELDS:
            if re.search(rf"\b{re.escape(forbidden_field)}\b", ugv2_body):
                violations.append(
                    f"UGV2ScreenRegistry contains forbidden mutable runtime cache field '{forbidden_field}'."
                )

        # Prohibit non-const Build(...) method on UGV2ScreenRegistry
        if re.search(r"\bBuild\s*\(", ugv2_body):
            violations.append(
                "UGV2ScreenRegistry declares a member Build() method. It must use const CompileResolvedRegistry()."
            )

        # Prohibit member Resolve(...) method on UGV2ScreenRegistry (Resolve belongs to FGV2ResolvedScreenRegistry)
        if re.search(r"\bResolve\s*\(", ugv2_body):
            violations.append(
                "UGV2ScreenRegistry declares a member Resolve() method. Resolve must belong to FGV2ResolvedScreenRegistry."
            )

        # Ensure CompileResolvedRegistry is const
        if not re.search(r"bool\s+CompileResolvedRegistry\s*\([^)]*\)\s*const\s*;", ugv2_body):
            violations.append(
                "UGV2ScreenRegistry must declare 'bool CompileResolvedRegistry(...) const;'."
            )

    # Also check UGV2ScreenRegistry.cpp for old member Build or member Resolve definitions
    if re.search(r"\bbool\s+UGV2ScreenRegistry::Build\s*\(", clean_registry_impl):
        violations.append("UGV2ScreenRegistry.cpp defines obsolete member UGV2ScreenRegistry::Build().")
    if re.search(r"\bbool\s+UGV2ScreenRegistry::Resolve\s*\(", clean_registry_impl):
        violations.append("UGV2ScreenRegistry.cpp defines obsolete member UGV2ScreenRegistry::Resolve().")

    return violations


def validate_repository() -> list[str]:
    return find_snapshot_ownership_violations(
        SNAPSHOT_HEADER_PATH.read_text(encoding="utf-8"),
        REGISTRY_HEADER_PATH.read_text(encoding="utf-8"),
        REGISTRY_IMPL_PATH.read_text(encoding="utf-8"),
    )


def run_self_test() -> bool:
    print("[*] Running validate_session_snapshot_ownership self-test...")
    base_snapshot_hdr = SNAPSHOT_HEADER_PATH.read_text(encoding="utf-8")
    base_registry_hdr = REGISTRY_HEADER_PATH.read_text(encoding="utf-8")
    base_registry_impl = REGISTRY_IMPL_PATH.read_text(encoding="utf-8")

    errors = find_snapshot_ownership_violations(base_snapshot_hdr, base_registry_hdr, base_registry_impl)
    if errors:
        print("FAILED: current production sources violate the snapshot ownership contract:\n" + "\n".join(errors))
        return False

    # Negative mutation 1: Snapshot retains TStrongObjectPtr<UGV2ScreenRegistry>
    mutated_snap_1 = base_snapshot_hdr.replace(
        "FGV2ResolvedScreenRegistry ScreenRegistry;",
        "TStrongObjectPtr<UGV2ScreenRegistry> ScreenRegistry;",
    )
    if not any("retains a pointer or reference" in err for err in find_snapshot_ownership_violations(mutated_snap_1, base_registry_hdr, base_registry_impl)):
        print("FAILED: gate did not flag Snapshot containing TStrongObjectPtr<UGV2ScreenRegistry>")
        return False

    # Negative mutation 2: Snapshot getter returning UGV2ScreenRegistry*
    mutated_snap_2 = base_snapshot_hdr.replace(
        "const FGV2ResolvedScreenRegistry& GetScreenRegistry() const { return ScreenRegistry; }",
        "UGV2ScreenRegistry* GetScreenRegistry() const { return nullptr; }",
    )
    if not any("declares a getter returning UGV2ScreenRegistry" in err for err in find_snapshot_ownership_violations(mutated_snap_2, base_registry_hdr, base_registry_impl)):
        print("FAILED: gate did not flag getter returning UGV2ScreenRegistry*")
        return False

    # Negative mutation 3: UGV2ScreenRegistry has mutable ResolvedByScreenId
    mutated_reg_3 = base_registry_hdr.replace(
        "TArray<FGV2ScreenRegistryEntry> Entries;",
        "TArray<FGV2ScreenRegistryEntry> Entries;\n    TMap<FString, int32> ResolvedByScreenId;",
    )
    if not any("ResolvedByScreenId" in err for err in find_snapshot_ownership_violations(base_snapshot_hdr, mutated_reg_3, base_registry_impl)):
        print("FAILED: gate did not flag mutable ResolvedByScreenId in UGV2ScreenRegistry")
        return False

    # Negative mutation 4: UGV2ScreenRegistry has bBuilt
    mutated_reg_4 = base_registry_hdr.replace(
        "TArray<FGV2ScreenRegistryEntry> Entries;",
        "TArray<FGV2ScreenRegistryEntry> Entries;\n    bool bBuilt = false;",
    )
    if not any("bBuilt" in err for err in find_snapshot_ownership_violations(base_snapshot_hdr, mutated_reg_4, base_registry_impl)):
        print("FAILED: gate did not flag mutable bBuilt in UGV2ScreenRegistry")
        return False

    # Negative mutation 5: UGV2ScreenRegistry declares member Build()
    mutated_reg_5 = base_registry_hdr.replace(
        "bool CompileResolvedRegistry(",
        "bool Build(const TArray<GV2PackageClosure::FEntry>& ClosureEntries, FString& OutError);\n    bool CompileResolvedRegistry(",
    )
    if not any("declares a member Build() method" in err for err in find_snapshot_ownership_violations(base_snapshot_hdr, mutated_reg_5, base_registry_impl)):
        print("FAILED: gate did not flag Build() member method on UGV2ScreenRegistry")
        return False

    # Negative mutation 6: UGV2ScreenRegistry declares member Resolve()
    mutated_reg_6 = base_registry_hdr.replace(
        "bool CompileResolvedRegistry(",
        "bool Resolve(const FString& ScreenId) const;\n    bool CompileResolvedRegistry(",
    )
    if not any("declares a member Resolve() method" in err for err in find_snapshot_ownership_violations(base_snapshot_hdr, mutated_reg_6, base_registry_impl)):
        print("FAILED: gate did not flag Resolve() member method on UGV2ScreenRegistry")
        return False

    # Negative mutation 7: FGV2ResolvedScreenRegistry misses TStrongObjectPtr<UClass>
    mutated_reg_7 = base_registry_hdr.replace(
        "TStrongObjectPtr<UClass> WidgetClass;",
        "UClass* WidgetClass;",
    )
    if not any("TStrongObjectPtr<UClass>" in err for err in find_snapshot_ownership_violations(base_snapshot_hdr, mutated_reg_7, base_registry_impl)):
        print("FAILED: gate did not flag missing TStrongObjectPtr<UClass> in FGV2ResolvedScreenRegistry")
        return False

    # Negative mutation 8: Unclassified member in Snapshot
    mutated_snap_8 = base_snapshot_hdr.replace(
        "FString SessionContentId;",
        "FString SessionContentId;\n    int32 RogueField;",
    )
    if not any("RogueField" in err for err in find_snapshot_ownership_violations(mutated_snap_8, base_registry_hdr, base_registry_impl)):
        print("FAILED: gate did not flag unclassified member in FGV2SessionContentSnapshot")
        return False

    print("SUCCESS: validate_session_snapshot_ownership self-test passed (8/8 negative mutations caught)")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true", help="Run negative mutation self-tests")
    args = parser.parse_args()

    if args.self_test:
        return 0 if run_self_test() else 1

    errors = validate_repository()
    if errors:
        print("validate_session_snapshot_ownership FAILED:\n" + "\n".join(f" - {err}" for err in errors), file=sys.stderr)
        return 1

    print("SUCCESS: FGV2SessionContentSnapshot and FGV2ResolvedScreenRegistry enforce value-isolated ownership")
    return 0


if __name__ == "__main__":
    sys.exit(main())
