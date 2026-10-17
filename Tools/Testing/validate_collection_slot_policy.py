#!/usr/bin/env python3
"""Every panel rebuild must state the slot policy it re-applies.

UPanelWidget::ClearChildren destroys the UPanelSlot objects; AddChild creates new ones
carrying nothing but their UCLASS defaults. So a function that does both has replaced every
slot in that panel, and whatever layout policy those slots carried is gone unless the same
function puts it back.

This was not hypothetical. FGV2KeyedCollection carried a ConfigureSlot parameter documenting
exactly this hazard and had no caller anywhere in the repository, while the production commit
path (FPreparedKeyedCollectionOperation) did its own bare ClearChildren/AddChild loop. The
measured consequence: the Theme's ButtonListItemPadding went from (0,0,0,8) to (0,0,0,0)
across one rebuild -- every revision that republishes a collection, i.e. every click that
changes it (GV2.Runtime.UIKit.CollectionItemSlotPolicySurvivesRebuild).

The set is derived from production sources, not maintained as a list of call sites: a function
is in scope when its own body calls both ClearChildren() and AddChild(. It satisfies the rule
when that same body also names a slot-configuring call. "Stating the policy" includes stating
that the defaults are correct -- an empty policy function is a decision, an absent one is not.

This gate is a text scan and so is secondary to code review, the same caveat
validate_viewport_refresh_coverage carries: a rebuild that reaches slots through some other
spelling can evade it.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SOURCE_ROOT = REPO_ROOT / "Source"

REBUILD_MARKERS = ("ClearChildren()", "AddChild(")
# A call whose name says it configures a slot. Matched against the function body, so a
# rebuild satisfies the rule by naming its policy rather than by inlining property writes --
# the policy stays one named thing the rollback path can reuse, which is why
# FGV2KeyedCollection::RestoreOrder takes the same callback.
POLICY_CALL_PATTERN = re.compile(
    r"\b(?:ConfigureCollectionItemSlot|ConfigureSlot|ConfigureTabContentSlot|ApplyScreenSlotLayout)\s*\("
)

# Leading whitespace is allowed on purpose: without it the pattern only ever matched
# definitions starting at column 0, so every method defined INSIDE a class body -- including
# FGV2KeyedCollection's own Reconcile/ReconcilePrepared/RestoreOrder templates, the three
# functions that actually own the rebuild primitive -- was silently out of scope. The gate
# reported "all rebuilds state their policy" while seeing two of the six.
FUNCTION_PATTERN = re.compile(
    r"^[ \t]*(?P<signature>[A-Za-z_][^\n;{}]*?\b(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*\([^;{}]*\)[^;{}]*)\{",
    re.MULTILINE,
)
# A control-flow block is not a function: without this an if/for whose body happens to hold
# both markers would be counted as its own in-scope "function" named `if`, inflating the
# count and letting the enclosing real function escape the check.
CONTROL_KEYWORDS = frozenset(
    {"if", "else", "for", "while", "switch", "catch", "do", "return"}
)


def strip_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", source)


def iter_function_bodies(source: str):
    """Yields (function_name, body) for each brace-balanced function body."""
    for match in FUNCTION_PATTERN.finditer(source):
        if match.group("name") in CONTROL_KEYWORDS:
            continue
        start = match.end() - 1
        depth = 0
        for index in range(start, len(source)):
            char = source[index]
            if char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
                if depth == 0:
                    yield match.group("name"), source[start : index + 1]
                    break


def production_sources() -> dict[str, str]:
    sources: dict[str, str] = {}
    for path in sorted(SOURCE_ROOT.rglob("*.cpp")) + sorted(SOURCE_ROOT.rglob("*.h")):
        parts = set(path.parts)
        if "Tests" in parts:
            continue
        sources[str(path.relative_to(REPO_ROOT))] = path.read_text(encoding="utf-8")
    return sources


def find_violations(sources: dict[str, str]) -> tuple[list[str], int]:
    errors: list[str] = []
    rebuilds_found = 0
    for filename, source in sorted(sources.items()):
        stripped = strip_comments(source)
        if not all(marker in stripped for marker in REBUILD_MARKERS):
            continue
        for function_name, body in iter_function_bodies(stripped):
            if not all(marker in body for marker in REBUILD_MARKERS):
                continue
            rebuilds_found += 1
            if POLICY_CALL_PATTERN.search(body) is None:
                errors.append(
                    f"{filename}: {function_name} rebuilds a panel's children "
                    "(ClearChildren + AddChild) without naming the slot policy it re-applies"
                )
    if rebuilds_found == 0:
        errors.append("panel-rebuild enumerator produced an empty set")
    return errors, rebuilds_found


def run_self_test() -> bool:
    print("[*] Running validate_collection_slot_policy self-test...")
    sources = production_sources()
    errors, rebuilds_found = find_violations(sources)
    if errors:
        print("FAILED: current repository already violates the gate:\n" + "\n".join(errors))
        return False

    missing_policy = {
        "Synthetic.cpp": """
void UGV2FutureHost::RebuildWithoutPolicy()
{
    Panel->ClearChildren();
    for (UWidget* Child : Children)
    {
        Panel->AddChild(Child);
    }
}
"""
    }
    negative_errors, _ = find_violations(missing_policy)
    if not any("without naming the slot policy" in error for error in negative_errors):
        print("FAILED: a rebuild with no slot policy was not rejected")
        return False

    with_policy = {
        "Synthetic.cpp": """
void UGV2FutureHost::RebuildWithPolicy()
{
    Panel->ClearChildren();
    for (UWidget* Child : Children)
    {
        UPanelSlot* NewSlot = Panel->AddChild(Child);
        ConfigureCollectionItemSlot(*NewSlot);
    }
}
"""
    }
    positive_errors, _ = find_violations(with_policy)
    if positive_errors:
        print("FAILED: a rebuild that states its policy was rejected:\n" + "\n".join(positive_errors))
        return False

    # A comment naming the policy must not satisfy the rule: the gate reads code, not prose.
    comment_only = {
        "Synthetic.cpp": """
void UGV2FutureHost::RebuildWithCommentOnly()
{
    // ConfigureCollectionItemSlot(Slot) would go here.
    Panel->ClearChildren();
    Panel->AddChild(Child);
}
"""
    }
    comment_errors, _ = find_violations(comment_only)
    if not any("without naming the slot policy" in error for error in comment_errors):
        print("FAILED: a commented-out policy satisfied the rule")
        return False

    print(
        f"SUCCESS: negative fixtures are rejected and all {rebuilds_found} "
        "repository panel rebuilds state their slot policy"
    )
    return True


def main(argv: list[str]) -> int:
    if "--self-test" in argv:
        return 0 if run_self_test() else 1

    errors, rebuilds_found = find_violations(production_sources())
    if errors:
        print("FAILED: panel rebuilds without a stated slot policy:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1
    print(
        f"SUCCESS: all {rebuilds_found} production panel rebuilds re-apply a named slot policy"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
