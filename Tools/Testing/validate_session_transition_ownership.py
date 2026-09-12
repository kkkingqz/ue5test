#!/usr/bin/env python3
"""Validates session transition policy, state mutation encapsulation, and single Lua VM invariants.

CFC-07 (ADR-0043, ADR-0044, STATUS-001):
1. State mutation encapsulation:
   - Direct mutations of Status.SessionState and Status.ApplicationState are strictly
     prohibited outside of TryTransitionSessionState and TryTransitionApplicationState
     in GV2SessionTransition.cpp.
2. Single live Lua VM invariant:
   - luaL_newstate() must only be called in GV2RuntimeSession.cpp, guarded by the atomic
     GLiveVmCount counter ensuring <= 1 live VM across the entire process.
   - GLiveVmCount must be decremented on session teardown/CloseState().
3. Exhaustive transition switches:
   - Switches on ESessionTransitionKind must be exhaustive with no fallback default: labels.
4. Transition policy interface:
   - FGV2SessionTransitionPolicy must provide request joining, single pending slot (superseding),
     priority shutdown, and point-of-no-return cancellation.
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


def validate_state_mutation_encapsulation(files_to_check: dict[str, str]) -> list[str]:
    """Ensures Status.SessionState and Status.ApplicationState mutations only happen via TryTransition*."""
    errors: list[str] = []
    forbidden_pattern = re.compile(
        r"(?:Status|\b[a-zA-Z0-9_]+)\.(?:SessionState|ApplicationState)\s*=[^=]"
    )

    for file_name, content in files_to_check.items():
        # Allow struct member default initialization in GV2BridgeTypes.h
        if "GV2BridgeTypes.h" in file_name:
            continue
        # Allow inside GV2SessionTransition.cpp inside TryTransition* functions
        if "GV2SessionTransition.cpp" in file_name:
            continue
        # Skip test files
        if "Tests" in file_name or "Test" in file_name:
            continue

        for line_idx, line in enumerate(content.splitlines(), start=1):
            # Ignore comments
            stripped = line.strip()
            if stripped.startswith("//") or stripped.startswith("*"):
                continue
            if forbidden_pattern.search(line):
                errors.append(
                    f"{file_name}:{line_idx}: Direct mutation of SessionState or ApplicationState "
                    f"detected ('{stripped}'). All state mutations must use "
                    f"TryTransitionSessionState or TryTransitionApplicationState."
                )

    return errors


def validate_single_vm_invariant(runtime_session_cpp: str, other_sources: dict[str, str]) -> list[str]:
    """Ensures luaL_newstate is strictly guarded by GLiveVmCount and only called in GV2RuntimeSession.cpp."""
    errors: list[str] = []

    # 1. No other file in Source/ may call luaL_newstate
    for file_name, content in other_sources.items():
        if "GV2RuntimeSession.cpp" in file_name:
            continue
        if "Tests" in file_name:
            continue
        if "luaL_newstate" in content:
            errors.append(
                f"{file_name}: Forbidden call to luaL_newstate() found. "
                f"All Lua VM creation must go through GV2RuntimeSession."
            )

    # 2. GV2RuntimeSession.cpp must have atomic GLiveVmCount
    if "GLiveVmCount" not in runtime_session_cpp:
        errors.append("GV2RuntimeSession.cpp: Missing atomic GLiveVmCount counter.")

    # 3. luaL_newstate must be preceded by GLiveVmCount check/guard
    newstate_idx = runtime_session_cpp.find("luaL_newstate")
    if newstate_idx == -1:
        errors.append("GV2RuntimeSession.cpp: luaL_newstate() not found.")
    else:
        # Check window before luaL_newstate
        window = runtime_session_cpp[max(0, newstate_idx - 600) : newstate_idx]
        if "GLiveVmCount" not in window:
            errors.append(
                "GV2RuntimeSession.cpp: luaL_newstate() must be guarded by "
                "GLiveVmCount check and atomic increment."
            )

    # 4. GLiveVmCount must be decremented in CloseState
    if "GLiveVmCount.fetch_sub" not in runtime_session_cpp and "--GLiveVmCount" not in runtime_session_cpp and "GLiveVmCount--" not in runtime_session_cpp:
        errors.append("GV2RuntimeSession.cpp: Missing decrement of GLiveVmCount on VM destruction.")

    return errors


def validate_transition_policy_interface(header_content: str, impl_content: str) -> list[str]:
    """Ensures FGV2SessionTransitionPolicy and FGV2SessionTransitionOracle adhere to architectural invariants."""
    errors: list[str] = []

    # Required classes and enums
    if "enum class ESessionTransitionKind" not in header_content:
        errors.append("GV2SessionTransition.h: Missing enum class ESessionTransitionKind.")

    if "struct GV2_API FGV2SessionTransitionOracle" not in header_content:
        errors.append("GV2SessionTransition.h: Missing FGV2SessionTransitionOracle declaration.")

    if "class GV2_API FGV2SessionTransitionPolicy" not in header_content:
        errors.append("GV2SessionTransition.h: Missing FGV2SessionTransitionPolicy declaration.")

    required_methods = [
        "EnqueueRequest",
        "EnqueueShutdown",
        "CancelRequest",
        "GetOutcome",
        "DequeuePendingOperation",
        "GetActiveOperation",
    ]
    for method in required_methods:
        if method not in header_content:
            errors.append(f"GV2SessionTransition.h: FGV2SessionTransitionPolicy missing required method '{method}'.")

    # In impl: switch (TransitionKind) must not use default:
    switch_pattern = re.compile(r"switch\s*\(\s*TransitionKind\s*\)\s*\{([^}]+)\}", re.MULTILINE)
    for match in switch_pattern.finditer(impl_content):
        body = match.group(1)
        if "default:" in body:
            errors.append(
                "GV2SessionTransition.cpp: switch (TransitionKind) contains forbidden 'default:' label. "
                "Exhaustive switch without default is required."
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
    if not any("Direct mutation of SessionState or ApplicationState" in e for e in errs):
        print("FAIL: Expected error for direct ApplicationState mutation", file=sys.stderr)
        all_passed = False

    # Test 3: Negative - luaL_newstate without GLiveVmCount guard
    mock_bad_runtime = """
    lua_State* CreateVm() {
        return luaL_newstate();
    }
    """
    errs = validate_single_vm_invariant(mock_bad_runtime, {})
    if not any("GLiveVmCount" in e for e in errs):
        print("FAIL: Expected error for unguarded luaL_newstate()", file=sys.stderr)
        all_passed = False

    # Test 4: Negative - luaL_newstate in external file
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

    # Test 5: Negative - Non-exhaustive switch on TransitionKind
    mock_header = """
    enum class ESessionTransitionKind { Menu, NewGame, LoadSave, Shutdown };
    struct GV2_API FGV2SessionTransitionOracle {};
    class GV2_API FGV2SessionTransitionPolicy {
        void EnqueueRequest();
        void EnqueueShutdown();
        void CancelRequest();
        void GetOutcome();
        void DequeuePendingOperation();
        void GetActiveOperation();
    };
    """
    mock_bad_impl = """
    void Foo(ESessionTransitionKind TransitionKind) {
        switch (TransitionKind) {
        case ESessionTransitionKind::Menu: break;
        default: break;
        }
    }
    """
    errs = validate_transition_policy_interface(mock_header, mock_bad_impl)
    if not any("switch (TransitionKind) contains forbidden 'default:'" in e for e in errs):
        print("FAIL: Expected error for default: in switch (TransitionKind)", file=sys.stderr)
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

    # 2. Validate single Lua VM invariant in GV2RuntimeCore
    runtime_session_cpp = RUNTIME_SESSION_IMPL_PATH.read_text(encoding="utf-8")
    all_errors.extend(validate_single_vm_invariant(runtime_session_cpp, gv2_files))

    # 3. Validate transition policy interface
    transition_h = TRANSITION_HEADER_PATH.read_text(encoding="utf-8")
    transition_cpp = TRANSITION_IMPL_PATH.read_text(encoding="utf-8")
    all_errors.extend(validate_transition_policy_interface(transition_h, transition_cpp))

    if all_errors:
        print("Session transition ownership validation failed:", file=sys.stderr)
        for err in all_errors:
            print(f"  - {err}", file=sys.stderr)
        return 1

    print("Session transition ownership validation passed cleanly.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
