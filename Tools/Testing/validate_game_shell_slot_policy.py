#!/usr/bin/env python3
"""Require every keyed GameShell rebuild to restore its slot layout policy.

PSC-14 / PSC-AF-02 exposed a gap between logical child reconciliation and the
physical UPanelSlot created by AddChild. The actual set is derived from every
FGV2KeyedCollection ReconcilePrepared/RestoreOrder call in the production
layered reconciler; each call must carry the independent GameShell-owned policy.
The direct attach primitive must delegate to that same policy as well.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
RECONCILER_PATH = REPO_ROOT / "Source" / "GV2" / "Private" / "UI" / "GV2LayeredUiReconciler.cpp"
SHELL_PATH = REPO_ROOT / "Source" / "GV2" / "Private" / "UI" / "GV2GameShellWidgetBase.cpp"
SLOT_POLICY = "UGV2GameShellWidgetBase::ApplyScreenSlotLayout"
ATTACH_SIGNATURE = "bool UGV2GameShellWidgetBase::AttachScreenToLayer"
CALL_NAMES = (
    "FGV2KeyedCollection::ReconcilePrepared",
    "FGV2KeyedCollection::RestoreOrder",
)


def mask_comments_and_literals(source: str) -> str:
    """Replace comments and literals with spaces while preserving offsets."""

    masked = list(source)
    index = 0
    while index < len(source):
        if source.startswith("//", index):
            end = source.find("\n", index)
            end = len(source) if end < 0 else end
        elif source.startswith("/*", index):
            close = source.find("*/", index + 2)
            end = len(source) if close < 0 else close + 2
        elif source[index] in {'"', "'"}:
            quote = source[index]
            end = index + 1
            while end < len(source):
                if source[end] == "\\":
                    end += 2
                    continue
                if source[end] == quote:
                    end += 1
                    break
                end += 1
        else:
            index += 1
            continue

        for masked_index in range(index, min(end, len(source))):
            if masked[masked_index] != "\n":
                masked[masked_index] = " "
        index = end
    return "".join(masked)


def matching_delimiter(source: str, start: int, opening: str, closing: str) -> int | None:
    depth = 0
    for index in range(start, len(source)):
        if source[index] == opening:
            depth += 1
        elif source[index] == closing:
            depth -= 1
            if depth == 0:
                return index
    return None


def extract_calls(source: str, name: str) -> list[tuple[int, int, str]]:
    masked = mask_comments_and_literals(source)
    calls: list[tuple[int, int, str]] = []
    for match in re.finditer(re.escape(name), masked):
        open_paren = masked.find("(", match.end())
        if open_paren < 0:
            continue
        if masked.find(";", match.end(), open_paren) >= 0:
            continue
        close_paren = matching_delimiter(masked, open_paren, "(", ")")
        if close_paren is not None:
            calls.append((match.start(), close_paren + 1, source[match.start() : close_paren + 1]))
    return calls


def extract_function_body(source: str, signature: str) -> str | None:
    masked = mask_comments_and_literals(source)
    start = masked.find(signature)
    if start < 0:
        return None
    open_brace = masked.find("{", start)
    if open_brace < 0:
        return None
    close_brace = matching_delimiter(masked, open_brace, "{", "}")
    return None if close_brace is None else source[open_brace + 1 : close_brace]


def validate_sources(reconciler_source: str, shell_source: str, label: str) -> list[str]:
    errors: list[str] = []
    for name in CALL_NAMES:
        calls = extract_calls(reconciler_source, name)
        short_name = name.rsplit("::", 1)[-1]
        if not calls:
            errors.append(
                f"{label}: source-derived GameShell set contains no {short_name} call; "
                "the gate must not pass vacuously"
            )
        for ordinal, (_, _, call) in enumerate(calls, start=1):
            if SLOT_POLICY not in call:
                errors.append(
                    f"{label}: {short_name} call #{ordinal} does not apply {SLOT_POLICY} "
                    "to its fresh panel slots"
                )

    attach_body = extract_function_body(shell_source, ATTACH_SIGNATURE)
    if attach_body is None:
        errors.append(f"{label}: AttachScreenToLayer body not found")
    elif SLOT_POLICY not in attach_body:
        errors.append(
            f"{label}: AttachScreenToLayer does not delegate to the shared {SLOT_POLICY} policy"
        )
    return errors


def validate_repository() -> list[str]:
    missing = [path for path in (RECONCILER_PATH, SHELL_PATH) if not path.exists()]
    if missing:
        return [f"{path}: source file not found" for path in missing]
    return validate_sources(
        RECONCILER_PATH.read_text(encoding="utf-8"),
        SHELL_PATH.read_text(encoding="utf-8"),
        "production",
    )


def remove_policy_from_first_call(source: str, name: str) -> str:
    calls = extract_calls(source, name)
    if not calls:
        return source
    start, end, call = calls[0]
    mutated_call = call.replace(SLOT_POLICY, "OmittedScreenSlotLayout", 1)
    return source[:start] + mutated_call + source[end:]


def run_self_test() -> bool:
    print("[*] Running validate_game_shell_slot_policy self-test...")
    production_errors = validate_repository()
    if production_errors:
        print("FAILED: current production source violates the gate:\n" + "\n".join(production_errors))
        return False

    reconciler = RECONCILER_PATH.read_text(encoding="utf-8")
    shell = SHELL_PATH.read_text(encoding="utf-8")
    for name in CALL_NAMES:
        mutated = remove_policy_from_first_call(reconciler, name)
        errors = validate_sources(mutated, shell, f"synthetic {name}")
        short_name = name.rsplit("::", 1)[-1]
        if not any(short_name in error and "does not apply" in error for error in errors):
            print(f"FAILED: gate accepted {short_name} without slot policy: {errors}")
            return False

    mutated_shell = shell.replace(SLOT_POLICY, "OmittedScreenSlotLayout")
    errors = validate_sources(reconciler, mutated_shell, "synthetic attach")
    if not any("AttachScreenToLayer does not delegate" in error for error in errors):
        print(f"FAILED: gate accepted direct attach without shared slot policy: {errors}")
        return False

    print("SUCCESS: synthetic missing-policy mutations are rejected")
    return True


def main(argv: list[str]) -> int:
    if argv == ["--self-test"]:
        return 0 if run_self_test() else 1
    if argv:
        print("usage: validate_game_shell_slot_policy.py [--self-test]", file=sys.stderr)
        return 2

    errors = validate_repository()
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("SUCCESS: every keyed GameShell rebuild and restore reapplies the shared slot policy")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
