#!/usr/bin/env python3
"""PSC-11 (ADR-0043 D2, ApplyBoundary.md): what PSC-12 will have to carry, classified now.

PSC-12 moves the physical widget bases into GV2PresentationApply in ONE change set. Whether
that is possible is decided by their include closure, not by the move itself: an include that
names an authority-aware GV2 type cannot follow them down, and discovering that during the
migration means an interrupted move.

So the closure is classified here, mechanically, before the move:

* the movable set is DERIVED -- a widget base is movable exactly when it performs at least
  one prepared role interface, which is what makes it a physical apply target. It is not a
  list someone maintains;
* each of its GV2-side includes is classified as either a value/physical header that can move
  with it, or an authority-aware header that must stay in GV2 behind the DTO boundary;
* the authority-aware set is recorded as a BASELINE. It may shrink -- that is PSC-12 making
  progress -- and may not grow: a new authority-aware include on a movable class is new work
  added to a migration that is supposed to be shrinking.

The baseline is a snapshot of a real measurement, not a wish. It is why this gate is honest
about being a ratchet rather than a proof.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
PUBLIC_UI = REPO_ROOT / "Source" / "GV2" / "Public" / "UI"

ROLE_INTERFACE = re.compile(r"public\s+(IGV2Prepared\w+Target)\b")
INCLUDE = re.compile(r'#include\s+"([^"]+)"')
GV2_PREFIXES = ("UI/", "Bridge/", "Application/", "Runtime/", "Tests/")

# Headers a movable widget base may keep including: they carry values, physical interfaces
# or capability descriptions, and none of them can reach a snapshot, catalog, registry or
# Theme source.
VALUE_OR_PHYSICAL_HEADERS = {
    "UI/GV2PreparedUiValue.h",
    "UI/GV2UiCapability.h",
    "UI/GV2UiStyleConsumer.h",
    "UI/GV2ScreenFieldHost.h",
    "UI/GV2TextPipelineHost.h",
    "UI/GV2UiBindingTarget.h",
    "UI/GV2UiPropertyHost.h",
}

# The measured authority-aware closure, 2026-09-09. May shrink; may not grow.
AUTHORITY_AWARE_BASELINE = {
    "Bridge/GV2BridgeTypes.h",
    "UI/GV2ImageResourceCatalog.h",
    "UI/GV2PropertyConsumers.h",
    "UI/GV2UiInteractionEmitter.h",
}


def movable_widget_bases() -> dict[str, str]:
    """Header name -> source, for every widget base that performs a prepared role."""
    result: dict[str, str] = {}
    for path in sorted(PUBLIC_UI.glob("*.h")):
        source = path.read_text(encoding="utf-8")
        if ROLE_INTERFACE.search(source):
            result[path.name] = source
    return result


def authority_aware_closure(bases: dict[str, str]) -> dict[str, set[str]]:
    """Include -> the movable bases that name it, for GV2-side includes that are not
    classified as value/physical."""
    closure: dict[str, set[str]] = {}
    for name, source in bases.items():
        # Re-derived here, not assumed from the caller: only a class that performs a prepared
        # role is one PSC-12 moves, so only its closure is this gate's business.
        if not ROLE_INTERFACE.search(source):
            continue
        for include in INCLUDE.findall(source):
            if not include.startswith(GV2_PREFIXES):
                continue
            if include in VALUE_OR_PHYSICAL_HEADERS:
                continue
            closure.setdefault(include, set()).add(name)
    return closure


def find_violations(bases: dict[str, str], baseline: set[str] | None = None) -> list[str]:
    errors: list[str] = []
    if not bases:
        return ["the movable-widget-base enumerator produced an empty set; the derivation is broken"]

    expected = AUTHORITY_AWARE_BASELINE if baseline is None else baseline
    closure = authority_aware_closure(bases)
    grown = sorted(set(closure) - expected)
    for include in grown:
        errors.append(
            f"{include}: authority-aware include newly reached by movable widget base(s) "
            f"{sorted(closure[include])} -- PSC-12's closure may shrink, not grow"
        )

    # A header classified as value/physical must actually stay free of the authority types
    # the Apply module may not name; otherwise the classification is a label, not a fact.
    for header in sorted(VALUE_OR_PHYSICAL_HEADERS):
        path = REPO_ROOT / "Source" / "GV2" / "Public" / header
        if not path.exists():
            errors.append(f"{header}: classified as value/physical but does not exist")
            continue
        text = path.read_text(encoding="utf-8")
        text = re.sub(r"//[^\n]*", "", text)
        for forbidden in ("FGV2PresentationPrepareContext", "FGV2SessionContentSnapshot",
                          "UGV2ImageResourceCatalog", "UGV2ScreenRegistry", "UGV2UiTheme"):
            if re.search(rf"\b{forbidden}\b", text):
                errors.append(
                    f"{header}: classified as value/physical but names the authority-aware type {forbidden}"
                )
    return errors


def run_self_test() -> bool:
    print("[*] Running validate_apply_move_closure self-test...")
    bases = movable_widget_bases()
    if errors := find_violations(bases):
        print("FAILED: the repository already violates the gate:\n" + "\n".join(errors))
        return False

    # A movable base that reaches a new authority-aware header must be rejected.
    grown = dict(bases)
    grown["GV2SyntheticWidgetBase.h"] = (
        '#include "Application/GV2SessionContentSnapshot.h"\n'
        "class GV2_API UGV2SyntheticWidgetBase : public UWidget, public IGV2PreparedTintStyleTarget {};\n"
    )
    if not find_violations(grown):
        print("FAILED: gate accepted a movable widget base reaching a new authority-aware header")
        return False

    # A shrinking closure must NOT be rejected: PSC-12 removing an include is progress.
    if find_violations(bases, AUTHORITY_AWARE_BASELINE | {"UI/GV2NoLongerIncluded.h"}):
        print("FAILED: gate rejected a closure that shrank")
        return False

    # A class performing no role is not movable, so its includes are not this gate's business.
    unrelated = dict(bases)
    unrelated["GV2UnrelatedWidgetBase.h"] = (
        '#include "Application/GV2SessionContentSnapshot.h"\n'
        "class GV2_API UGV2UnrelatedWidgetBase : public UWidget {};\n"
    )
    if find_violations(unrelated):
        print("FAILED: gate flagged a class that performs no prepared role")
        return False

    print("SUCCESS: the movable widget bases' authority-aware include closure has not grown")
    return True


def main(argv: list[str]) -> int:
    if len(argv) > 1 and argv[1] == "--self-test":
        return 0 if run_self_test() else 1
    errors = find_violations(movable_widget_bases())
    if errors:
        print("FAILED: PSC-11 move-closure violations:\n" + "\n".join(errors))
        return 1
    print("SUCCESS: the movable widget bases' authority-aware include closure has not grown")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
