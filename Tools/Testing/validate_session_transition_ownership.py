#!/usr/bin/env python3
"""Validates session transition policy, state mutation encapsulation, and single Lua VM invariants.

CFC-07 (ADR-0043, ADR-0044, STATUS-001):
1. State mutation encapsulation:
   - Direct mutations of Status.SessionState and Status.ApplicationState are strictly
     prohibited outside of TryTransitionSessionState and TryTransitionApplicationState
     in GV2SessionTransition.cpp.
   - Inside TryTransitionSessionState, only SessionState may be mutated (InOutStatus.SessionState = TargetState).
   - Inside TryTransitionApplicationState, only ApplicationState may be mutated (InOutStatus.ApplicationState = TargetState).
   - Whole-struct assignments to Status or InOutStatus are prohibited everywhere.
2. Single live Lua VM invariant:
   - luaL_newstate() must only be called in GV2RuntimeSession.cpp, guarded by the atomic
     GLiveVmCount counter ensuring <= 1 live VM across the entire process.
   - Every call to luaL_newstate() must be preceded by an atomic increment of GLiveVmCount
     in its enclosing function.
   - GLiveVmCount must be decremented on session teardown/CloseState().
3. Exhaustive transition switches:
   - Switches on ESessionTransitionKind and ESessionStartMode across all files in Source/GV2
     (both headers and implementation files) must be exhaustive with no fallback default: labels
     and no surrogate fallback return statements after the switch block.
4. Transition policy structural interface and enum coverage:
   - Closed ESessionTransitionKind enum must have SessionTransitionKindCount and AllSessionTransitionKinds.
   - FGV2SessionTransitionPolicy class must structurally declare all required methods:
     EnqueueRequest, EnqueueShutdown, CancelRequest, GetOutcome, DequeuePendingOperation, GetActiveOperation.
   - Behavioral invariants (request joining, single pending slot superseding, priority shutdown,
     and point-of-no-return cancellation) are verified by the automation test suite in GV2SessionTransitionTests.cpp.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
GV2_SOURCE_DIR = REPO_ROOT / "Source" / "GV2"
RUNTIME_CORE_DIR = REPO_ROOT / "Source" / "GV2RuntimeCore"
TRANSITION_HEADER_PATH = GV2_SOURCE_DIR / "Private" / "Application" / "GV2SessionTransition.h"
TRANSITION_IMPL_PATH = GV2_SOURCE_DIR / "Private" / "Application" / "GV2SessionTransition.cpp"
COORDINATOR_IMPL_PATH = GV2_SOURCE_DIR / "Private" / "Application" / "GV2SessionCoordinator.cpp"
RUNTIME_SESSION_IMPL_PATH = RUNTIME_CORE_DIR / "Private" / "GV2RuntimeSession.cpp"
TESTS_PATH = GV2_SOURCE_DIR / "Private" / "Tests" / "GV2SessionTransitionTests.cpp"

CONTROL_KEYWORDS = {"if", "for", "while", "switch", "catch", "else", "do", "try"}
DECL_KEYWORDS = {"const", "auto", "FGV2SessionStatus", "EGV2PropertyConsumerKindStatus", "struct", "class", "int", "bool"}


def strip_cpp_comments(source: str) -> str:
    """Removes C++ single-line and multi-line comments while preserving character offsets and newlines."""
    chars = list(source)
    n = len(chars)
    i = 0
    in_str = None
    while i < n:
        c = chars[i]
        if in_str:
            if c == "\\":
                i += 2
                continue
            elif c == in_str:
                in_str = None
        else:
            if c in ("\"", "'"):
                in_str = c
            elif c == "/" and i + 1 < n:
                if chars[i + 1] == "/":
                    while i < n and chars[i] != "\n":
                        chars[i] = " "
                        i += 1
                    continue
                elif chars[i + 1] == "*":
                    chars[i] = " "
                    chars[i + 1] = " "
                    i += 2
                    while i + 1 < n and not (chars[i] == "*" and chars[i + 1] == "/"):
                        if chars[i] != "\n":
                            chars[i] = " "
                        i += 1
                    if i + 1 < n:
                        chars[i] = " "
                        chars[i + 1] = " "
                        i += 1
        i += 1
    return "".join(chars)


def find_matching_brace(source: str, start: int) -> int:
    """Given index of '{', returns index right after matching '}'."""
    depth = 0
    i = start
    n = len(source)
    in_str = None
    while i < n:
        c = source[i]
        if in_str:
            if c == "\\":
                i += 2
                continue
            elif c == in_str:
                in_str = None
        else:
            if c in ("\"", "'"):
                in_str = c
            elif c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    return i + 1
        i += 1
    return -1


def find_enclosing_function_scope(clean: str, pos: int) -> tuple[str, int] | None:
    """Finds the declaration header and opening brace index of the innermost function enclosing pos."""
    curr = pos
    while curr > 0:
        depth = 0
        i = curr - 1
        brace_start = -1
        while i >= 0:
            c = clean[i]
            if c == "}":
                depth += 1
            elif c == "{":
                if depth > 0:
                    depth -= 1
                else:
                    brace_start = i
                    break
            i -= 1
        if brace_start == -1:
            return None

        j = brace_start - 1
        while j >= 0 and clean[j] not in (";", "}", "{"):
            j -= 1
        header = clean[j + 1 : brace_start].strip()
        tokens = re.findall(r"\b[A-Za-z0-9_]+\b", header)
        if tokens and tokens[0] in CONTROL_KEYWORDS:
            curr = brace_start
            continue
        if tokens and tokens[0] in ("namespace", "class", "struct", "enum"):
            return None
        return header, brace_start
    return None


def is_declaration(line: str, match_start: int) -> bool:
    """Checks if a match of Status = is a variable/member declaration rather than a mutating assignment."""
    prefix = line[:match_start].strip()
    if not prefix:
        return False
    if prefix.endswith("&") or prefix.endswith("*"):
        return True
    last_word = prefix.split()[-1]
    return last_word in DECL_KEYWORDS or last_word.endswith("Status")


def get_depth1_content(body: str) -> str:
    """Returns only text at depth 1 inside a brace body, ignoring nested { ... } blocks."""
    depth = 0
    parts: list[str] = []
    i = 0
    n = len(body)
    in_str = None
    while i < n:
        char = body[i]
        if in_str:
            if char == "\\":
                i += 2
                continue
            elif char == in_str:
                in_str = None
        else:
            if char in ("\"", "'"):
                in_str = char
            elif char == "{":
                depth += 1
                if depth == 1:
                    last_start = i + 1
            elif char == "}":
                depth -= 1
                if depth == 1:
                    last_start = i + 1
                elif depth == 0:
                    parts.append(body[last_start:i])
                    break
            else:
                if depth == 1:
                    parts.append(char)
        i += 1
    return "".join(parts)


def validate_state_mutation_encapsulation(files_to_check: dict[str, str]) -> list[str]:
    """Ensures Status.SessionState and Status.ApplicationState mutations only happen via TryTransition*."""
    errors: list[str] = []
    field_pattern = re.compile(r"(?:(?:\.|->)\s*|\b)(SessionState|ApplicationState)\s*=(?!=)")
    struct_pattern = re.compile(r"(?:(?:\.|->)\s*Status|\b(?:Status|InOutStatus))\s*=(?!=)")

    for file_name, content in files_to_check.items():
        # Allow struct member default initialization in GV2BridgeTypes.h
        if "GV2BridgeTypes.h" in file_name:
            continue
        # Skip test files
        if "Tests" in file_name or "Test" in file_name:
            continue

        clean_content = strip_cpp_comments(content)
        is_transition_cpp = "GV2SessionTransition.cpp" in file_name

        # CFC-07: Coordinator and other components must not expose mutable reference to transition policy
        if re.search(r"(?<!const\s)\bFGV2SessionTransitionPolicy\s*&\s*Get[A-Za-z0-9_]*\s*\(", clean_content):
            errors.append(
                f"{file_name}: Forbidden non-const FGV2SessionTransitionPolicy& getter detected. "
                f"Transition policy mutation must be strictly encapsulated."
            )

        offset = 0
        for line_idx, line in enumerate(clean_content.splitlines(keepends=True), start=1):
            stripped = line.strip()
            if stripped:
                field_match = field_pattern.search(line)
                if field_match:
                    target_field = field_match.group(1)
                    match_offset = offset + field_match.start()
                    if is_transition_cpp:
                        scope = find_enclosing_function_scope(clean_content, match_offset)
                        header = scope[0] if scope else ""
                        if "TryTransitionSessionState" in header:
                            if target_field != "SessionState":
                                errors.append(
                                    f"{file_name}:{line_idx}: Forbidden mutation of {target_field} inside "
                                    f"TryTransitionSessionState ('{stripped}')."
                                )
                        elif "TryTransitionApplicationState" in header:
                            if target_field != "ApplicationState":
                                errors.append(
                                    f"{file_name}:{line_idx}: Forbidden mutation of {target_field} inside "
                                    f"TryTransitionApplicationState ('{stripped}')."
                                )
                        else:
                            errors.append(
                                f"{file_name}:{line_idx}: Direct mutation of {target_field} detected ('{stripped}'). "
                                f"All state mutations must use TryTransitionSessionState or TryTransitionApplicationState."
                            )
                    else:
                        errors.append(
                            f"{file_name}:{line_idx}: Direct mutation of {target_field} detected ('{stripped}'). "
                            f"All state mutations must use TryTransitionSessionState or TryTransitionApplicationState."
                        )

                struct_match = struct_pattern.search(line)
                if struct_match and not is_declaration(line, struct_match.start()):
                    errors.append(
                        f"{file_name}:{line_idx}: Whole-struct assignment to session status detected ('{stripped}'). "
                        f"State transitions must not overwrite status struct directly."
                    )
            offset += len(line)

    return errors


def validate_exhaustive_transition_switches(files_to_check: dict[str, str]) -> list[str]:
    """Ensures all switches on transition enums (ESessionTransitionKind, ESessionStartMode) across all files
    are exhaustive with no default: label and no surrogate fallback return statements."""
    errors: list[str] = []
    switch_pattern = re.compile(r"\bswitch\s*\(([^)]+)\)\s*\{", re.MULTILINE)

    for file_name, content in files_to_check.items():
        if "Tests" in file_name or "Test" in file_name:
            continue

        clean = strip_cpp_comments(content)
        for m in switch_pattern.finditer(clean):
            expr = m.group(1).strip()
            brace_start = m.end() - 1
            brace_end = find_matching_brace(clean, brace_start)
            if brace_end == -1:
                continue

            body = clean[brace_start:brace_end]
            depth1 = get_depth1_content(body)

            is_transition_switch = (
                "ESessionTransitionKind" in expr
                or "TransitionKind" in expr
                or "case ESessionTransitionKind::" in depth1
                or "ESessionStartMode" in expr
                or "case ESessionStartMode::" in depth1
            )

            if is_transition_switch:
                if re.search(r"\bdefault\s*:", depth1):
                    line_no = clean[: m.start()].count("\n") + 1
                    errors.append(
                        f"{file_name}:{line_no}: switch ({expr}) contains forbidden 'default:' label. "
                        f"Exhaustive switch without default is required."
                    )
                tail = clean[brace_end:].lstrip()
                if tail.startswith("return"):
                    return_stmt = tail.split(";")[0].strip() + ";"
                    line_no = clean[: brace_end].count("\n") + 1
                    errors.append(
                        f"{file_name}:{line_no}: switch ({expr}) is followed by a surrogate default fallback "
                        f"return statement ('{return_stmt}'). Exhaustive switch without fallback is required."
                    )

    return errors


def validate_single_vm_invariant(runtime_session_cpp: str, other_sources: dict[str, str]) -> list[str]:
    """Ensures luaL_newstate is strictly guarded by GLiveVmCount and only called in GV2RuntimeSession.cpp."""
    errors: list[str] = []

    # 1. No other file in Source/ may call luaL_newstate
    for file_name, content in other_sources.items():
        if "GV2RuntimeSession.cpp" in file_name:
            continue
        if "Tests" in file_name or "Test" in file_name:
            continue
        clean_content = strip_cpp_comments(content)
        if re.search(r"\bluaL_newstate\s*\(", clean_content):
            errors.append(
                f"{file_name}: Forbidden call to luaL_newstate() found. "
                f"All Lua VM creation must go through GV2RuntimeSession."
            )

    clean_runtime = strip_cpp_comments(runtime_session_cpp)

    # 2. GV2RuntimeSession.cpp must have atomic GLiveVmCount
    if not re.search(r"\bGLiveVmCount\b", clean_runtime):
        errors.append("GV2RuntimeSession.cpp: Missing atomic GLiveVmCount counter.")

    # 3. All occurrences of luaL_newstate must be preceded by GLiveVmCount check/guard in enclosing function
    newstate_matches = list(re.finditer(r"\bluaL_newstate\s*\(", clean_runtime))
    if not newstate_matches:
        errors.append("GV2RuntimeSession.cpp: luaL_newstate() not found.")
    else:
        for m in newstate_matches:
            call_pos = m.start()
            line_no = clean_runtime[:call_pos].count("\n") + 1
            scope = find_enclosing_function_scope(clean_runtime, call_pos)
            if not scope:
                errors.append(
                    f"GV2RuntimeSession.cpp:{line_no}: luaL_newstate() is not inside a recognized function definition."
                )
            else:
                header, brace_start = scope
                func_prefix = clean_runtime[brace_start:call_pos]
                func_name = header.splitlines()[-1].strip()
                if not re.search(r"(\+\+\s*GLiveVmCount|GLiveVmCount\s*\+\+|GLiveVmCount\.fetch_add)", func_prefix):
                    errors.append(
                        f"GV2RuntimeSession.cpp:{line_no}: luaL_newstate() in \"{func_name}\" must be guarded by "
                        f"GLiveVmCount check and atomic increment."
                    )

    # 4. GLiveVmCount must be decremented in teardown
    if (
        "GLiveVmCount.fetch_sub" not in clean_runtime
        and "--GLiveVmCount" not in clean_runtime
        and "GLiveVmCount--" not in clean_runtime
    ):
        errors.append("GV2RuntimeSession.cpp: Missing decrement of GLiveVmCount on VM destruction.")

    return errors


def validate_transition_policy_interface(
    header_content: str,
    impl_content: str,
    tests_content: str | None = None
) -> list[str]:
    """Ensures FGV2SessionTransitionPolicy and FGV2SessionTransitionOracle adhere to architectural invariants."""
    errors: list[str] = []
    clean_header = strip_cpp_comments(header_content)

    # Required classes and enums
    if not re.search(r"\benum\s+class\s+ESessionTransitionKind\b", clean_header):
        errors.append("GV2SessionTransition.h: Missing enum class ESessionTransitionKind.")

    # Check SessionTransitionKindCount and AllSessionTransitionKinds
    if "SessionTransitionKindCount" not in clean_header:
        errors.append("GV2SessionTransition.h: Missing SessionTransitionKindCount constant.")
    if "AllSessionTransitionKinds" not in clean_header:
        errors.append("GV2SessionTransition.h: Missing AllSessionTransitionKinds array.")

    if not re.search(r"\bstruct\s+(?:GV2_API\s+)?FGV2SessionTransitionOracle\b", clean_header):
        errors.append("GV2SessionTransition.h: Missing FGV2SessionTransitionOracle declaration.")

    # Structural check: extract class body of FGV2SessionTransitionPolicy
    policy_match = re.search(r"\bclass\s+(?:GV2_API\s+)?FGV2SessionTransitionPolicy\b[^{]*\{", clean_header)
    if not policy_match:
        errors.append("GV2SessionTransition.h: Missing FGV2SessionTransitionPolicy class declaration.")
    else:
        brace_start = policy_match.end() - 1
        brace_end = find_matching_brace(clean_header, brace_start)
        if brace_end == -1:
            errors.append("GV2SessionTransition.h: Malformed FGV2SessionTransitionPolicy class body.")
        else:
            class_body = clean_header[brace_start:brace_end]
            required_methods = [
                "EnqueueRequest",
                "EnqueueShutdown",
                "CancelRequest",
                "GetOutcome",
                "DequeuePendingOperation",
                "GetActiveOperation",
            ]
            for method in required_methods:
                if not re.search(rf"\b{re.escape(method)}\s*\(", class_body):
                    errors.append(
                        f"GV2SessionTransition.h: FGV2SessionTransitionPolicy missing structural member declaration for '{method}'."
                    )

    # Exhaustive switch check for transition header and implementation
    errors.extend(validate_exhaustive_transition_switches({
        "GV2SessionTransition.h": header_content,
        "GV2SessionTransition.cpp": impl_content,
    }))

    # In tests: verify presence of automation tests exercising behavioral invariants
    if tests_content is not None:
        required_test_identifiers = [
            ("request joining", "RequestJoining"),
            ("pending slot superseding", "PendingSuperseded"),
            ("point-of-no-return cancellation", "Cancellation"),
            ("priority shutdown", "ShutdownPriority"),
        ]
        for desc, identifier in required_test_identifiers:
            if identifier not in tests_content:
                errors.append(
                    f"GV2SessionTransitionTests.cpp: Missing behavioral test for {desc} (expected '{identifier}')."
                )

    return errors


def run_self_tests() -> bool:
    """Executes positive and negative validation fixtures."""
    all_passed = True

    # Test 1: Negative - Direct SessionState mutation in coordinator
    mock_bad_coord = """
    void FGV2SessionCoordinator::SneakySet() {
        Status.SessionState = EGV2SessionState::Ready;
    }
    """
    errs = validate_state_mutation_encapsulation({"GV2SessionCoordinator.cpp": mock_bad_coord})
    if not any("Direct mutation of SessionState" in e for e in errs):
        print("FAIL: Expected error for direct SessionState mutation", file=sys.stderr)
        all_passed = False

    # Test 2: Negative - Direct ApplicationState mutation in coordinator
    mock_bad_app = """
    void FGV2SessionCoordinator::SneakySetApp() {
        Status.ApplicationState = EGV2ApplicationState::MenuActive;
    }
    """
    errs = validate_state_mutation_encapsulation({"GV2SessionCoordinator.cpp": mock_bad_app})
    if not any("Direct mutation of ApplicationState" in e for e in errs):
        print("FAIL: Expected error for direct ApplicationState mutation", file=sys.stderr)
        all_passed = False

    # Test 3: Negative - Pointer arrow mutation (Coord->SessionState = ...)
    mock_ptr_coord = """
    void SetState(FGV2SessionCoordinator* Coord) {
        Coord->SessionState = EGV2SessionState::Ready;
    }
    """
    errs = validate_state_mutation_encapsulation({"GV2External.cpp": mock_ptr_coord})
    if not any("Direct mutation of SessionState" in e for e in errs):
        print("FAIL: Expected error for pointer arrow SessionState mutation", file=sys.stderr)
        all_passed = False

    # Test 4: Negative - Whole struct mutation (Status = Other;)
    mock_whole_struct = """
    void Overwrite(FGV2SessionCoordinator& Coord, const FGV2SessionStatus& Other) {
        Coord.Status = Other;
    }
    """
    errs = validate_state_mutation_encapsulation({"GV2External.cpp": mock_whole_struct})
    if not any("Whole-struct assignment to session status" in e for e in errs):
        print("FAIL: Expected error for whole-struct assignment to Status", file=sys.stderr)
        all_passed = False

    # Test 5: Negative - Whole struct mutation via InOutStatus = Other;
    mock_inout_struct = """
    void ResetStatus(FGV2SessionStatus& InOutStatus) {
        InOutStatus = {};
    }
    """
    errs = validate_state_mutation_encapsulation({"GV2External.cpp": mock_inout_struct})
    if not any("Whole-struct assignment to session status" in e for e in errs):
        print("FAIL: Expected error for whole-struct assignment to InOutStatus", file=sys.stderr)
        all_passed = False

    # Test 6: Negative - State mutation in GV2SessionTransition.cpp outside TryTransition*
    mock_transition_oracle_bad = """
    bool FGV2SessionTransitionOracle::CanTransitionSessionState(
        EGV2SessionState CurrentState,
        EGV2SessionState TargetState,
        ESessionTransitionKind TransitionKind)
    {
        InOutStatus.SessionState = TargetState;
        return true;
    }
    """
    errs = validate_state_mutation_encapsulation({"GV2SessionTransition.cpp": mock_transition_oracle_bad})
    if not any("Direct mutation of SessionState detected" in e for e in errs):
        print("FAIL: Expected error for mutation in GV2SessionTransition.cpp outside TryTransition*", file=sys.stderr)
        all_passed = False

    # Test 7: Negative - Inverted mutation in TryTransitionSessionState (assigning ApplicationState)
    mock_inverted_transition = """
    bool TryTransitionSessionState(
        FGV2SessionStatus& InOutStatus,
        const EGV2SessionState TargetState,
        const ESessionTransitionKind TransitionKind,
        FString* OutError)
    {
        InOutStatus.ApplicationState = TargetState;
        return true;
    }
    """
    errs = validate_state_mutation_encapsulation({"GV2SessionTransition.cpp": mock_inverted_transition})
    if not any("Forbidden mutation of ApplicationState inside TryTransitionSessionState" in e for e in errs):
        print("FAIL: Expected error for ApplicationState mutation inside TryTransitionSessionState", file=sys.stderr)
        all_passed = False

    # Test 8: Positive - Valid mutations in GV2SessionTransition.cpp inside TryTransition*
    mock_valid_transitions = """
    bool TryTransitionSessionState(
        FGV2SessionStatus& InOutStatus,
        const EGV2SessionState TargetState,
        const ESessionTransitionKind TransitionKind,
        FString* OutError)
    {
        InOutStatus.SessionState = TargetState;
        return true;
    }

    bool TryTransitionApplicationState(
        FGV2SessionStatus& InOutStatus,
        const EGV2ApplicationState TargetState,
        FString* OutError)
    {
        InOutStatus.ApplicationState = TargetState;
        return true;
    }
    """
    errs = validate_state_mutation_encapsulation({"GV2SessionTransition.cpp": mock_valid_transitions})
    if errs:
        print(f"FAIL: Expected valid transitions to pass cleanly, got: {errs}", file=sys.stderr)
        all_passed = False

    # Test 9: Negative - Switch on TransitionKind with default:
    mock_bad_switch = """
    void Foo(ESessionTransitionKind TransitionKind) {
        switch (TransitionKind) {
        case ESessionTransitionKind::Menu: break;
        default: break;
        }
    }
    """
    errs = validate_exhaustive_transition_switches({"GV2SessionTransition.cpp": mock_bad_switch})
    if not any("contains forbidden 'default:' label" in e for e in errs):
        print("FAIL: Expected error for default: in switch (TransitionKind)", file=sys.stderr)
        all_passed = False

    # Test 10: Negative - Switch with nested { ... } before default:
    mock_nested_block_switch = """
    void Foo(ESessionTransitionKind TransitionKind) {
        switch (TransitionKind) {
        case ESessionTransitionKind::Menu: {
            int x = 42;
            break;
        }
        default:
            break;
        }
    }
    """
    errs = validate_exhaustive_transition_switches({"GV2SessionTransition.cpp": mock_nested_block_switch})
    if not any("contains forbidden 'default:' label" in e for e in errs):
        print("FAIL: Expected error for default: after nested block in switch", file=sys.stderr)
        all_passed = False

    # Test 11: Negative - Switch on Kind (different variable name) with default:
    mock_kind_switch = """
    void Foo(ESessionTransitionKind Kind) {
        switch (Kind) {
        case ESessionTransitionKind::Menu:
            break;
        default:
            break;
        }
    }
    """
    errs = validate_exhaustive_transition_switches({"GV2SessionCoordinator.cpp": mock_kind_switch})
    if not any("contains forbidden 'default:' label" in e for e in errs):
        print("FAIL: Expected error for default: in switch (Kind)", file=sys.stderr)
        all_passed = False

    # Test 12: Negative - Switch with default: in external file
    errs = validate_exhaustive_transition_switches({"GV2OtherFile.cpp": mock_bad_switch})
    if not any("contains forbidden 'default:' label" in e for e in errs):
        print("FAIL: Expected error for default: in external file", file=sys.stderr)
        all_passed = False

    # Test 13: Negative - Switch in header followed by surrogate default fallback return (ToTransitionKind regression)
    mock_header_surrogate_fallback_mode = """
    inline ESessionTransitionKind ToTransitionKind(ESessionStartMode Mode)
    {
        switch (Mode)
        {
        case ESessionStartMode::Menu:
            return ESessionTransitionKind::Menu;
        case ESessionStartMode::NewGame:
            return ESessionTransitionKind::NewGame;
        case ESessionStartMode::LoadSave:
            return ESessionTransitionKind::LoadSave;
        }
        return ESessionTransitionKind::NewGame;
    }
    """
    errs = validate_exhaustive_transition_switches({"GV2SessionTransition.h": mock_header_surrogate_fallback_mode})
    if not any("is followed by a surrogate default fallback return statement" in e for e in errs):
        print("FAIL: Expected error for surrogate fallback return after switch (Mode)", file=sys.stderr)
        all_passed = False

    # Test 14: Negative - LexToString in header followed by surrogate default fallback return
    mock_header_surrogate_fallback_lex = """
    inline const TCHAR* LexToString(ESessionTransitionKind Kind)
    {
        switch (Kind)
        {
        case ESessionTransitionKind::Menu:
            return TEXT("Menu");
        case ESessionTransitionKind::NewGame:
            return TEXT("NewGame");
        case ESessionTransitionKind::LoadSave:
            return TEXT("LoadSave");
        case ESessionTransitionKind::Shutdown:
            return TEXT("Shutdown");
        }
        return TEXT("Unknown");
    }
    """
    errs = validate_exhaustive_transition_switches({"GV2SessionTransition.h": mock_header_surrogate_fallback_lex})
    if not any("is followed by a surrogate default fallback return statement" in e for e in errs):
        print("FAIL: Expected error for surrogate fallback return after switch (Kind)", file=sys.stderr)
        all_passed = False

    # Test 15: Negative - Header switch containing forbidden default: caught by validate_transition_policy_interface
    mock_header_with_default = """
    enum class ESessionTransitionKind { Menu, NewGame, LoadSave, Shutdown };
    inline constexpr uint8 SessionTransitionKindCount = 4;
    inline constexpr ESessionTransitionKind AllSessionTransitionKinds[4] = {};
    inline const TCHAR* LexToString(ESessionTransitionKind Kind)
    {
        switch (Kind)
        {
        case ESessionTransitionKind::Menu: return TEXT("Menu");
        default: return TEXT("Unknown");
        }
    }
    struct GV2_API FGV2SessionTransitionOracle {};
    class GV2_API FGV2SessionTransitionPolicy {
    public:
        void EnqueueRequest();
        void EnqueueShutdown();
        void CancelRequest();
        void GetOutcome();
        void DequeuePendingOperation();
        void GetActiveOperation();
    };
    """
    errs = validate_transition_policy_interface(mock_header_with_default, "void Foo() {}")
    if not any("contains forbidden 'default:' label" in e for e in errs):
        print("FAIL: Expected validate_transition_policy_interface to catch default: in header", file=sys.stderr)
        all_passed = False

    # Test 16: Negative - Header switch with surrogate fallback caught by validate_transition_policy_interface
    mock_header_with_surrogate = """
    enum class ESessionTransitionKind { Menu, NewGame, LoadSave, Shutdown };
    inline constexpr uint8 SessionTransitionKindCount = 4;
    inline constexpr ESessionTransitionKind AllSessionTransitionKinds[4] = {};
    inline ESessionTransitionKind ToTransitionKind(ESessionStartMode Mode)
    {
        switch (Mode)
        {
        case ESessionStartMode::Menu: return ESessionTransitionKind::Menu;
        case ESessionStartMode::NewGame: return ESessionTransitionKind::NewGame;
        case ESessionStartMode::LoadSave: return ESessionTransitionKind::LoadSave;
        }
        return ESessionTransitionKind::NewGame;
    }
    struct GV2_API FGV2SessionTransitionOracle {};
    class GV2_API FGV2SessionTransitionPolicy {
    public:
        void EnqueueRequest();
        void EnqueueShutdown();
        void CancelRequest();
        void GetOutcome();
        void DequeuePendingOperation();
        void GetActiveOperation();
    };
    """
    errs = validate_transition_policy_interface(mock_header_with_surrogate, "void Foo() {}")
    if not any("is followed by a surrogate default fallback return statement" in e for e in errs):
        print("FAIL: Expected validate_transition_policy_interface to catch surrogate fallback in header", file=sys.stderr)
        all_passed = False

    # Test 17: Negative - luaL_newstate without GLiveVmCount guard
    mock_bad_runtime = """
    lua_State* CreateVm() {
        return luaL_newstate();
    }
    """
    errs = validate_single_vm_invariant(mock_bad_runtime, {})
    if not any("GLiveVmCount" in e for e in errs):
        print("FAIL: Expected error for unguarded luaL_newstate()", file=sys.stderr)
        all_passed = False

    # Test 18: Negative - Second luaL_newstate call without guard
    mock_multi_runtime = """
    std::atomic<int> GLiveVmCount{0};
    void OpenFirst() {
        if (GLiveVmCount.fetch_add(1) != 0) return;
        luaL_newstate();
    }
    void OpenSecond() {
        luaL_newstate();
    }
    void Close() {
        GLiveVmCount.fetch_sub(1);
    }
    """
    errs = validate_single_vm_invariant(mock_multi_runtime, {})
    if not any("luaL_newstate() in \"void OpenSecond()\" must be guarded" in e for e in errs):
        print("FAIL: Expected error for second unguarded luaL_newstate()", file=sys.stderr)
        all_passed = False

    # Test 19: Negative - luaL_newstate in external file
    mock_ext = {"ExternalModule.cpp": "void Foo() { luaL_newstate(); }"}
    mock_good_runtime = """
    std::atomic<int> GLiveVmCount{0};
    void Open() {
        if (GLiveVmCount.fetch_add(1) != 0) { return; }
        luaL_newstate();
    }
    void Close() {
        GLiveVmCount.fetch_sub(1);
    }
    """
    errs = validate_single_vm_invariant(mock_good_runtime, mock_ext)
    if not any("Forbidden call to luaL_newstate()" in e for e in errs):
        print("FAIL: Expected error for external luaL_newstate()", file=sys.stderr)
        all_passed = False

    # Test 20: Negative - Required method only in comment inside header
    mock_header_comment = """
    enum class ESessionTransitionKind { Menu, NewGame, LoadSave, Shutdown };
    inline constexpr uint8 SessionTransitionKindCount = 4;
    inline constexpr ESessionTransitionKind AllSessionTransitionKinds[4] = {};
    struct GV2_API FGV2SessionTransitionOracle {};
    class GV2_API FGV2SessionTransitionPolicy {
    public:
        // void EnqueueRequest();
        void EnqueueShutdown();
        void CancelRequest();
        void GetOutcome();
        void DequeuePendingOperation();
        void GetActiveOperation();
    };
    """
    errs = validate_transition_policy_interface(mock_header_comment, "void Foo() {}")
    if not any("missing structural member declaration for 'EnqueueRequest'" in e for e in errs):
        print("FAIL: Expected error when EnqueueRequest is only in a comment", file=sys.stderr)
        all_passed = False

    # Test 21: Negative - Missing SessionTransitionKindCount / AllSessionTransitionKinds
    mock_header_missing_count = """
    enum class ESessionTransitionKind { Menu, NewGame, LoadSave, Shutdown };
    struct GV2_API FGV2SessionTransitionOracle {};
    class GV2_API FGV2SessionTransitionPolicy {
    public:
        void EnqueueRequest();
        void EnqueueShutdown();
        void CancelRequest();
        void GetOutcome();
        void DequeuePendingOperation();
        void GetActiveOperation();
    };
    """
    errs = validate_transition_policy_interface(mock_header_missing_count, "void Foo() {}")
    if not any("Missing SessionTransitionKindCount" in e for e in errs):
        print("FAIL: Expected error for missing SessionTransitionKindCount", file=sys.stderr)
        all_passed = False

    # Test 22: Negative - Missing required behavioral test in GV2SessionTransitionTests.cpp
    mock_header = """
    enum class ESessionTransitionKind { Menu, NewGame, LoadSave, Shutdown };
    inline constexpr uint8 SessionTransitionKindCount = 4;
    inline constexpr ESessionTransitionKind AllSessionTransitionKinds[4] = {};
    struct GV2_API FGV2SessionTransitionOracle {};
    class GV2_API FGV2SessionTransitionPolicy {
    public:
        void EnqueueRequest();
        void EnqueueShutdown();
        void CancelRequest();
        void GetOutcome();
        void DequeuePendingOperation();
        void GetActiveOperation();
    };
    """
    mock_bad_tests = """
    IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSomeTest, "GV2.Test", 0)
    bool FSomeTest::RunTest() { return true; }
    """
    errs = validate_transition_policy_interface(mock_header, "void Foo() {}", mock_bad_tests)
    if not any("Missing behavioral test" in e for e in errs):
        print("FAIL: Expected error for missing behavioral tests", file=sys.stderr)
        all_passed = False

    # Test 23: Negative - Non-const FGV2SessionTransitionPolicy& getter on coordinator
    mock_bad_coord_policy = """
    class FGV2SessionCoordinator {
    public:
        FGV2SessionTransitionPolicy& GetTransitionPolicy() { return TransitionPolicy; }
    };
    """
    errs = validate_state_mutation_encapsulation({"GV2SessionCoordinator.h": mock_bad_coord_policy})
    if not any("Forbidden non-const FGV2SessionTransitionPolicy& getter" in e for e in errs):
        print("FAIL: Expected error for non-const FGV2SessionTransitionPolicy& getter", file=sys.stderr)
        all_passed = False

    if all_passed:
        print("All self-tests passed!")
    return all_passed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true", help="Run self-tests")
    args = parser.parse_args()

    if args.self_test:
        return 0 if run_self_tests() else 1

    # Gather source files in GV2
    gv2_files: dict[str, str] = {}
    for ext in ("*.h", "*.cpp"):
        for path in GV2_SOURCE_DIR.rglob(ext):
            try:
                gv2_files[str(path.relative_to(REPO_ROOT))] = path.read_text(encoding="utf-8")
            except Exception:
                pass

    all_errors: list[str] = []

    # 1. Validate state mutation encapsulation across GV2
    all_errors.extend(validate_state_mutation_encapsulation(gv2_files))

    # 2. Validate exhaustive switches on transition enums across GV2
    all_errors.extend(validate_exhaustive_transition_switches(gv2_files))

    # 3. Validate single Lua VM invariant in GV2RuntimeCore
    runtime_session_cpp = RUNTIME_SESSION_IMPL_PATH.read_text(encoding="utf-8")
    all_errors.extend(validate_single_vm_invariant(runtime_session_cpp, gv2_files))

    # 4. Validate transition policy interface
    transition_h = TRANSITION_HEADER_PATH.read_text(encoding="utf-8")
    transition_cpp = TRANSITION_IMPL_PATH.read_text(encoding="utf-8")
    tests_cpp = TESTS_PATH.read_text(encoding="utf-8") if TESTS_PATH.exists() else None
    all_errors.extend(validate_transition_policy_interface(transition_h, transition_cpp, tests_cpp))

    if all_errors:
        print("Session transition ownership validation failed:", file=sys.stderr)
        for err in all_errors:
            print(f"  - {err}", file=sys.stderr)
        return 1

    print("Session transition ownership validation passed cleanly.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
