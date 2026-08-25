#!/usr/bin/env python3
"""Monotonic decrease gate for the Universal UI Property Pipeline migration (UPP-11).

Counts three legacy-surface metrics that the migration (UPP-12..30) must only ever
shrink, never grow, once a widget starts migrating to the universal pipeline:

  1. Schema-specific Prepare/Build functions in GV2ScreenFieldMaterializer.cpp.
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
ADAPTER_REGISTRY_PATH = REPO_ROOT / "Source" / "GV2" / "Private" / "Application" / "GV2ScreenFieldMaterializer.cpp"
BRIDGE_TYPES_PATH = REPO_ROOT / "Source" / "GV2" / "Public" / "Bridge" / "GV2BridgeTypes.h"

# Upper bounds captured at UPP-11 (2026-08-24). Lowering one of these is how a widget
# migration change set proves it actually deleted its legacy adapter/DTO/union branch,
# per the plan's rule 2 ("удаляет свой PrepareXxx/BuildXxx/DTO/ветку union").
#
# screen_field_value_payload_members raised 0 -> 2 at UPP-27 (2026-08-25): PreparedValue
# and CompiledSchema are the materialized-candidate carrier UGV2ScreenWidgetBase's
# Prepare/Commit consumes -- unlike the schema-specific union members this gate exists to
# shrink, both fields are the *same* generic type for every schema_id (TSharedPtr<const
# FGV2PreparedUiObject>, GV2ContentCore::FCompiledUiFieldSpecPtr), so growth here is the
# generic pipeline's own load-bearing state, not a reappearing legacy transport.
# schema_specific_dtos dropped 4 -> 0 at UPP-30 (2026-08-25): FGV2ButtonViewModel and
# FGV2ProgressBarViewModel were deleted along with the legacy ApplyButtonModels/
# ApplyProgressBarModel methods that were their only callers (REV3-01/02/05's tests now
# exercise the same widgets through PrepareUiHostProperties/CommitUiHostProperties
# directly); FGV2RichTextHoverViewModel/FGV2RichTextSpanViewModel moved to
# INFRA_STRUCT_ALLOWLIST below since they were never legacy-transport DTOs in the first
# place. This is now a permanent zero -- UPP-30's Done requires all three counters here
# to reach zero and become a standing prohibition, not just a lower number.
BASELINES = {
    "prepare_build_functions": 0,
    "screen_field_value_payload_members": 2,
    "schema_specific_dtos": 0,
}

PREPARE_BUILD_PATTERN = re.compile(r"^bool ((?:Prepare|Build)[A-Z][A-Za-z0-9_]*)\(", re.MULTILINE)

# GV2ScreenFieldMaterializer.cpp's two permanent, schema-agnostic entry points
# (UPP-30): unlike a schema-specific PrepareLocationTopBar/BuildButtonList (one per
# widget type -- the debt this gate tracks), these walk whatever compiled schema a
# caller passes them, so their count never grows with the number of widget types.
PREPARE_BUILD_ALLOWLIST = {"PrepareBindingDefinitions", "BuildFields"}

# FGV2ScreenFieldValue's identity fields (not payload).
SCREEN_FIELD_VALUE_IDENTITY_FIELDS = 2  # FieldId, SchemaId
SCREEN_FIELD_VALUE_UNREFLECTED_PAYLOAD_MEMBERS: list[str] = ["PreparedValue", "CompiledSchema"]

STRUCT_PATTERN = re.compile(r"^struct GV2_API ([A-Za-z0-9_]+)", re.MULTILINE)
UPROPERTY_PATTERN = re.compile(r"UPROPERTY\(")

# Generic/envelope structs GV2BridgeTypes.h keeps permanently: not schema-specific DTOs,
# so they are excluded from the count this gate tracks. Adding a new schema-specific
# ViewModel struct is not exempt by editing this set -- only genuinely generic,
# domain-agnostic infrastructure belongs here.
#
# FGV2RichTextHoverViewModel/FGV2RichTextSpanViewModel joined at UPP-30 (2026-08-25):
# unlike FGV2ButtonViewModel/FGV2ProgressBarViewModel (deleted at UPP-30 along with the
# legacy ApplyButtonModels/ApplyProgressBarModel methods that were their only callers),
# these two are the *active* production pipeline's own internal representation --
# FGV2RichTextSpansPropertyConsumer::PreparedSpans (GV2PropertyConsumers.h) is what
# PrepareUiHostProperties/CommitUiHostProperties actually build and commit for RichText's
# "spans" capability, not a payload left over from the retired transport.
INFRA_STRUCT_ALLOWLIST = {
    "FGV2UiBindingHandle",
    "FGV2UiControlValue",
    "FGV2TextViewModel",
    "FGV2RichTextHoverViewModel",
    "FGV2RichTextSpanViewModel",
    "FGV2ScreenFieldValue",
    "FGV2ScreenViewModel",
    "FGV2ScreenInstanceViewModel",
    "FGV2UiDocumentViewModel",
    "FGV2SessionStatus",
}


def count_prepare_build_functions(source: str) -> int:
    return len([name for name in PREPARE_BUILD_PATTERN.findall(source) if name not in PREPARE_BUILD_ALLOWLIST])


def count_screen_field_value_payload_members(source: str) -> int:
    struct_start = source.index("struct GV2_API FGV2ScreenFieldValue")
    if "static FGV2ScreenFieldValue Make" in source[struct_start:]:
        end_idx = source.index("static FGV2ScreenFieldValue Make", struct_start)
    else:
        end_idx = source.index("};", struct_start)
    body = source[struct_start:end_idx]
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
        fake_adapter = fake_root / "GV2ScreenFieldMaterializer.cpp"
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
        fake_adapter = fake_root / "GV2ScreenFieldMaterializer.cpp"
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
        fake_adapter = fake_root / "GV2ScreenFieldMaterializer.cpp"
        fake_adapter.write_text(ADAPTER_REGISTRY_PATH.read_text(encoding="utf-8"), encoding="utf-8")

        fake_bridge = fake_root / "GV2BridgeTypes.h"
        bridge_source = BRIDGE_TYPES_PATH.read_text(encoding="utf-8")
        marker = "    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = \"GV2|UI|Screen\")\n    FString SchemaId;"
        injected = bridge_source.replace(
            marker,
            marker + "\n\n    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = \"GV2|UI|Screen\")\n    FGV2ButtonViewModel NewWidgetValue;",
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
