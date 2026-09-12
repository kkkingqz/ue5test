#!/usr/bin/env python3
"""Validates that:
1. FGV2SessionContentSnapshot and FGV2ResolvedScreenRegistry own independent,
   immutable resolved values and do not retain pointers/references to the authoring UGV2ScreenRegistry DataAsset.
2. Prepared UI types (reconciliation, collection consumers, tab consumers, screen field plans, property mutations)
   strictly maintain explicit GC ownership: off-tree candidates use TStrongObjectPtr, borrowed targets use
   TWeakObjectPtr, and no untraced raw or plain TObjectPtr pointers exist.

CFC-04A (ADR-0043 D1, SNAP-AF-01, STATUS-020):
- FGV2SessionContentSnapshot and FGV2ResolvedScreenRegistry must not store pointers or references to UGV2ScreenRegistry.
- UGV2ScreenRegistry is strictly an authoring DataAsset without mutable runtime caches.
- FGV2ResolvedScreenRegistry holds GC-safe strong references (TStrongObjectPtr<UClass>) to resolved widget classes.
- FGV2SessionContentSnapshot member declarations are strictly inventoried and classified.

CFC-04B (ADR-0041, ADR-0042, CFC-AF-03, STATUS-021):
- Prepared candidate widgets (FPreparedScreenInstance, CandidateWidgetsByKey) are held as TStrongObjectPtr.
- Borrowed targets (HostWidget, TargetWidget, ScreenWidget, ActiveWidgetsByKey, Modals) are held as TWeakObjectPtr.
- Member inventories of prepared UI types are strictly inventoried and classified.
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
RECONCILER_HEADER_PATH = (
    REPO_ROOT / "Source" / "GV2" / "Public" / "UI" / "GV2LayeredUiReconciler.h"
)
PROPERTY_CONSUMERS_HEADER_PATH = (
    REPO_ROOT / "Source" / "GV2" / "Public" / "UI" / "GV2PropertyConsumers.h"
)
SCREEN_WIDGET_BASE_HEADER_PATH = (
    REPO_ROOT / "Source" / "GV2" / "Public" / "UI" / "GV2ScreenWidgetBase.h"
)
UI_MUTATION_PLAN_HEADER_PATH = (
    REPO_ROOT / "Source" / "GV2" / "Public" / "UI" / "GV2UiMutationPlan.h"
)

EXPECTED_PREPARED_TYPES_MEMBERS: dict[str, dict[str, str]] = {
    "FActiveScreenEntry": {
        "ScreenId": "FString",
        "Widget": "TWeakObjectPtr<UGV2ScreenWidgetBase>",
    },
    "FPreparedScreenInstance": {
        "Layer": "FName",
        "InstanceKey": "FName",
        "ScreenId": "FString",
        "TargetWidget": "TWeakObjectPtr<UGV2ScreenWidgetBase>",
        "CandidateWidget": "TStrongObjectPtr<UGV2ScreenWidgetBase>",
        "MutationPlan": "FGV2ScreenMutationPlan",
        "CentralStyleTransaction": "GV2PresentationApply::FGV2PreparedPresentationTransaction",
        "bIsReuse": "bool",
    },
    "FPreparedReconciliationPlan": {
        "ScreensToUpdateOrAttach": "TArray<FPreparedScreenInstance>",
        "NewActiveScreens": "TMap<FScreenSlotKey, FActiveScreenEntry>",
        "bHasModals": "bool",
        "Modals": "TArray<TWeakObjectPtr<UGV2ScreenWidgetBase>>",
    },
    "FPreparedCollectionItem": {
        "Key": "FName",
        "Widget": "TWeakObjectPtr<UWidget>",
        "Plan": "TSharedPtr<FGV2UiHostMutationPlan>",
        "bIsHost": "bool",
        "PreviousCommittedSnapshot": "FGV2UiHostCommittedSnapshot",
        "bIsReused": "bool",
        "RollbackPlan": "TSharedPtr<FGV2UiHostMutationPlan>",
        "CommittedValue": "TSharedPtr<const FGV2PreparedUiObject>",
        "CentralStyleTransaction": "GV2PresentationApply::FGV2PreparedPresentationTransaction",
    },
    "FGV2KeyedCollectionPropertyConsumer": {
        "PreparedItems": "TArray<FPreparedCollectionItem>",
        "ActiveWidgetsByKey": "TMap<FName, TWeakObjectPtr<UWidget>>",
        "CandidateWidgetsByKey": "TMap<FName, TStrongObjectPtr<UWidget>>",
        "CompiledItemSpec": "GV2ContentCore::FCompiledUiFieldSpecPtr",
        "ContextSchemaId": "FString",
        "ContextPropertyPath": "FString",
        "ContextScreenId": "FString",
        "ContextFieldId": "FString",
        "PrepareContext": "const FGV2PresentationPrepareContext*",
        "Discrepancies": "TArray<FGV2CollectionItemDiscrepancy>",
    },
    "FPreparedTabItem": {
        "Key": "FName",
        "Title": "FGV2TextViewModel",
        "ScreenId": "FString",
        "ScreenWidgetClass": "TSubclassOf<UGV2ScreenWidgetBase>",
        "ScreenWidget": "TWeakObjectPtr<UGV2ScreenWidgetBase>",
        "ChildScreenPlan": "TSharedPtr<FGV2ScreenMutationPlan>",
        "CentralStyleTransaction": "GV2PresentationApply::FGV2PreparedPresentationTransaction",
        "bHasChildPlan": "bool",
    },
    "FGV2TabContainerTabsPropertyConsumer": {
        "PreparedTabs": "TArray<FPreparedTabItem>",
        "CandidateWidgetsByKey": "TMap<FName, TStrongObjectPtr<UGV2ScreenWidgetBase>>",
        "ActiveCompositionChain": "const TArray<FString>*",
        "PrepareContext": "const FGV2PresentationPrepareContext*",
        "bHasAcceptedRevision": "bool",
    },
    "FGV2ScreenFieldPlan": {
        "HostWidget": "TWeakObjectPtr<UUserWidget>",
        "MutationPlan": "FGV2UiHostMutationPlan",
        "CommittedValue": "TSharedPtr<const FGV2PreparedUiObject>",
        "CommittedSchema": "std::shared_ptr<const GV2ContentCore::FCompiledUiFieldSpec>",
        "CommittedSchemaId": "FString",
        "PreviousCommittedSnapshot": "FGV2UiHostCommittedSnapshot",
        "RollbackPlan": "FGV2UiHostMutationPlan",
    },
    "FGV2ScreenMutationPlan": {
        "FieldPlans": "TArray<FGV2ScreenFieldPlan>",
    },
    "FGV2UiPropertyMutation": {
        "PropertyName": "FString",
        "PropertyPath": "FString",
        "Kind": "EGV2PreparedUiValueKind",
        "Consumer": "TSharedPtr<IGV2PropertyConsumer>",
        "TargetWidget": "TWeakObjectPtr<UWidget>",
        "bIsReset": "bool",
        "PreparedValue": "FGV2PreparedUiValue",
    },
    "FGV2UiHostMutationPlan": {
        "Mutations": "TArray<FGV2UiPropertyMutation>",
    },
}

PREPARED_TYPES_CONFIG: list[tuple[str, str, bool, bool]] = [
    # (type_name, header_key, private_only, strip_nested)
    ("FActiveScreenEntry", "reconciler", False, False),
    ("FPreparedScreenInstance", "reconciler", False, False),
    ("FPreparedReconciliationPlan", "reconciler", False, False),
    ("FPreparedCollectionItem", "property_consumers", False, False),
    ("FGV2KeyedCollectionPropertyConsumer", "property_consumers", True, True),
    ("FPreparedTabItem", "property_consumers", False, False),
    ("FGV2TabContainerTabsPropertyConsumer", "property_consumers", True, True),
    ("FGV2ScreenFieldPlan", "screen_widget_base", False, False),
    ("FGV2ScreenMutationPlan", "screen_widget_base", False, False),
    ("FGV2UiPropertyMutation", "ui_mutation_plan", False, False),
    ("FGV2UiHostMutationPlan", "ui_mutation_plan", True, True),
]

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


def extract_type_body(source: str, type_name: str) -> str | None:
    signature_pattern = re.compile(
        rf"(?<!friend\s)\b(?:class|struct)\s+(?:[A-Z0-9_]+_API\s+)?{re.escape(type_name)}\b(?!\s*;)"
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


def extract_class_body(source: str, class_name: str) -> str | None:
    return extract_type_body(source, class_name)


def extract_private_section(body: str) -> str:
    marker = body.find("private:")
    if marker < 0:
        return ""
    return body[marker:]


def strip_nested_blocks(body: str) -> str:
    result = []
    depth = 0
    for char in body:
        if char == "{":
            depth += 1
            if depth == 1:
                result.append(";")
        elif char == "}":
            depth -= 1
        elif depth == 0:
            result.append(char)
    return "".join(result)


FIELD_LINE_PATTERN = re.compile(
    r"^\s*(?P<type>[\w:<>,\*&]+(?:\s+[\w:<>,\*&]+)*)\s+(?P<name>[A-Za-z_]\w*)\s*(?:=\s*[^;]+)?;\s*$"
)


def extract_member_fields(body: str, private_only: bool = False, strip_nested: bool = False) -> dict[str, str]:
    if private_only:
        marker = body.find("private:")
        if marker >= 0:
            body = body[marker:]
        else:
            return {}
    if strip_nested:
        body = strip_nested_blocks(body)
    fields = {}
    for line in body.splitlines():
        stripped = line.strip()
        if (
            not stripped
            or stripped.startswith("friend ")
            or stripped.startswith("GENERATED_BODY")
            or stripped.startswith("public:")
            or stripped.startswith("protected:")
            or stripped.startswith("private:")
        ):
            continue
        if "(" in stripped and not stripped.startswith("using "):
            continue
        match = FIELD_LINE_PATTERN.match(stripped)
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


def find_prepared_types_ownership_violations(headers: dict[str, str]) -> list[str]:
    violations: list[str] = []
    clean_headers = {k: strip_comments(v) for k, v in headers.items()}

    untraced_widget_ptr_pattern = re.compile(
        r"\b(?:UWidget|UUserWidget|UGV2ScreenWidgetBase)\s*\*|TObjectPtr\s*<\s*(?:UWidget|UUserWidget|UGV2ScreenWidgetBase)\s*>"
    )

    for type_name, header_key, private_only, strip_nested in PREPARED_TYPES_CONFIG:
        header_src = clean_headers.get(header_key, "")
        body = extract_type_body(header_src, type_name)
        if not body:
            violations.append(f"Could not find '{type_name}' in {header_key} header source")
            continue

        members = extract_member_fields(body, private_only=private_only, strip_nested=strip_nested)
        if not members:
            violations.append(f"{type_name} has no member fields parsed; extraction failed")
            continue

        expected = EXPECTED_PREPARED_TYPES_MEMBERS.get(type_name, {})
        for name, expected_type in expected.items():
            if name not in members:
                violations.append(f"{type_name} is missing expected member '{name}'")
            elif members[name] != expected_type:
                violations.append(
                    f"{type_name}::{name} has type '{members[name]}', expected '{expected_type}'"
                )

        for member_name in members:
            if member_name not in expected:
                violations.append(
                    f"{type_name}::{member_name} is unclassified in EXPECTED_PREPARED_TYPES_MEMBERS"
                )

            actual_type = members[member_name]
            if untraced_widget_ptr_pattern.search(actual_type):
                violations.append(
                    f"{type_name}::{member_name} has untraced raw/plain pointer type '{actual_type}'. "
                    "Off-tree candidates must use TStrongObjectPtr and borrowed targets must use TWeakObjectPtr."
                )

    return violations


def validate_repository() -> list[str]:
    violations = find_snapshot_ownership_violations(
        SNAPSHOT_HEADER_PATH.read_text(encoding="utf-8"),
        REGISTRY_HEADER_PATH.read_text(encoding="utf-8"),
        REGISTRY_IMPL_PATH.read_text(encoding="utf-8"),
    )
    headers = {
        "reconciler": RECONCILER_HEADER_PATH.read_text(encoding="utf-8"),
        "property_consumers": PROPERTY_CONSUMERS_HEADER_PATH.read_text(encoding="utf-8"),
        "screen_widget_base": SCREEN_WIDGET_BASE_HEADER_PATH.read_text(encoding="utf-8"),
        "ui_mutation_plan": UI_MUTATION_PLAN_HEADER_PATH.read_text(encoding="utf-8"),
    }
    violations.extend(find_prepared_types_ownership_violations(headers))
    return violations


def run_self_test() -> bool:
    print("[*] Running validate_session_snapshot_ownership self-test...")
    base_snapshot_hdr = SNAPSHOT_HEADER_PATH.read_text(encoding="utf-8")
    base_registry_hdr = REGISTRY_HEADER_PATH.read_text(encoding="utf-8")
    base_registry_impl = REGISTRY_IMPL_PATH.read_text(encoding="utf-8")
    base_headers = {
        "reconciler": RECONCILER_HEADER_PATH.read_text(encoding="utf-8"),
        "property_consumers": PROPERTY_CONSUMERS_HEADER_PATH.read_text(encoding="utf-8"),
        "screen_widget_base": SCREEN_WIDGET_BASE_HEADER_PATH.read_text(encoding="utf-8"),
        "ui_mutation_plan": UI_MUTATION_PLAN_HEADER_PATH.read_text(encoding="utf-8"),
    }

    errors = find_snapshot_ownership_violations(base_snapshot_hdr, base_registry_hdr, base_registry_impl)
    if errors:
        print("FAILED: current production sources violate the snapshot ownership contract:\n" + "\n".join(errors))
        return False

    prep_errors = find_prepared_types_ownership_violations(base_headers)
    if prep_errors:
        print("FAILED: current production sources violate the prepared types ownership contract:\n" + "\n".join(prep_errors))
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

    # Negative mutation 9: FPreparedScreenInstance::CandidateWidget uses TObjectPtr
    mutated_9 = dict(base_headers)
    mutated_9["reconciler"] = mutated_9["reconciler"].replace(
        "TStrongObjectPtr<UGV2ScreenWidgetBase> CandidateWidget;",
        "TObjectPtr<UGV2ScreenWidgetBase> CandidateWidget;",
    )
    if not any("CandidateWidget" in err for err in find_prepared_types_ownership_violations(mutated_9)):
        print("FAILED: gate did not flag CandidateWidget using TObjectPtr")
        return False

    # Negative mutation 10: FGV2KeyedCollectionPropertyConsumer::CandidateWidgetsByKey uses untraced raw pointer
    mutated_10 = dict(base_headers)
    mutated_10["property_consumers"] = mutated_10["property_consumers"].replace(
        "TMap<FName, TStrongObjectPtr<UWidget>> CandidateWidgetsByKey;",
        "TMap<FName, UWidget*> CandidateWidgetsByKey;",
    )
    if not any("CandidateWidgetsByKey" in err for err in find_prepared_types_ownership_violations(mutated_10)):
        print("FAILED: gate did not flag CandidateWidgetsByKey using raw pointer")
        return False

    # Negative mutation 11: FGV2ScreenFieldPlan::HostWidget uses raw pointer UUserWidget*
    mutated_11 = dict(base_headers)
    mutated_11["screen_widget_base"] = mutated_11["screen_widget_base"].replace(
        "TWeakObjectPtr<UUserWidget> HostWidget;",
        "UUserWidget* HostWidget;",
    )
    if not any("HostWidget" in err for err in find_prepared_types_ownership_violations(mutated_11)):
        print("FAILED: gate did not flag FGV2ScreenFieldPlan::HostWidget using raw pointer")
        return False

    # Negative mutation 12: FGV2UiPropertyMutation::TargetWidget uses raw pointer UWidget*
    mutated_12 = dict(base_headers)
    mutated_12["ui_mutation_plan"] = mutated_12["ui_mutation_plan"].replace(
        "TWeakObjectPtr<UWidget> TargetWidget;",
        "UWidget* TargetWidget;",
    )
    if not any("TargetWidget" in err for err in find_prepared_types_ownership_violations(mutated_12)):
        print("FAILED: gate did not flag FGV2UiPropertyMutation::TargetWidget using raw pointer")
        return False

    # Negative mutation 13: Unclassified member in FPreparedScreenInstance
    mutated_13 = dict(base_headers)
    mutated_13["reconciler"] = mutated_13["reconciler"].replace(
        "bool bIsReuse = false;",
        "bool bIsReuse = false;\n    UWidget* RogueCandidateWidget;",
    )
    if not any("RogueCandidateWidget" in err for err in find_prepared_types_ownership_violations(mutated_13)):
        print("FAILED: gate did not flag unclassified member in FPreparedScreenInstance")
        return False

    print("SUCCESS: validate_session_snapshot_ownership self-test passed (13/13 negative mutations caught)")
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

    print("SUCCESS: FGV2SessionContentSnapshot and prepared UI types enforce GC safety and value-isolated ownership")
    return 0


if __name__ == "__main__":
    sys.exit(main())
