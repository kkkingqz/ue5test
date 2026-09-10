#!/usr/bin/env python3
"""Checks that every FGV2UiPropertyCapability member has a classification.

The expected side is deliberately the explicit table below.  The actual side is
derived from the public C++ declaration, so adding or deleting a member cannot
silently make this pass by updating the same source from which it is read.
"""

from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
CAPABILITY_HEADER_PATH = REPO_ROOT / "Source" / "GV2PresentationApply" / "Public" / "UI" / "GV2UiCapability.h"
CAPABILITY_SOURCE_PATHS = (
    REPO_ROOT / "Source" / "GV2PresentationApply" / "Private" / "UI" / "GV2UiCapability.cpp",
    REPO_ROOT / "Source" / "GV2" / "Private" / "UI" / "GV2UiCapability.cpp",
)
STRUCT_SIGNATURE = "struct GV2PRESENTATIONAPPLY_API FGV2UiPropertyCapability"

# This is the independent, explicit classification table.  Its values record why
# each member participates in the capability contract, or why it does not belong
# to IsUiCapabilitySubset's constraint comparison.
CLASSIFIED_MEMBERS = {
    "PropertyName": "identity of the property, not a constraint",
    "SupportedKind": "compared (KindMismatch)",
    "TargetType": "target-resolution shape, not a constraint",
    "TargetName": "physical target identity, not a constraint",
    "ChildCapabilityName": "child selector resolved before subset comparison",
    "TargetKind": "compared (TargetKindMismatch)",
    "IntMin": "compared (IntRangeMismatch)",
    "IntMax": "compared (IntRangeMismatch)",
    "NumberMin": "compared (NumberRangeMismatch)",
    "NumberMax": "compared (NumberRangeMismatch)",
    "bRequiresKeyedIdentity": "compared (KeyedIdentityMismatch)",
    "KeyPropertyName": "compared (KeyPropertyMismatch)",
    "EntryWidgetClass": "compared (EntryWidgetClassMismatch)",
    "ChildTree": "nested schema recursion, not a leaf constraint",
    "ItemCapability": "compared recursively (ItemMismatch)",
}


def strip_comments(source: str) -> str:
    """Preserve layout while excluding braces and semicolons in comments."""

    source = re.sub(r"/\*.*?\*/", lambda match: "\n" * match.group(0).count("\n"), source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", source)


def extract_struct_body(source: str) -> str | None:
    start = source.find(STRUCT_SIGNATURE)
    if start < 0:
        return None

    open_brace = source.find("{", start)
    if open_brace < 0:
        return None

    depth = 0
    for index in range(open_brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[open_brace + 1 : index]
    return None


def extract_data_members(path: Path) -> tuple[list[str], list[str]]:
    if not path.exists():
        return [], [f"{path}: capability header not found"]

    body = extract_struct_body(strip_comments(path.read_text(encoding="utf-8")))
    if body is None:
        return [], [f"{path}: FGV2UiPropertyCapability body not found"]

    members: list[str] = []
    errors: list[str] = []
    statement: list[str] = []
    nested_braces = 0
    for character in body:
        statement.append(character)
        if character == "{":
            nested_braces += 1
        elif character == "}":
            nested_braces -= 1
        elif character == ";" and nested_braces == 0:
            declaration = "".join(statement).strip()[:-1].strip()
            statement.clear()
            before_initializer = declaration.split("=", 1)[0].strip()
            if not declaration or "operator" in declaration or "(" in before_initializer or ")" in before_initializer:
                continue
            match = re.search(r"([A-Za-z_]\w*)\s*$", before_initializer)
            if match is None:
                errors.append(f"{path}: cannot extract capability data member from '{declaration}'")
                continue
            members.append(match.group(1))

    return members, errors


def validate_header(path: Path) -> list[str]:
    members, errors = extract_data_members(path)
    if errors:
        return errors

    actual = set(members)
    classified = set(CLASSIFIED_MEMBERS)
    unknown = sorted(actual - classified)
    missing = sorted(classified - actual)
    if unknown:
        errors.append(
            f"{path}: unclassified FGV2UiPropertyCapability member(s): {', '.join(unknown)}"
        )
    if missing:
        errors.append(
            f"{path}: classification table member(s) absent from FGV2UiPropertyCapability: {', '.join(missing)}"
        )
    return errors


def validate_repository() -> list[str]:
    errors = validate_header(CAPABILITY_HEADER_PATH)
    for source_path in CAPABILITY_SOURCE_PATHS:
        if not source_path.exists():
            errors.append(f"{source_path}: capability source not found")
            continue
        source = strip_comments(source_path.read_text(encoding="utf-8"))
        if re.search(r"\bsizeof\s*\(\s*FGV2UiPropertyCapability\s*\)", source):
            errors.append(
                f"{source_path}: sizeof(FGV2UiPropertyCapability) is not a completeness gate; "
                "use the source member inventory"
            )
    return errors


def run_self_test() -> bool:
    print("[*] Running validate_ui_capability_member_inventory self-test...")
    actual_errors = validate_header(CAPABILITY_HEADER_PATH)
    if actual_errors:
        print("FAILED: current capability header violates the inventory:\n" + "\n".join(actual_errors))
        return False

    with tempfile.TemporaryDirectory() as tmpdir:
        temporary_header = Path(tmpdir) / "GV2UiCapability.h"
        production_header = CAPABILITY_HEADER_PATH.read_text(encoding="utf-8")

        synthetic_member = production_header.replace(
            "    bool bRequiresKeyedIdentity = false;\n",
            "    bool bRequiresKeyedIdentity = false;\n    bool bSyntheticUnknownMember = false;\n",
            1,
        )
        temporary_header.write_text(synthetic_member, encoding="utf-8")
        errors = validate_header(temporary_header)
        if not any("bSyntheticUnknownMember" in error for error in errors):
            print(f"FAILED: gate accepted synthetic unclassified member: {errors}")
            return False

        missing_member = production_header.replace("    FString PropertyName;\n", "", 1)
        temporary_header.write_text(missing_member, encoding="utf-8")
        errors = validate_header(temporary_header)
        if not any("PropertyName" in error and "absent" in error for error in errors):
            print(f"FAILED: gate accepted deleted classified member: {errors}")
            return False

    print("SUCCESS: capability member inventory rejects unclassified additions and deleted classifications")
    return True


def main(argv: list[str]) -> int:
    if argv == ["--self-test"]:
        return 0 if run_self_test() else 1
    if argv:
        print("usage: validate_ui_capability_member_inventory.py [--self-test]", file=sys.stderr)
        return 2

    errors = validate_repository()
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("SUCCESS: every FGV2UiPropertyCapability member has an explicit classification")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
