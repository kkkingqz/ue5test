#!/usr/bin/env python3
"""Checks that every declaration receiving a FGV2PresentationPrepareContext (as a function
parameter or a class field) has an explicit classification.

The expected side is the independent table below. The actual side is derived from
Source/GV2/Public/**/*.h's own declarations, so a new/removed site cannot silently pass by
updating the same source this check reads from.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SCAN_ROOT = REPO_ROOT / "Source" / "GV2" / "Public"
TYPE_NAME = "FGV2PresentationPrepareContext"
FORWARD_DECLARATION_PATTERN = re.compile(r"^\s*class\s+" + TYPE_NAME + r"\s*;\s*$")

# PSC-06 (ADR-0043 D1): the independent classification table -- one entry per source line
# (relative path + trimmed line text) where FGV2PresentationPrepareContext appears as a
# parameter or field type. New sites are expected to grow this table as more Prepare-phase
# call sites migrate off their legacy accessors (PSC-10's job, not a fixed set).
CLASSIFIED_SITES = {
    ("UI/GV2ScreenWidgetBase.h", "const FGV2PresentationPrepareContext* PrepareContext = nullptr) const;"):
        "UGV2ScreenWidgetBase::PrepareScreenFields parameter",
    ("UI/GV2ScreenWidgetBase.h", "const FGV2PresentationPrepareContext& PrepareContext);"):
        "UGV2ScreenWidgetBase::ApplyScreenFields requires explicit pinned authority for its one-shot Prepare+Commit path",
    ("UI/GV2ScreenWidgetBase.h", "const FGV2PresentationPrepareContext& PrepareContext) const;"):
        "UGV2ScreenWidgetBase::CanApplyScreenFields requires explicit pinned authority for preflight",
    ("UI/GV2LayeredUiReconciler.h", "const FGV2PresentationPrepareContext& PrepareContext) const;"):
        "PSC-10B: FGV2LayeredUiReconciler::PrepareReconcile parameter -- REQUIRED, by reference. "
        "Since no widget resolves a Theme of its own, a Prepare without the session snapshot "
        "would publish a physically correct but entirely unstyled tree, so the optional "
        "pointer form is deliberately no longer expressible",
    ("UI/GV2LayeredUiReconciler.h", "const FGV2PresentationPrepareContext& PrepareContext,"):
        "PSC-10B: FGV2LayeredUiReconciler::Reconcile parameter -- same requirement as "
        "PrepareReconcile above; it precedes the optional test-only failure injectors",
    ("UI/GV2LayeredUiReconciler.h", "const FGV2PresentationPrepareContext& PrepareContext);"):
        "PSC-10B: FGV2LayeredUiReconciler::PerformCatastrophicRecovery parameter -- recovery "
        "replays an ordinary fresh Prepare/Apply and therefore needs the SAME context as the "
        "reconcile that triggered it; the old nullptr default here left the rebuilt tree unstyled",
    ("UI/GV2PropertyConsumers.h", "virtual void SetPrepareContext(const FGV2PresentationPrepareContext* InContext) {}"):
        "IGV2PropertyConsumer::SetPrepareContext base (no-op default)",
    ("UI/GV2PropertyConsumers.h", "virtual void SetPrepareContext(const FGV2PresentationPrepareContext* InContext) override { PrepareContext = InContext; }"):
        "consumer override storing the context for recursive/session-owned resolution (four classes share this line: image resource, keyed collection, RichText spans and nested tabs)",
    ("UI/GV2PropertyConsumers.h", "const FGV2PresentationPrepareContext* PrepareContext = nullptr;"):
        "consumer's stored context field (four classes share this line: image resource, keyed collection, RichText spans and nested tabs)",
    ("UI/GV2UiMutationPlan.h", "const FGV2PresentationPrepareContext* PrepareContext = nullptr);"):
        "PrepareUiHostProperties and PrepareUiHostRollbackPlan parameters",
    ("UI/GV2TextPipeline.h", "const FGV2PresentationPrepareContext* PrepareContext);"):
        "PSC-10B: UGV2TextPipeline::Resolve requires the session context and has no context-free runtime fallback",
    ("UI/GV2CentralStylePreparer.h", "const FGV2PresentationPrepareContext& PrepareContext,"):
        "PSC-10B: GV2CentralStylePreparer::PrepareForSubtree parameter -- the only place a Theme is read on behalf of a styled widget; turns it into central-style operations on the caller's transaction. By reference, not pointer: unlike the Prepare paths above there is no legacy no-context call site to keep working, so a caller without a snapshot cannot reach this function at all",
    ("UI/GV2UiCapabilityObservability.h", "const FGV2PresentationPrepareContext& PrepareContext,"):
        "PSC-10B: observability probes execute the real property Prepare path against the caller's pinned snapshot instead of a global test catalog",
}


def strip_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", lambda match: "\n" * match.group(0).count("\n"), source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", source)


def find_sites(headers: dict[Path, str]) -> dict[tuple[str, str], list[str]]:
    """Maps (relative_path, trimmed_line) -> list of "path:line_no" occurrences."""
    sites: dict[tuple[str, str], list[str]] = {}
    for path, source in headers.items():
        stripped = strip_comments(source)
        rel_path = str(path.relative_to(SCAN_ROOT)).replace("\\", "/")
        for line_no, line in enumerate(stripped.splitlines(), start=1):
            if TYPE_NAME not in line:
                continue
            if FORWARD_DECLARATION_PATTERN.match(line):
                continue
            key = (rel_path, line.strip())
            sites.setdefault(key, []).append(f"{rel_path}:{line_no}")
    return sites


def collect_headers() -> dict[Path, str]:
    headers: dict[Path, str] = {}
    if not SCAN_ROOT.exists():
        return headers
    for path in sorted(SCAN_ROOT.rglob("*.h")):
        if path.is_file():
            headers[path] = path.read_text(encoding="utf-8")
    return headers


def find_violations(headers: dict[Path, str]) -> list[str]:
    sites = find_sites(headers)
    violations = []
    seen_keys = set()
    for key, occurrences in sites.items():
        seen_keys.add(key)
        if key not in CLASSIFIED_SITES:
            rel_path, line_text = key
            violations.append(
                f"{occurrences[0]}: '{line_text}' receives {TYPE_NAME} but has no entry in "
                "CLASSIFIED_SITES -- add its classification to validate_prepare_context_inventory.py"
            )
    for key in CLASSIFIED_SITES:
        if key not in seen_keys:
            rel_path, line_text = key
            violations.append(
                f"CLASSIFIED_SITES names '{rel_path}': '{line_text}', which no longer appears in "
                "the scanned headers -- remove the stale classification entry"
            )
    return violations


def validate_repository() -> list[str]:
    return find_violations(collect_headers())


def run_self_test() -> bool:
    print("[*] Running validate_prepare_context_inventory self-test...")
    if errors := validate_repository():
        print("FAILED: current production headers violate the gate:\n" + "\n".join(errors))
        return False

    headers = collect_headers()
    synthetic_path = SCAN_ROOT / "UI" / "__SyntheticPrepareContextConsumer.h"
    headers[synthetic_path] = (
        "#pragma once\n"
        "class FGV2PresentationPrepareContext;\n"
        "class FSyntheticConsumer\n"
        "{\n"
        "public:\n"
        f"    void SyntheticPrepare(const {TYPE_NAME}* SyntheticUnclassifiedParam);\n"
        "};\n"
    )
    errors = find_violations(headers)
    if not any("SyntheticUnclassifiedParam" in error for error in errors):
        print(f"FAILED: gate accepted an unclassified synthetic PrepareContext parameter: {errors}")
        return False

    print(f"SUCCESS: every declaration receiving {TYPE_NAME} has an explicit classification")
    return True


def main(argv: list[str]) -> int:
    if "--self-test" in argv:
        return 0 if run_self_test() else 1

    if errors := validate_repository():
        print("FAILED:\n" + "\n".join(errors))
        return 1

    print(f"SUCCESS: every declaration receiving {TYPE_NAME} has an explicit classification")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
