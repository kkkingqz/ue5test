#!/usr/bin/env python3
"""Validates that every field declared on a GV2PresentationApply prepared-operation
struct is a plain, self-contained value -- never a function/callback, a pointer or
reference to a resolver/context/repository/registry/theme source, or a generic untyped
service handle.

PSC-09B (ADR-0043 D2/D3): a prepared operation carries a resolved value, not a way to
obtain one. Source-scanning here is a SECOND, weaker rubric -- the primary guarantee is
GV2PresentationApply.Build.cs's own dependency denylist (validate_presentation_apply_
module_graph.py), which makes it structurally impossible to declare a field of a
GV2/GV2ContentCore/GV2ContentHostSupport/GV2RuntimeCore type at all. This gate exists for
the narrower, denylist-independent claim ADR-0043 D3 makes explicitly: even a field whose
type COULD be declared here (a callback closure capturing GV2 state by value, a raw
UObject* used as a generic handle, a soft/lazy reference resolved later) must not be, and
the Build.cs graph alone does not forbid those on its own.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
TRANSACTION_HEADER = (
    REPO_ROOT
    / "Source"
    / "GV2PresentationApply"
    / "Public"
    / "GV2PresentationApply"
    / "PreparedPresentationTransaction.h"
)

STRUCT_PATTERN = re.compile(
    r"struct\s+GV2PRESENTATIONAPPLY_API\s+(?P<name>F\w+)\s*\{(?P<body>.*?)\n\};",
    re.DOTALL,
)
ENUM_PATTERN = re.compile(r"enum\s+class\s+(?P<name>E\w+)\s*:")
# PSC-10B: a payload variant alias (`using FX = TVariant<A, B, C>;`). It is NOT accepted on
# the strength of being declared locally -- that would turn any variant alias into an
# unchecked escape hatch for exactly the field types this gate rejects. Each alternative
# must itself classify as safe, and every alternative that is a local struct is separately
# walked field-by-field by find_violations.
VARIANT_ALIAS_PATTERN = re.compile(
    r"using\s+(?P<name>F\w+)\s*=\s*TVariant<(?P<args>.*?)>\s*;",
    re.DOTALL,
)
FIELD_PATTERN = re.compile(
    r"^\s*(?P<type>[A-Za-z_].*?)\s+(?P<name>\w+)\s*(?:=\s*[^;]+)?;\s*$",
    re.MULTILINE,
)

# Explicit denylist, matched against the RAW type text (before generic-argument
# stripping) so "TArray<FPreparedX>" and "TWeakObjectPtr<UGV2Foo>" are both caught by the
# same substring rule. Reported by name, not just "not on the allowlist", so a violation
# names the actual rule broken.
FORBIDDEN_SUBSTRINGS = (
    "TFunction",
    "PrepareContext",
    "Repository",
    "PackageSet",
    "ScreenRegistry",
    "Registry",
    "Theme",
    "Session",
    "Snapshot",
    "void *",
    "void*",
)

# Base (non-template) types that are always safe: plain values with no notion of a
# resolver, authority, or deferred lookup.
ALLOWED_BASE_TYPES = {
    "bool",
    "float",
    "double",
    "int32",
    "int64",
    "uint8",
    "FString",
    "FName",
    "FText",
    "FSlateBrush",
    "FTextBlockStyle",
    "FRuntimeFloatCurve",
    # PSC-10B: plain geometry/colour values a central-style role carries. Same class as
    # FSlateBrush above -- a finished value with no notion of a lookup.
    "FLinearColor",
    "FMargin",
    "FProgressBarStyle",
    "FCheckBoxStyle",
    "FEditableTextBoxStyle",
}

# UObject-derived types a TWeakObjectPtr<...> field may point to. Structurally, this
# module's own Build.cs denylist already makes a GV2-owned class impossible to name here
# at all -- this whitelist is the narrower, denylist-independent claim: only these
# specific plain Engine/UMG widget types are expected, not "any non-GV2 UObject".
ALLOWED_WEAK_OBJECT_TARGETS = {
    "UWidget",
    "UImage",
    "UCheckBox",
    "UEditableTextBox",
    "UProgressBar",
    "UCommonTextBlock",
    "UCommonRichTextBlock",
}

# UObject-derived class types a TSubclassOf<...> field may reference. Same rationale as
# ALLOWED_WEAK_OBJECT_TARGETS above -- the Build.cs denylist already forbids naming a
# GV2-owned class here, this is the narrower explicit allowlist of plain CommonUI types.
ALLOWED_SUBCLASS_TARGETS = {
    "UCommonTextStyle",
    # PSC-10B: CommonUI's own button style class, same category as UCommonTextStyle above.
    "UCommonButtonStyle",
}


def extract_locally_declared_types(source: str) -> set[str]:
    names = {match.group("name") for match in STRUCT_PATTERN.finditer(source)}
    names |= {match.group("name") for match in ENUM_PATTERN.finditer(source)}
    return names


def extract_safe_variant_aliases(source: str, local_types: set[str]) -> tuple[set[str], list[str]]:
    """Variant aliases whose every alternative is itself classified safe, plus the
    reasons for any alias that is not."""
    safe: set[str] = set()
    reasons: list[str] = []
    for match in VARIANT_ALIAS_PATTERN.finditer(source):
        name = match.group("name")
        args = re.sub(r"//[^\n]*", "", match.group("args"))
        alternatives = [arg.strip() for arg in args.split(",") if arg.strip()]
        unsafe = [
            alt for alt in alternatives
            if alt not in local_types and alt not in ALLOWED_BASE_TYPES
        ]
        if unsafe:
            reasons.append(
                f"variant alias '{name}' has unclassified alternative(s): {', '.join(unsafe)}"
            )
        else:
            safe.add(name)
    return safe, reasons


def classify_field(raw_type: str, field_name: str, local_types: set[str]) -> str | None:
    """Returns None if the field is allowed, else a violation reason string."""
    type_text = raw_type.strip()

    for forbidden in FORBIDDEN_SUBSTRINGS:
        if forbidden in type_text:
            return f"type matches forbidden pattern '{forbidden}'"
        if forbidden in field_name:
            return f"field name matches forbidden pattern '{forbidden}'"

    if type_text in ALLOWED_BASE_TYPES:
        return None

    if type_text in local_types:
        return None

    weak_match = re.fullmatch(r"TWeakObjectPtr\s*<\s*(?P<inner>\w+)\s*>", type_text)
    if weak_match:
        inner = weak_match.group("inner")
        if inner in ALLOWED_WEAK_OBJECT_TARGETS:
            return None
        return (
            f"TWeakObjectPtr<{inner}> -- '{inner}' is not in the allowed plain "
            "Engine/UMG widget target list; classify it explicitly"
        )

    subclass_match = re.fullmatch(r"TSubclassOf\s*<\s*(?P<inner>\w+)\s*>", type_text)
    if subclass_match:
        inner = subclass_match.group("inner")
        if inner in ALLOWED_SUBCLASS_TARGETS:
            return None
        return (
            f"TSubclassOf<{inner}> -- '{inner}' is not in the allowed plain "
            "Engine/UMG class target list; classify it explicitly"
        )

    array_match = re.fullmatch(r"TArray\s*<\s*(?P<inner>[\w:]+)\s*>", type_text)
    if array_match:
        inner = array_match.group("inner")
        if inner in local_types or inner in ALLOWED_BASE_TYPES:
            return None
        return f"TArray<{inner}> -- element type '{inner}' is not itself classified as safe"

    return f"unclassified type '{type_text}' -- not in the allowlist and not a locally-declared struct/enum"


def find_violations(source: str) -> list[str]:
    violations: list[str] = []
    local_types = extract_locally_declared_types(source)
    safe_aliases, alias_reasons = extract_safe_variant_aliases(source, local_types)
    violations.extend(alias_reasons)
    local_types = local_types | safe_aliases

    for struct_match in STRUCT_PATTERN.finditer(source):
        struct_name = struct_match.group("name")
        body = struct_match.group("body")
        # Strip comments so a mention of a forbidden word in prose never trips the gate.
        body_no_comments = re.sub(r"//[^\n]*", "", body)
        body_no_comments = re.sub(r"/\*.*?\*/", "", body_no_comments, flags=re.DOTALL)

        for field_match in FIELD_PATTERN.finditer(body_no_comments):
            field_type = field_match.group("type")
            field_name = field_match.group("name")
            reason = classify_field(field_type, field_name, local_types)
            if reason is not None:
                violations.append(f"{struct_name}::{field_name} ({field_type}): {reason}")

    return violations


def validate_repository() -> list[str]:
    if not TRANSACTION_HEADER.exists():
        return [f"{TRANSACTION_HEADER}: not found"]
    return find_violations(TRANSACTION_HEADER.read_text(encoding="utf-8"))


def run_self_test() -> bool:
    print("[*] Running validate_presentation_apply_field_inventory self-test...")
    if errors := validate_repository():
        print("FAILED: current repository already violates the gate:\n" + "\n".join(errors))
        return False

    # A callback field must be rejected.
    synthetic_callback = """
struct GV2PRESENTATIONAPPLY_API FPreparedSyntheticOperation
{
    TFunction<void()> OnApplied;
};
"""
    errors = find_violations(synthetic_callback)
    if not any("TFunction" in error for error in errors):
        print(f"FAILED: gate did not reject a TFunction callback field: {errors}")
        return False

    # A PrepareContext pointer must be rejected.
    synthetic_context = """
struct GV2PRESENTATIONAPPLY_API FPreparedSyntheticOperation
{
    const void* PrepareContext = nullptr;
};
"""
    errors = find_violations(synthetic_context)
    if not any("PrepareContext" in error for error in errors):
        print(f"FAILED: gate did not reject a PrepareContext-named field: {errors}")
        return False

    # A raw UObject* generic service handle must be rejected (not on the allowlist, and
    # not a TWeakObjectPtr<> either).
    synthetic_handle = """
struct GV2PRESENTATIONAPPLY_API FPreparedSyntheticOperation
{
    UObject* GenericHandle = nullptr;
};
"""
    errors = find_violations(synthetic_handle)
    if not any("GenericHandle" in error for error in errors):
        print(f"FAILED: gate did not reject a raw UObject* generic service handle: {errors}")
        return False

    # A TWeakObjectPtr to an unclassified (non-allowlisted) UObject type must be rejected.
    synthetic_weak = """
struct GV2PRESENTATIONAPPLY_API FPreparedSyntheticOperation
{
    TWeakObjectPtr<USomeFutureWidget> TargetWidget;
};
"""
    errors = find_violations(synthetic_weak)
    if not any("USomeFutureWidget" in error for error in errors):
        print(f"FAILED: gate did not reject an unclassified TWeakObjectPtr target: {errors}")
        return False

    # A TSubclassOf to an unclassified (non-allowlisted) class type must be rejected.
    synthetic_subclass = """
struct GV2PRESENTATIONAPPLY_API FPreparedSyntheticOperation
{
    TSubclassOf<USomeFutureStyle> Style;
};
"""
    errors = find_violations(synthetic_subclass)
    if not any("USomeFutureStyle" in error for error in errors):
        print(f"FAILED: gate did not reject an unclassified TSubclassOf target: {errors}")
        return False

    # A field naming Repository/Registry/Theme must be rejected even without a pointer.
    synthetic_authority = """
struct GV2PRESENTATIONAPPLY_API FPreparedSyntheticOperation
{
    FString ThemeName;
};
"""
    errors = find_violations(synthetic_authority)
    if not any("ThemeName" in error for error in errors):
        print(f"FAILED: gate did not reject a Theme-named field: {errors}")
        return False

    # PSC-10A: a violation buried inside a NESTED struct (referenced from a container via
    # TArray<...>, the same shape FPreparedTabEntry/FPreparedKeyedCollectionEntry use)
    # must be caught too -- not just violations at the outer struct's own top level. The
    # gate scans every GV2PRESENTATIONAPPLY_API struct declared in the file independently
    # (STRUCT_PATTERN.finditer over the whole source), so a nested struct is caught
    # because it is ALSO scanned directly, not skipped as "somebody else's problem".
    synthetic_recursive_violation = """
struct GV2PRESENTATIONAPPLY_API FPreparedSyntheticNestedBad
{
    FString Value;
    TFunction<void()> OnResolved;
};

struct GV2PRESENTATIONAPPLY_API FPreparedSyntheticContainer
{
    TArray<FPreparedSyntheticNestedBad> Items;
};
"""
    errors = find_violations(synthetic_recursive_violation)
    if not any("FPreparedSyntheticNestedBad" in error and "TFunction" in error for error in errors):
        print(f"FAILED: gate did not catch a callback field buried inside a nested (TArray-referenced) struct: {errors}")
        return False

    # PSC-10B: a variant alias must not launder an unclassified alternative. The alias is
    # accepted only when every alternative is itself classified, so this synthetic -- whose
    # alias names a type declared nowhere -- must be reported, and the field typed with that
    # alias must not silently pass on the strength of the alias being declared locally.
    synthetic_variant_launder = """
struct GV2PRESENTATIONAPPLY_API FPreparedSyntheticRole
{
    FString Value;
};

using FPreparedSyntheticPayload = TVariant<
    FPreparedSyntheticRole,
    FSomeUnclassifiedResolverHandle
>;

struct GV2PRESENTATIONAPPLY_API FPreparedSyntheticStyleOperation
{
    FPreparedSyntheticPayload Payload;
};
"""
    errors = find_violations(synthetic_variant_launder)
    if not any("FSomeUnclassifiedResolverHandle" in error for error in errors):
        print(f"FAILED: gate did not reject a variant alias hiding an unclassified alternative: {errors}")
        return False
    if not any("FPreparedSyntheticStyleOperation::Payload" in error for error in errors):
        print(f"FAILED: gate accepted a field typed with an unsafe variant alias: {errors}")
        return False

    # A variant alias whose alternatives are all locally declared must be accepted, and a
    # field typed with it must produce no violation.
    synthetic_variant_ok = """
struct GV2PRESENTATIONAPPLY_API FPreparedSyntheticRoleA
{
    FString Value;
};

struct GV2PRESENTATIONAPPLY_API FPreparedSyntheticRoleB
{
    float Amount = 0.0f;
};

using FPreparedSyntheticOkPayload = TVariant<
    FPreparedSyntheticRoleA,
    FPreparedSyntheticRoleB
>;

struct GV2PRESENTATIONAPPLY_API FPreparedSyntheticOkOperation
{
    FPreparedSyntheticOkPayload Payload;
};
"""
    errors = find_violations(synthetic_variant_ok)
    if errors:
        print(f"FAILED: gate rejected a variant alias whose alternatives are all classified: {errors}")
        return False

    # A fully-allowed synthetic struct (plain values, a locally-declared nested struct,
    # and an array of it) must produce zero violations.
    synthetic_allowed = """
struct GV2PRESENTATIONAPPLY_API FPreparedSyntheticNested
{
    FString Value;
};

struct GV2PRESENTATIONAPPLY_API FPreparedSyntheticOperation
{
    TWeakObjectPtr<UWidget> TargetWidget;
    FString Value;
    bool bFlag = false;
    TArray<FPreparedSyntheticNested> Nested;
};
"""
    errors = find_violations(synthetic_allowed)
    if errors:
        print(f"FAILED: gate rejected an entirely allowed synthetic struct: {errors}")
        return False

    print("SUCCESS: every GV2PresentationApply prepared-operation field is a plain, self-contained value")
    return True


def main(argv: list[str]) -> int:
    if argv == ["--self-test"]:
        return 0 if run_self_test() else 1
    if argv:
        print("usage: validate_presentation_apply_field_inventory.py [--self-test]", file=sys.stderr)
        return 2

    errors = validate_repository()
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("SUCCESS: every GV2PresentationApply prepared-operation field is a plain, self-contained value")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
