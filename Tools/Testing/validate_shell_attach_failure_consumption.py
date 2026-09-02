#!/usr/bin/env python3
"""Enumerates fallible Engine AddChild calls in the Game Shell attach path.

GBF-01 needs a structural gate in addition to its runtime regression.  The set is
not a hand-written checklist: this scanner derives every ``->AddChild(...)`` call
inside UGV2GameShellWidgetBase::AttachScreenToLayer from the production source and
requires each return value to be propagated as ``false`` when it is null.
"""

from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SHELL_SOURCE_PATH = REPO_ROOT / "Source" / "GV2" / "Private" / "UI" / "GV2GameShellWidgetBase.cpp"
ATTACH_SIGNATURE = "bool UGV2GameShellWidgetBase::AttachScreenToLayer"
ADD_CHILD_CALL_PATTERN = re.compile(r"\b[A-Za-z_]\w*\s*->\s*AddChild\s*\(")
ASSIGNED_ADD_CHILD_PATTERN = re.compile(
    r"(?:UPanelSlot\s*\*\s*)?(?P<result>[A-Za-z_]\w*)\s*=\s*"
    r"[A-Za-z_]\w*\s*->\s*AddChild\s*\(",
    re.DOTALL,
)


def strip_comments(source: str) -> str:
    """Preserve code layout while excluding comments from the source enumerator."""

    source = re.sub(r"/\*.*?\*/", lambda match: "\n" * match.group(0).count("\n"), source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", source)


def extract_attach_body(source: str) -> str | None:
    signature_index = source.find(ATTACH_SIGNATURE)
    if signature_index < 0:
        return None
    open_brace = source.find("{", signature_index)
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


def is_result_propagated(body: str, result_name: str, statement_end: int) -> bool:
    remaining_body = body[statement_end:]
    propagation_pattern = re.compile(
        rf"if\s*\(\s*{re.escape(result_name)}\s*==\s*nullptr\s*\)\s*"
        rf"\{{\s*return\s+false\s*;\s*\}}",
        re.DOTALL,
    )
    return propagation_pattern.search(remaining_body) is not None


def validate_source(path: Path) -> list[str]:
    if not path.exists():
        return [f"{path}: error [GBF01_SHELL_ATTACH_FAILURE_CONSUMPTION]: source file not found"]

    body = extract_attach_body(strip_comments(path.read_text(encoding="utf-8")))
    if body is None:
        return [
            f"{path}: error [GBF01_SHELL_ATTACH_FAILURE_CONSUMPTION]: "
            "AttachScreenToLayer function body not found"
        ]

    errors: list[str] = []
    calls = list(ADD_CHILD_CALL_PATTERN.finditer(body))
    if not calls:
        errors.append(
            f"{path}: error [GBF01_SHELL_ATTACH_FAILURE_CONSUMPTION]: "
            "AttachScreenToLayer must enumerate its fallible Engine AddChild call"
        )

    for call in calls:
        statement_start = max(body.rfind(";", 0, call.start()), body.rfind("{", 0, call.start()), body.rfind("}", 0, call.start())) + 1
        statement_end = body.find(";", call.end())
        if statement_end < 0:
            errors.append(
                f"{path}: error [GBF01_SHELL_ATTACH_FAILURE_CONSUMPTION]: "
                "unterminated AddChild statement"
            )
            continue

        statement = body[statement_start : statement_end + 1]
        assigned = ASSIGNED_ADD_CHILD_PATTERN.search(statement)
        if assigned is None:
            errors.append(
                f"{path}: error [GBF01_SHELL_ATTACH_FAILURE_CONSUMPTION]: "
                "AddChild result is discarded; assign it and propagate nullptr as false"
            )
            continue

        result_name = assigned.group("result")
        if not is_result_propagated(body, result_name, statement_end + 1):
            errors.append(
                f"{path}: error [GBF01_SHELL_ATTACH_FAILURE_CONSUMPTION]: "
                f"AddChild result '{result_name}' is not propagated as false on nullptr"
            )

    return errors


def run_self_test() -> bool:
    print("[*] Running validate_shell_attach_failure_consumption self-test...")
    actual_errors = validate_source(SHELL_SOURCE_PATH)
    if actual_errors:
        print("FAILED: current production source violates the gate:\n" + "\n".join(actual_errors))
        return False

    with tempfile.TemporaryDirectory() as tmpdir:
        temporary_source = Path(tmpdir) / "GV2GameShellWidgetBase.cpp"
        production_source = SHELL_SOURCE_PATH.read_text(encoding="utf-8")

        missing_propagation = production_source.replace(
            "        if (NewSlot == nullptr)\n        {\n            return false;\n        }\n",
            "",
            1,
        )
        temporary_source.write_text(missing_propagation, encoding="utf-8")
        errors = validate_source(temporary_source)
        if not any("NewSlot' is not propagated" in error for error in errors):
            print(f"FAILED: gate accepted an unpropagated AddChild result: {errors}")
            return False

        discarded_result = production_source.replace(
            "        UPanelSlot* NewSlot = Host->AddChild(ScreenWidget);",
            "        Host->AddChild(ScreenWidget);",
            1,
        )
        temporary_source.write_text(discarded_result, encoding="utf-8")
        errors = validate_source(temporary_source)
        if not any("AddChild result is discarded" in error for error in errors):
            print(f"FAILED: gate accepted a discarded AddChild result: {errors}")
            return False

    print("SUCCESS: every fallible Engine AddChild call in AttachScreenToLayer propagates nullptr as false")
    return True


def main(argv: list[str]) -> int:
    if argv == ["--self-test"]:
        return 0 if run_self_test() else 1
    if argv:
        print("usage: validate_shell_attach_failure_consumption.py [--self-test]", file=sys.stderr)
        return 2

    errors = validate_source(SHELL_SOURCE_PATH)
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("SUCCESS: every fallible Engine AddChild call in AttachScreenToLayer propagates nullptr as false")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
