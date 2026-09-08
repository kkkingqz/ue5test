#!/usr/bin/env python3
"""Derives the set of IGV2PropertyConsumer kinds from the header's own class
declarations and requires each one's Commit()/CommitWithFailureInjector() to route
through the GV2PresentationApply transaction protocol -- not a task-authored list of
"kinds that must migrate".

PSC-09B (ADR-0043 D2/D3, Payload.md M3): "второго пути, минующего [транзакцию], не
существует ни для одного вида операции" -- exhaustive over
GV2PropertyConsumers.h's own `class GV2_API F...PropertyConsumer : public
IGV2PropertyConsumer` declarations, not a fixed enumeration typed into this gate. A new
consumer class added later is picked up automatically; forgetting to route its own
Commit() through GV2PresentationApply::Apply()/GV2LegacyPresentationApplyAdapter::Apply()
fails this gate the same way an existing one regressing would.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
HEADER = REPO_ROOT / "Source" / "GV2" / "Public" / "UI" / "GV2PropertyConsumers.h"
SOURCE = REPO_ROOT / "Source" / "GV2" / "Private" / "UI" / "GV2PropertyConsumers.cpp"

CLASS_DECLARATION_PATTERN = re.compile(
    r"class\s+GV2_API\s+(?P<name>F\w+PropertyConsumer)\s*:\s*public\s+IGV2PropertyConsumer"
)

TRANSACTION_MARKERS = (
    "GV2PresentationApply::Apply",
    "GV2LegacyPresentationApplyAdapter::Apply",
)


def extract_consumer_class_names(header_text: str) -> list[str]:
    return [match.group("name") for match in CLASS_DECLARATION_PATTERN.finditer(header_text)]


def matching_brace(source: str, open_brace: int) -> int | None:
    depth = 0
    for index in range(open_brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return index
    return None


def find_method_body(source: str, class_name: str, method_name: str) -> str | None:
    """Finds `bool ClassName::method_name(` (or `void ...`) and returns its brace-balanced
    body text, or None if no such definition exists in source."""
    pattern = re.compile(
        r"\b" + re.escape(class_name) + r"::" + re.escape(method_name) + r"\s*\([^;{]*\)\s*(?:const\s*)?\{"
    )
    match = pattern.search(source)
    if match is None:
        return None
    open_brace = source.rindex("{", match.start(), match.end())
    close_brace = matching_brace(source, open_brace)
    if close_brace is None:
        return None
    return source[open_brace : close_brace + 1]


def find_violations(header_text: str, source_text: str) -> list[str]:
    violations: list[str] = []
    class_names = extract_consumer_class_names(header_text)
    if not class_names:
        return ["no `class GV2_API F...PropertyConsumer : public IGV2PropertyConsumer` declarations found"]

    for class_name in class_names:
        commit_body = find_method_body(source_text, class_name, "Commit")
        failure_injector_body = find_method_body(source_text, class_name, "CommitWithFailureInjector")

        bodies_found = [body for body in (commit_body, failure_injector_body) if body is not None]
        if not bodies_found:
            violations.append(f"{class_name}: no Commit() or CommitWithFailureInjector() definition found")
            continue

        if not any(marker in body for body in bodies_found for marker in TRANSACTION_MARKERS):
            violations.append(
                f"{class_name}: neither Commit() nor CommitWithFailureInjector() calls "
                "GV2PresentationApply::Apply(...) or GV2LegacyPresentationApplyAdapter::Apply(...) "
                "-- this kind's physical mutation does not route through the transaction protocol"
            )

    return violations


def validate_repository() -> list[str]:
    if not HEADER.exists():
        return [f"{HEADER}: not found"]
    if not SOURCE.exists():
        return [f"{SOURCE}: not found"]
    return find_violations(HEADER.read_text(encoding="utf-8"), SOURCE.read_text(encoding="utf-8"))


def run_self_test() -> bool:
    print("[*] Running validate_property_consumer_transaction_coverage self-test...")
    if errors := validate_repository():
        print("FAILED: current repository already violates the gate:\n" + "\n".join(errors))
        return False

    # A consumer class whose Commit() never routes through the transaction protocol must
    # be flagged.
    synthetic_header = (
        "class GV2_API FGV2SyntheticPropertyConsumer : public IGV2PropertyConsumer\n"
        "{\n"
        "};\n"
    )
    synthetic_source_bad = (
        "bool FGV2SyntheticPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)\n"
        "{\n"
        "    TargetWidget->SetIsEnabled(true);\n"
        "    return true;\n"
        "}\n"
    )
    errors = find_violations(synthetic_header, synthetic_source_bad)
    if not any("FGV2SyntheticPropertyConsumer" in error for error in errors):
        print(f"FAILED: gate did not flag a Commit() that never calls the transaction protocol: {errors}")
        return False

    # The same class, but with Commit() routing through GV2PresentationApply::Apply, must
    # not be flagged.
    synthetic_source_good = (
        "bool FGV2SyntheticPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)\n"
        "{\n"
        "    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;\n"
        "    return GV2PresentationApply::Apply(Transaction, OutError);\n"
        "}\n"
    )
    errors = find_violations(synthetic_header, synthetic_source_good)
    if errors:
        print(f"FAILED: gate rejected a Commit() that does call GV2PresentationApply::Apply: {errors}")
        return False

    # A class with no Commit()/CommitWithFailureInjector() definition at all must be
    # flagged too (missing, not just non-routing).
    errors = find_violations(synthetic_header, "// no definitions here\n")
    if not any("no Commit()" in error for error in errors):
        print(f"FAILED: gate did not flag a missing Commit() definition: {errors}")
        return False

    # A class whose REAL work lives in CommitWithFailureInjector() (Commit() just
    # delegates to it) must be classified by that function's body, not Commit()'s own.
    synthetic_source_delegate = (
        "bool FGV2SyntheticPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)\n"
        "{\n"
        "    return CommitWithFailureInjector(TargetWidget, OutError, nullptr, FString());\n"
        "}\n"
        "\n"
        "bool FGV2SyntheticPropertyConsumer::CommitWithFailureInjector(\n"
        "    UWidget* TargetWidget, FString& OutError,\n"
        "    const TFunction<bool(const FString&)>& FailureInjector, const FString& PropertyPath)\n"
        "{\n"
        "    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;\n"
        "    return GV2LegacyPresentationApplyAdapter::Apply(Transaction, OutError);\n"
        "}\n"
    )
    errors = find_violations(synthetic_header, synthetic_source_delegate)
    if errors:
        print(f"FAILED: gate rejected a class whose CommitWithFailureInjector() does route through the adapter: {errors}")
        return False

    print("SUCCESS: every IGV2PropertyConsumer kind's Commit routes through the GV2PresentationApply transaction protocol")
    return True


def main(argv: list[str]) -> int:
    if argv == ["--self-test"]:
        return 0 if run_self_test() else 1
    if argv:
        print("usage: validate_property_consumer_transaction_coverage.py [--self-test]", file=sys.stderr)
        return 2

    errors = validate_repository()
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("SUCCESS: every IGV2PropertyConsumer kind's Commit routes through the GV2PresentationApply transaction protocol")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
