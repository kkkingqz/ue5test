#!/usr/bin/env python3
"""Derives cancellable UI Commit roots and requires a rollback classification.

GBF-07 must not rely on the six boundaries found by a past code review.  The
actual side is every production definition named ``Commit*`` or
``AttachScreenToLayer`` under Source/GV2/Private/UI. Every definition must
declare whether it is the unique owner of a rollback boundary, a direct
property-mutation leaf, or a delegation to an owning boundary. The independently
declared C++ enum must contain exactly the owning boundary IDs.
"""

from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
UI_SOURCE_ROOT = REPO_ROOT / "Source" / "GV2" / "Private" / "UI"
BOUNDARY_HEADER = REPO_ROOT / "Source" / "GV2" / "Public" / "UI" / "GV2UiRollbackBoundary.h"
BOUNDARY_SOURCE = REPO_ROOT / "Source" / "GV2" / "Private" / "UI" / "GV2UiRollbackBoundary.cpp"
ENUM_SIGNATURE = "enum class EGV2UiRollbackBoundary"
RECOVERY_SIGNATURE = "EGV2UiRollbackRecovery GetUiRollbackBoundaryRecovery"
MARKER_PATTERN = re.compile(r"GBF-07:\s*rollback_boundary=(?P<id>[A-Za-z_]\w*)")
LEAF_MARKER_PATTERN = re.compile(r"GBF-07:\s*rollback_leaf=(?P<id>[A-Za-z_]\w*)")
DELEGATE_MARKER_PATTERN = re.compile(r"GBF-07:\s*rollback_delegate=(?P<id>[A-Za-z_]\w*)")
FUNCTION_NAME_PATTERN = re.compile(
    r"(?<![A-Za-z0-9_])(?:(?P<scope>[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)::)?"
    r"(?P<name>Commit(?:[A-Z]\w*)?|AttachScreenToLayer)\s*\("
)
RECOVERY_CASE_PATTERN = re.compile(
    r"case\s+EGV2UiRollbackBoundary::(?P<boundary>[A-Za-z_]\w*)\s*:\s*"
    r"return\s+EGV2UiRollbackRecovery::(?P<recovery>[A-Za-z_]\w*)\s*;"
)
# PAH-01: RollbackFieldPlans (GV2ScreenWidgetBase.h) is [[nodiscard]] -- this pattern finds
# a call to it, and the check below rejects one used as a bare, discarded statement instead
# of bound to a variable. Defense in depth alongside the compiler's own [[nodiscard]]
# diagnostic, matching this repo's established practice of pairing a C++ attribute with an
# independent text-level gate rather than trusting the compiler warning alone.
ROLLBACK_RESULT_CALL_PATTERN = re.compile(r"RollbackFieldPlans\s*\(")


def extract_enum_values(header_source: str) -> tuple[set[str], list[str]]:
    start = header_source.find(ENUM_SIGNATURE)
    if start < 0:
        return set(), [f"{BOUNDARY_HEADER}: EGV2UiRollbackBoundary enum not found"]
    open_brace = header_source.find("{", start)
    close_brace = header_source.find("}", open_brace)
    if open_brace < 0 or close_brace < 0:
        return set(), [f"{BOUNDARY_HEADER}: EGV2UiRollbackBoundary body not found"]

    values: set[str] = set()
    for raw_entry in header_source[open_brace + 1 : close_brace].split(","):
        entry = re.sub(r"//[^\n]*", "", raw_entry).strip()
        if not entry:
            continue
        name = entry.split("=", 1)[0].strip()
        if name == "Count":
            continue
        if not re.fullmatch(r"[A-Za-z_]\w*", name):
            return set(), [f"{BOUNDARY_HEADER}: cannot parse enum entry '{entry}'"]
        values.add(name)
    return values, []


def extract_function_body(source: str, signature: str) -> str | None:
    start = source.find(signature)
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


def mask_comments_and_literals(source: str) -> str:
    """Preserve offsets while removing syntax that may contain fake function names."""
    masked = list(source)
    index = 0
    while index < len(source):
        if source.startswith("//", index):
            end = source.find("\n", index)
            if end < 0:
                end = len(source)
            for masked_index in range(index, end):
                masked[masked_index] = " "
            index = end
            continue
        if source.startswith("/*", index):
            end = source.find("*/", index + 2)
            end = len(source) if end < 0 else end + 2
            for masked_index in range(index, end):
                if masked[masked_index] != "\n":
                    masked[masked_index] = " "
            index = end
            continue
        if source[index] in {'"', "'"}:
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
            for masked_index in range(index, min(end, len(source))):
                if masked[masked_index] != "\n":
                    masked[masked_index] = " "
            index = end
            continue
        index += 1
    return "".join(masked)


def find_discarded_rollback_result_calls(source: str) -> list[int]:
    """Returns 1-indexed line numbers of a bare, discarded RollbackFieldPlans(...) call --
    one whose [[nodiscard]] FGV2UiRollbackResult is not bound to anything, i.e. the four
    call sites PAH-01 closed (silently continuing on a restoration failure), reintroduced.
    A properly consumed call is always preceded by '=' (assignment into a result variable);
    anything else -- a bare statement, immediately after '{' or ';' -- is a discard.
    """
    masked = mask_comments_and_literals(source)
    violations: list[int] = []
    for match in ROLLBACK_RESULT_CALL_PATTERN.finditer(masked):
        if is_function_definition(masked, match.end() - 1):
            continue  # the function's own declaration/definition, not a call site
        preceding = masked[: match.start()].rstrip()
        if not preceding.endswith("="):
            violations.append(source.count("\n", 0, match.start()) + 1)
    return violations


def matching_parenthesis(source: str, open_paren: int) -> int | None:
    depth = 0
    for index in range(open_paren, len(source)):
        if source[index] == "(":
            depth += 1
        elif source[index] == ")":
            depth -= 1
            if depth == 0:
                return index
    return None


def is_function_definition(source: str, open_paren: int) -> bool:
    close_paren = matching_parenthesis(source, open_paren)
    if close_paren is None:
        return False

    suffix = close_paren + 1
    while suffix < len(source) and source[suffix].isspace():
        suffix += 1
    # The first close parenthesis means that this candidate is an expression such
    # as `if (Object->Commit())`, not a declaration with this call's parameter list.
    if suffix < len(source) and source[suffix] == ")":
        return False

    next_brace = source.find("{", suffix)
    next_semicolon = source.find(";", suffix)
    return next_brace >= 0 and (next_semicolon < 0 or next_brace < next_semicolon)


def iter_function_definitions(source: str):
    masked_source = mask_comments_and_literals(source)
    for match in FUNCTION_NAME_PATTERN.finditer(masked_source):
        open_paren = masked_source.find("(", match.start(), match.end())
        if open_paren >= 0 and is_function_definition(masked_source, open_paren):
            yield match


def validate_recovery_classifications(enum_values: set[str], recovery_source: str) -> list[str]:
    body = extract_function_body(recovery_source, RECOVERY_SIGNATURE)
    if body is None:
        return [f"{BOUNDARY_SOURCE}: {RECOVERY_SIGNATURE} function not found"]

    classifications: dict[str, str] = {}
    errors: list[str] = []
    for match in RECOVERY_CASE_PATTERN.finditer(body):
        boundary = match.group("boundary")
        recovery = match.group("recovery")
        if boundary in classifications:
            errors.append(f"{BOUNDARY_SOURCE}: rollback boundary '{boundary}' has duplicate recovery cases")
        classifications[boundary] = recovery

    actual = set(classifications)
    missing = sorted(enum_values - actual)
    unexpected = sorted(actual - enum_values)
    if missing:
        errors.append("rollback boundary enum value(s) lack a recovery classification: " + ", ".join(missing))
    if unexpected:
        errors.append("recovery classification(s) name unknown boundary enum value(s): " + ", ".join(unexpected))
    if not classifications:
        errors.append(f"{BOUNDARY_SOURCE}: no rollback recovery classifications found")
    return errors


def classification_marker_for(source: str, function_start: int) -> tuple[str, str] | None:
    line_start = source.rfind("\n", 0, function_start) + 1
    for line in reversed(source[:line_start].splitlines()):
        stripped = line.strip()
        if not stripped:
            continue
        for kind, pattern in (
            ("boundary", MARKER_PATTERN),
            ("leaf", LEAF_MARKER_PATTERN),
            ("delegate", DELEGATE_MARKER_PATTERN),
        ):
            marker = pattern.search(stripped)
            if marker:
                return kind, marker.group("id")
        return None
    return None


def validate_sources(source_files: dict[Path, str], header_source: str, recovery_source: str) -> list[str]:
    enum_values, errors = extract_enum_values(header_source)
    discovered: dict[str, tuple[Path, str]] = {}

    for path, source in source_files.items():
        for line_number in find_discarded_rollback_result_calls(source):
            errors.append(
                f"{path}:{line_number}: RollbackFieldPlans(...) result discarded -- PAH-01 requires "
                "every [[nodiscard]] restoration result to be bound to a variable and consumed, "
                "not called as a bare statement"
            )

    for path, source in source_files.items():
        for match in iter_function_definitions(source):
            name = match.group("name")
            qualified_name = f"{match.group('scope')}::{name}" if match.group("scope") else name

            marker = classification_marker_for(source, match.start())
            if name == "Commit":
                if marker is None:
                    errors.append(
                        f"{path}: unclassified cancellable Commit leaf '{qualified_name}'; "
                        "add a GBF-07 rollback_leaf or rollback_delegate marker"
                    )
                    continue
                marker_kind, marker_id = marker
                if marker_kind not in {"leaf", "delegate"}:
                    errors.append(
                        f"{path}: bare Commit leaf '{qualified_name}' must use rollback_leaf "
                        "or rollback_delegate, not rollback_boundary"
                    )
                elif marker_id not in enum_values:
                    errors.append(
                        f"{path}: Commit leaf '{qualified_name}' references unknown rollback boundary "
                        f"'{marker_id}'"
                    )
                continue

            if marker is None:
                errors.append(
                    f"{path}: unclassified cancellable Commit root '{qualified_name}'; "
                    "add a GBF-07 rollback_boundary marker and enum classification"
                )
                continue
            marker_kind, boundary_id = marker
            if marker_kind != "boundary":
                errors.append(
                    f"{path}: cancellable Commit root '{qualified_name}' must use rollback_boundary, "
                    f"not rollback_{marker_kind}"
                )
                continue
            if boundary_id in discovered:
                previous_path, previous_function = discovered[boundary_id]
                errors.append(
                    f"{path}: rollback boundary '{boundary_id}' is duplicated by '{qualified_name}' "
                    f"(already '{previous_function}' in {previous_path})"
                )
                continue
            discovered[boundary_id] = (path, qualified_name)

    discovered_ids = set(discovered)
    unclassified = sorted(discovered_ids - enum_values)
    missing_roots = sorted(enum_values - discovered_ids)
    if unclassified:
        errors.append(
            "GBF-07 rollback boundary marker(s) missing from EGV2UiRollbackBoundary: "
            + ", ".join(unclassified)
        )
    if missing_roots:
        errors.append(
            "EGV2UiRollbackBoundary value(s) have no production Commit root: "
            + ", ".join(missing_roots)
        )
    if not discovered:
        errors.append("GBF-07 rollback boundary scan found no cancellable Commit roots")
    errors.extend(validate_recovery_classifications(enum_values, recovery_source))
    return errors


def production_sources() -> dict[Path, str]:
    return {
        path: path.read_text(encoding="utf-8")
        for path in sorted(UI_SOURCE_ROOT.rglob("*.cpp"))
    }


def validate_repository() -> list[str]:
    if not BOUNDARY_HEADER.exists():
        return [f"{BOUNDARY_HEADER}: EGV2UiRollbackBoundary header not found"]
    if not BOUNDARY_SOURCE.exists():
        return [f"{BOUNDARY_SOURCE}: rollback recovery source not found"]
    return validate_sources(
        production_sources(),
        BOUNDARY_HEADER.read_text(encoding="utf-8"),
        BOUNDARY_SOURCE.read_text(encoding="utf-8"),
    )


def run_self_test() -> bool:
    print("[*] Running validate_ui_rollback_boundaries self-test...")
    if errors := validate_repository():
        print("FAILED: current production source violates the gate:\n" + "\n".join(errors))
        return False

    sources = production_sources()
    header = BOUNDARY_HEADER.read_text(encoding="utf-8")
    recovery_source = BOUNDARY_SOURCE.read_text(encoding="utf-8")
    with tempfile.TemporaryDirectory() as tmpdir:
        synthetic_path = Path(tmpdir) / "SyntheticBoundary.cpp"
        for declaration in (
            "static bool FSyntheticBoundary::CommitAfterMutation() { return false; }\n",
            "UE_NODISCARD bool FSyntheticBoundary::CommitAfterMutation() { return false; }\n",
            "UE_DEPRECATED(5.0, \"migration\") bool FSyntheticBoundary::CommitAfterMutation() { return false; }\n",
            "[[nodiscard]] bool FSyntheticBoundary::CommitAfterMutation() { return false; }\n",
            "auto FSyntheticBoundary::CommitAfterMutation() -> bool { return false; }\n",
            "decltype(auto) FSyntheticBoundary::CommitAfterMutation() { return false; }\n",
        ):
            sources[synthetic_path] = declaration
            errors = validate_sources(sources, header, recovery_source)
            if not any("FSyntheticBoundary::CommitAfterMutation" in error for error in errors):
                print(f"FAILED: gate accepted an unclassified synthetic boundary: {errors}")
                return False

        sources[synthetic_path] = "bool FSyntheticLeaf::Commit() { return false; }\n"
        errors = validate_sources(sources, header, recovery_source)
        if not any("FSyntheticLeaf::Commit" in error for error in errors):
            print(f"FAILED: gate accepted an unclassified synthetic Commit leaf: {errors}")
            return False

        sources.pop(synthetic_path)
        mutation_source = next(
            path for path, source in sources.items() if "rollback_boundary=PropertyMutation" in source
        )
        sources[mutation_source] = sources[mutation_source].replace(
            "GBF-07: rollback_boundary=PropertyMutation", "GBF-07: removed_marker", 1
        )
        errors = validate_sources(sources, header, recovery_source)
        if not any("CommitUiHostProperties" in error for error in errors):
            print(f"FAILED: gate accepted a root with its rollback marker removed: {errors}")
            return False

        missing_recovery = recovery_source.replace(
            "    case EGV2UiRollbackBoundary::Document:\n        return EGV2UiRollbackRecovery::ReplayInverse;\n",
            "",
            1,
        )
        errors = validate_sources(production_sources(), header, missing_recovery)
        if not any("Document" in error and "lack a recovery classification" in error for error in errors):
            print(f"FAILED: gate accepted a boundary without recovery classification: {errors}")
            return False

        # PAH-01: a RollbackFieldPlans(...) call reverted to a bare, discarded statement
        # (the exact shape of the four call sites this task closed) must be caught, even
        # though it is real production source with a correctly classified boundary.
        discardable_sources = production_sources()
        discard_path = next(
            path for path, source in discardable_sources.items() if "SiblingRollback = RollbackFieldPlans(" in source
        )
        discardable_sources[discard_path] = discardable_sources[discard_path].replace(
            "const FGV2UiRollbackResult SiblingRollback = RollbackFieldPlans(",
            "RollbackFieldPlans(",
            1,
        )
        errors = validate_sources(discardable_sources, header, recovery_source)
        if not any("result discarded" in error and str(discard_path) in error for error in errors):
            print(f"FAILED: gate accepted a discarded RollbackFieldPlans(...) result: {errors}")
            return False

    print("SUCCESS: every cancellable UI Commit root has one enum-backed rollback classification")
    return True


def main(argv: list[str]) -> int:
    if argv == ["--self-test"]:
        return 0 if run_self_test() else 1
    if argv:
        print("usage: validate_ui_rollback_boundaries.py [--self-test]", file=sys.stderr)
        return 2

    errors = validate_repository()
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("SUCCESS: every cancellable UI Commit root has one enum-backed rollback classification")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
