#!/usr/bin/env python3
"""Monotonic decrease gate for the Universal UI Property Pipeline migration (UPP-11).

Counts three legacy-surface metrics that the migration (UPP-12..30) must only ever
shrink, never grow, once a widget starts migrating to the universal pipeline:

  1. Schema-specific Prepare/Build functions in GV2ScreenFieldAdapterRegistry.cpp.
  2. Payload members of FGV2ScreenFieldValue (the union every legacy screen field
     value is boxed into).
  3. Schema-specific DTO structs in GV2BridgeTypes.h (excludes the small set of
     generic/envelope structs the new pipeline still needs: binding handle, text
     view model, screen/document envelopes, session status).

Each metric has a stored upper bound below. A count above its bound fails the gate;
a count below is fine (that's the migration progressing) but does NOT lower the
bound automatically -- update BASELINES by hand as a deliberate part of the change
set that shrinks a metric, per the plan's rule that a real decrease is evidence, not
an assumption.

Rule ID: UI_PIPELINE_LEGACY_GATE
"""

from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
ADAPTER_REGISTRY_PATH = REPO_ROOT / "Source" / "GV2" / "Private" / "Application" / "GV2ScreenFieldAdapterRegistry.cpp"
BRIDGE_TYPES_PATH = REPO_ROOT / "Source" / "GV2" / "Public" / "Bridge" / "GV2BridgeTypes.h"

# Upper bounds captured at UPP-11 (2026-08-24). Lowering one of these is how a widget
# migration change set proves it actually deleted its legacy adapter/DTO/union branch,
# per the plan's rule 2 ("удаляет свой PrepareXxx/BuildXxx/DTO/ветку union").
BASELINES = {
    "prepare_build_functions": 20,
    "screen_field_value_payload_members": 8,
    "schema_specific_dtos": 16,
}

PREPARE_BUILD_PATTERN = re.compile(r"^bool (Prepare|Build)[A-Z][A-Za-z0-9_]*\(", re.MULTILINE)

# FGV2ScreenFieldValue's identity fields (not payload) and its one member that is not
# a UPROPERTY (TSharedPtr<FGV2TabContainerViewModel> cannot be reflected: the type is
# only forward-declared at the point of the struct's definition).
SCREEN_FIELD_VALUE_IDENTITY_FIELDS = 2  # FieldId, SchemaId
SCREEN_FIELD_VALUE_UNREFLECTED_PAYLOAD_MEMBERS = ["TabContainerValue"]

STRUCT_PATTERN = re.compile(r"^struct GV2_API ([A-Za-z0-9_]+)", re.MULTILINE)
UPROPERTY_PATTERN = re.compile(r"UPROPERTY\(")

# Generic/envelope structs GV2BridgeTypes.h keeps permanently: not schema-specific DTOs,
# so they are excluded from the count this gate tracks. Adding a new schema-specific
# ViewModel struct is not exempt by editing this set -- only genuinely generic,
# domain-agnostic infrastructure belongs here.
INFRA_STRUCT_ALLOWLIST = {
    "FGV2UiBindingHandle",
    "FGV2UiControlValue",
    "FGV2TextViewModel",
    "FGV2ScreenFieldDescriptor",
    "FGV2ScreenFieldValue",
    "FGV2ScreenViewModel",
    "FGV2ScreenInstanceViewModel",
    "FGV2UiDocumentViewModel",
    "FGV2SessionStatus",
}


def count_prepare_build_functions(source: str) -> int:
    return len(PREPARE_BUILD_PATTERN.findall(source))


def count_screen_field_value_payload_members(source: str) -> int:
    struct_start = source.index("struct GV2_API FGV2ScreenFieldValue")
    # The field block ends where the first static factory method begins.
    static_start = source.index("static FGV2ScreenFieldValue Make", struct_start)
    body = source[struct_start:static_start]
    uproperty_count = len(UPROPERTY_PATTERN.findall(body))
    unreflected_count = sum(1 for name in SCREEN_FIELD_VALUE_UNREFLECTED_PAYLOAD_MEMBERS if name in body)
    return uproperty_count - SCREEN_FIELD_VALUE_IDENTITY_FIELDS + unreflected_count


def count_schema_specific_dtos(source: str) -> int:
    all_structs = STRUCT_PATTERN.findall(source)
    return len([name for name in all_structs if name not in INFRA_STRUCT_ALLOWLIST])


def run_validation(adapter_registry_path: Path, bridge_types_path: Path) -> list[str]:
    violations: list[str] = []

    if not adapter_registry_path.exists():
        violations.append(f"{adapter_registry_path}: error [UI_PIPELINE_LEGACY_GATE]: file not found")
        return violations
    if not bridge_types_path.exists():
        violations.append(f"{bridge_types_path}: error [UI_PIPELINE_LEGACY_GATE]: file not found")
        return violations

    adapter_source = adapter_registry_path.read_text(encoding="utf-8")
    bridge_source = bridge_types_path.read_text(encoding="utf-8")

    actual = {
        "prepare_build_functions": count_prepare_build_functions(adapter_source),
        "screen_field_value_payload_members": count_screen_field_value_payload_members(bridge_source),
        "schema_specific_dtos": count_schema_specific_dtos(bridge_source),
    }

    for metric, count in actual.items():
        baseline = BASELINES[metric]
        if count > baseline:
            violations.append(
                f"error [UI_PIPELINE_LEGACY_GATE]: {metric} grew from {baseline} to {count}. "
                f"The universal UI property pipeline migration (UPP-11) forbids growing legacy "
                f"schema-specific surface after M2; delete the corresponding adapter/DTO/union "
                f"branch instead of adding to it."
            )

    return violations


def run_self_test() -> bool:
    print("[*] Running validate_ui_pipeline_legacy_gate self-test...")

    # 1. Positive test against the actual codebase.
    actual_violations = run_validation(ADAPTER_REGISTRY_PATH, BRIDGE_TYPES_PATH)
    if actual_violations:
        print("FAILED: expected the current codebase to be within baseline, found violations:\n" + "\n".join(actual_violations))
        return False

    # 2. Negative test: an extra Prepare/Build function pushes the count over baseline.
    with tempfile.TemporaryDirectory() as tmpdir:
        fake_root = Path(tmpdir)
        fake_adapter = fake_root / "GV2ScreenFieldAdapterRegistry.cpp"
        adapter_source = ADAPTER_REGISTRY_PATH.read_text(encoding="utf-8")
        fake_adapter.write_text(
            adapter_source + "\nbool PrepareNewWidget(int x) { return true; }\n", encoding="utf-8"
        )

        fake_bridge = fake_root / "GV2BridgeTypes.h"
        fake_bridge.write_text(BRIDGE_TYPES_PATH.read_text(encoding="utf-8"), encoding="utf-8")

        violations = run_validation(fake_adapter, fake_bridge)
        if not violations or not any("prepare_build_functions" in v for v in violations):
            print(f"FAILED: scanner did not catch an added Prepare function. Violations: {violations}")
            return False

    # 3. Negative test: a new schema-specific DTO struct pushes the count over baseline.
    with tempfile.TemporaryDirectory() as tmpdir:
        fake_root = Path(tmpdir)
        fake_adapter = fake_root / "GV2ScreenFieldAdapterRegistry.cpp"
        fake_adapter.write_text(ADAPTER_REGISTRY_PATH.read_text(encoding="utf-8"), encoding="utf-8")

        fake_bridge = fake_root / "GV2BridgeTypes.h"
        bridge_source = BRIDGE_TYPES_PATH.read_text(encoding="utf-8")
        fake_bridge.write_text(
            bridge_source + "\nstruct GV2_API FGV2NewWidgetViewModel\n{\n    int Dummy;\n};\n", encoding="utf-8"
        )

        violations = run_validation(fake_adapter, fake_bridge)
        if not violations or not any("schema_specific_dtos" in v for v in violations):
            print(f"FAILED: scanner did not catch an added schema-specific DTO struct. Violations: {violations}")
            return False

    # 4. Negative test: a new payload UPROPERTY on FGV2ScreenFieldValue pushes the count over baseline.
    with tempfile.TemporaryDirectory() as tmpdir:
        fake_root = Path(tmpdir)
        fake_adapter = fake_root / "GV2ScreenFieldAdapterRegistry.cpp"
        fake_adapter.write_text(ADAPTER_REGISTRY_PATH.read_text(encoding="utf-8"), encoding="utf-8")

        fake_bridge = fake_root / "GV2BridgeTypes.h"
        bridge_source = BRIDGE_TYPES_PATH.read_text(encoding="utf-8")
        marker = "static FGV2ScreenFieldValue MakeButtonList("
        injected = bridge_source.replace(
            marker,
            "UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = \"GV2|UI|Screen\")\n"
            "    FGV2ImageFieldViewModel NewWidgetValue;\n\n    " + marker,
            1,
        )
        if injected == bridge_source:
            print("FAILED: self-test injection marker not found in GV2BridgeTypes.h")
            return False
        fake_bridge.write_text(injected, encoding="utf-8")

        violations = run_validation(fake_adapter, fake_bridge)
        if not violations or not any("screen_field_value_payload_members" in v for v in violations):
            print(f"FAILED: scanner did not catch an added FGV2ScreenFieldValue payload member. Violations: {violations}")
            return False

    print("  PASS: validate_ui_pipeline_legacy_gate self-test succeeded.")
    return True


def main() -> int:
    if "--self-test" in sys.argv:
        return 0 if run_self_test() else 1

    violations = run_validation(ADAPTER_REGISTRY_PATH, BRIDGE_TYPES_PATH)
    if violations:
        print(f"UI pipeline legacy gate failed ({len(violations)} violation(s)):", file=sys.stderr)
        for violation in violations:
            print(violation, file=sys.stderr)
        return 1

    print("UI pipeline legacy gate passed: legacy surface within baseline.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
