#!/usr/bin/env python3
"""Validates session replacement token, owner routines, and projection publication/teardown boundaries.

CFC-06 (ADR-0044 D1/D2, STATUS-014, STATUS-015, PSC-AF-04, PSC-AF-05):
1. FGV2SessionCoordinator::FDocumentSink takes (const FGV2UiDocumentViewModel&, const FGV2PresentationPrepareContext&).
2. Ambient methods GetContentSnapshotForPrepare() and InProgressCandidate are eliminated.
3. Private move-only FSessionReplacementToken governs stages: Preflight, Replacing, Preparing, Committed, Aborted.
4. Owner routines BeginReplace and PublishReady govern replacement lifecycle:
   - Host projection teardown (ProjectionTeardownSink) occurs at BeginReplace.
   - Host projection publication (ProjectionPublishSink) occurs at PublishReady.
   - ContentSnapshot mutations are restricted to BeginReplace, PublishReady, EndSession, FailRuntime.
5. UGV2RuntimeSubsystem does NOT eagerly destroy ActiveScreen/ActiveGameShell in StartSession().
6. UGV2RuntimeSubsystem does NOT synchronously load GameShellClass from settings in StartSession();
   GameShell is created from PrepareContext.GetGameShellClass() and held off-viewport until PublishReady.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
COORDINATOR_HEADER_PATH = (
    REPO_ROOT / "Source" / "GV2" / "Private" / "Application" / "GV2SessionCoordinator.h"
)
COORDINATOR_IMPL_PATH = (
    REPO_ROOT / "Source" / "GV2" / "Private" / "Application" / "GV2SessionCoordinator.cpp"
)
SUBSYSTEM_HEADER_PATH = (
    REPO_ROOT / "Source" / "GV2" / "Public" / "Runtime" / "GV2RuntimeSubsystem.h"
)
SUBSYSTEM_IMPL_PATH = (
    REPO_ROOT / "Source" / "GV2" / "Private" / "Runtime" / "GV2RuntimeSubsystem.cpp"
)


def extract_function_body(source: str, class_name: str, func_name: str) -> str | None:
    """Extracts the body of a C++ member function."""
    pattern = rf"(?:[A-Za-z0-9_:<>\s\*&]+\s+)?{re.escape(class_name)}::{re.escape(func_name)}\s*\([^)]*\)\s*(?:const)?\s*\{{"
    match = re.search(pattern, source)
    if not match:
        return None

    start = match.end() - 1
    depth = 0
    i = start
    while i < len(source):
        char = source[i]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[start : i + 1]
        elif char in ('"', "'"):
            quote = char
            i += 1
            while i < len(source) and source[i] != quote:
                if source[i] == "\\":
                    i += 1
                i += 1
        elif source[i : i + 2] == "//":
            end_line = source.find("\n", i)
            i = len(source) if end_line == -1 else end_line
            continue
        elif source[i : i + 2] == "/*":
            end_comment = source.find("*/", i + 2)
            i = len(source) if end_comment == -1 else end_comment + 1
            continue
        i += 1
    return None


def validate_coordinator_header(header_content: str) -> list[str]:
    errors: list[str] = []

    # 1. FDocumentSink takes FGV2PresentationPrepareContext
    sink_pattern = r"using\s+FDocumentSink\s*=\s*TFunction<bool\s*\(\s*const\s+FGV2UiDocumentViewModel\s*&(?:\s*\w+)?[,\s]+const\s+FGV2PresentationPrepareContext\s*&(?:\s*\w+)?\s*\)>"
    if not re.search(sink_pattern, header_content):
        errors.append(
            "FGV2SessionCoordinator::FDocumentSink must have signature: "
            "TFunction<bool(const FGV2UiDocumentViewModel&, const FGV2PresentationPrepareContext&)>"
        )

    # 2. No ambient GetContentSnapshotForPrepare
    if "GetContentSnapshotForPrepare" in header_content:
        errors.append("Forbidden ambient method GetContentSnapshotForPrepare() found in GV2SessionCoordinator.h")

    # 3. No InProgressCandidate
    if "InProgressCandidate" in header_content:
        errors.append("Forbidden ambient member InProgressCandidate found in GV2SessionCoordinator.h")

    # 4. FSessionReplacementToken defined with stages
    token_pattern = r"(?:struct|class)\s+FSessionReplacementToken"
    if not re.search(token_pattern, header_content):
        errors.append("FSessionReplacementToken class/struct definition missing in GV2SessionCoordinator.h")

    for stage in ["Preflight", "Replacing", "Preparing", "Committed", "Aborted"]:
        if stage not in header_content:
            errors.append(f"EReplacementStage::{stage} missing in GV2SessionCoordinator.h")

    # 5. Owner routines BeginReplace and PublishReady
    if not re.search(r"BeginReplace\s*\(", header_content):
        errors.append("Owner routine BeginReplace() declaration missing in GV2SessionCoordinator.h")
    if not re.search(r"PublishReady\s*\(", header_content):
        errors.append("Owner routine PublishReady() declaration missing in GV2SessionCoordinator.h")

    return errors


def validate_coordinator_impl(impl_content: str) -> list[str]:
    errors: list[str] = []

    # 1. No ambient GetContentSnapshotForPrepare or InProgressCandidate
    if "GetContentSnapshotForPrepare" in impl_content:
        errors.append("Forbidden ambient method GetContentSnapshotForPrepare found in GV2SessionCoordinator.cpp")
    if "InProgressCandidate" in impl_content:
        errors.append("Forbidden ambient member InProgressCandidate found in GV2SessionCoordinator.cpp")

    # 2. Extract allowed owner routines
    allowed_routines = ["BeginReplace", "PublishReady", "EndSession", "FailRuntime"]
    bodies: dict[str, str] = {}
    for routine in allowed_routines:
        body = extract_function_body(impl_content, "FGV2SessionCoordinator", routine)
        if not body:
            errors.append(f"Required routine FGV2SessionCoordinator::{routine} not found in GV2SessionCoordinator.cpp")
        else:
            bodies[routine] = body

    # 3. Check that ContentSnapshot modifications only occur in allowed routines
    # Find all assignments or resets to ContentSnapshot
    mutation_regex = re.compile(r"\bContentSnapshot\s*(?:=|\.Reset\s*\()")
    for match in mutation_regex.finditer(impl_content):
        offset = match.start()
        # Find which function body contains this offset
        in_allowed = False
        for routine, body in bodies.items():
            func_pos = impl_content.find(body)
            if func_pos <= offset < func_pos + len(body):
                in_allowed = True
                break
        if not in_allowed:
            line_no = impl_content.count("\n", 0, offset) + 1
            errors.append(
                f"ContentSnapshot mutation at line {line_no} is outside approved owner routines "
                f"({', '.join(allowed_routines)})"
            )

    # 4. ProjectionTeardownSink invocation must only occur in BeginReplace (or EndSession/FailRuntime)
    teardown_invoke = re.compile(r"\bProjectionTeardownSink\s*\(")
    for match in teardown_invoke.finditer(impl_content):
        offset = match.start()
        in_allowed = False
        for routine in ["BeginReplace", "EndSession", "FailRuntime"]:
            if routine in bodies:
                func_pos = impl_content.find(bodies[routine])
                if func_pos <= offset < func_pos + len(bodies[routine]):
                    in_allowed = True
                    break
        if not in_allowed:
            line_no = impl_content.count("\n", 0, offset) + 1
            errors.append(
                f"ProjectionTeardownSink invocation at line {line_no} is outside BeginReplace/EndSession/FailRuntime"
            )

    # 5. ProjectionPublishSink invocation must only occur in PublishReady
    publish_invoke = re.compile(r"\bProjectionPublishSink\s*\(")
    for match in publish_invoke.finditer(impl_content):
        offset = match.start()
        in_allowed = False
        if "PublishReady" in bodies:
            func_pos = impl_content.find(bodies["PublishReady"])
            if func_pos <= offset < func_pos + len(bodies["PublishReady"]):
                in_allowed = True
        if not in_allowed:
            line_no = impl_content.count("\n", 0, offset) + 1
            errors.append(
                f"ProjectionPublishSink invocation at line {line_no} is outside PublishReady"
            )

    return errors


def validate_subsystem(impl_content: str, header_content: str) -> list[str]:
    errors: list[str] = []

    # 1. StartSession body must NOT eagerly teardown ActiveScreen or ActiveGameShell
    start_session_body = extract_function_body(impl_content, "UGV2RuntimeSubsystem", "StartSession")
    if not start_session_body:
        errors.append("UGV2RuntimeSubsystem::StartSession body not found in GV2RuntimeSubsystem.cpp")
    else:
        if "ActiveScreen->RemoveFromParent()" in start_session_body or "ActiveGameShell->RemoveFromParent()" in start_session_body:
            errors.append(
                "UGV2RuntimeSubsystem::StartSession must NOT eagerly call RemoveFromParent() on ActiveScreen/ActiveGameShell. "
                "Teardown belongs to TeardownActiveProjection() via ProjectionTeardownSink (PSC-AF-05, STATUS-015)."
            )
        if "GetDefault<UGV2ScreenRegistrySettings>()" in start_session_body:
            errors.append(
                "UGV2RuntimeSubsystem::StartSession must NOT synchronously load GameShellClass from settings. "
                "GameShellClass must be obtained from PrepareContext.GetGameShellClass()."
            )

    # 2. TeardownActiveProjection and PublishActiveProjection exist
    teardown_body = extract_function_body(impl_content, "UGV2RuntimeSubsystem", "TeardownActiveProjection")
    if not teardown_body:
        errors.append("UGV2RuntimeSubsystem::TeardownActiveProjection not found in GV2RuntimeSubsystem.cpp")

    publish_body = extract_function_body(impl_content, "UGV2RuntimeSubsystem", "PublishActiveProjection")
    if not publish_body:
        errors.append("UGV2RuntimeSubsystem::PublishActiveProjection not found in GV2RuntimeSubsystem.cpp")

    # 3. GameShell is created from PrepareContext.GetGameShellClass()
    if "PrepareContext.GetGameShellClass()" not in impl_content:
        errors.append(
            "UGV2RuntimeSubsystem must obtain GameShellClass via PrepareContext.GetGameShellClass()."
        )

    # 4. HandleDocumentRequested must accept const FGV2PresentationPrepareContext&
    hdr_sig = r"HandleDocumentRequested\s*\(\s*const\s+FGV2UiDocumentViewModel\s*&\s*\w+,\s*const\s+FGV2PresentationPrepareContext\s*&\s*\w+\s*\)"
    if not re.search(hdr_sig, impl_content):
        errors.append(
            "UGV2RuntimeSubsystem::HandleDocumentRequested must take "
            "(const FGV2UiDocumentViewModel& Document, const FGV2PresentationPrepareContext& PrepareContext)"
        )

    return errors


def run_self_tests() -> bool:
    print("Running validate_session_replacement_ownership self-tests...")
    all_passed = True

    # Test 1: Header missing PrepareContext in FDocumentSink
    bad_header_sink = "using FDocumentSink = TFunction<bool(const FGV2UiDocumentViewModel&)>;"
    errs = validate_coordinator_header(bad_header_sink)
    if not any("FDocumentSink must have signature" in e for e in errs):
        print("FAIL: Expected error for FDocumentSink signature mismatch")
        all_passed = False

    # Test 2: Header with GetContentSnapshotForPrepare
    bad_header_ambient = "const FGV2SessionContentSnapshot* GetContentSnapshotForPrepare() const;"
    errs = validate_coordinator_header(bad_header_ambient)
    if not any("GetContentSnapshotForPrepare" in e for e in errs):
        print("FAIL: Expected error for GetContentSnapshotForPrepare in header")
        all_passed = False

    # Test 3: Impl with ContentSnapshot mutation outside owner routines
    mock_impl = """
    FGV2SessionReplacementToken FGV2SessionCoordinator::BeginReplace(FGV2SessionReplacementToken Token, FGV2SessionContentSnapshot& Candidate) {
        ContentSnapshot.Reset();
        ProjectionTeardownSink();
        return Token;
    }
    void FGV2SessionCoordinator::PublishReady(FGV2SessionReplacementToken Token) {
        ContentSnapshot = MoveTemp(Token.Candidate);
        ProjectionPublishSink();
    }
    void FGV2SessionCoordinator::EndSession(EGV2SessionState TargetState) {
        ContentSnapshot.Reset();
        ProjectionTeardownSink();
    }
    void FGV2SessionCoordinator::FailRuntime(const GV2RuntimeCore::FRuntimeFault& Fault) {
        ContentSnapshot.Reset();
    }
    void FGV2SessionCoordinator::SneakyBypass() {
        ContentSnapshot.Reset();
    }
    """
    errs = validate_coordinator_impl(mock_impl)
    if not any("SneakyBypass" in e or "outside approved owner routines" in e for e in errs):
        print("FAIL: Expected error for ContentSnapshot mutation in SneakyBypass")
        all_passed = False

    # Test 4: Subsystem with eager teardown in StartSession
    mock_subsystem_impl = """
    void UGV2RuntimeSubsystem::StartSession() {
        if (ActiveGameShell != nullptr) {
            ActiveGameShell->RemoveFromParent();
        }
        Coordinator->StartSession(Repo, Ver, Set);
    }
    void UGV2RuntimeSubsystem::TeardownActiveProjection() {}
    void UGV2RuntimeSubsystem::PublishActiveProjection() {}
    bool UGV2RuntimeSubsystem::HandleDocumentRequested(const FGV2UiDocumentViewModel& Document, const FGV2PresentationPrepareContext& PrepareContext) {
        UClass* Cls = PrepareContext.GetGameShellClass();
        return true;
    }
    """
    errs = validate_subsystem(mock_subsystem_impl, "")
    if not any("must NOT eagerly call RemoveFromParent" in e for e in errs):
        print("FAIL: Expected error for eager RemoveFromParent in StartSession")
        all_passed = False

    # Test 5: Subsystem loading GameShellClass from settings in StartSession
    mock_subsystem_settings = """
    void UGV2RuntimeSubsystem::StartSession() {
        const UGV2ScreenRegistrySettings* Settings = GetDefault<UGV2ScreenRegistrySettings>();
        Coordinator->StartSession(Repo, Ver, Set);
    }
    void UGV2RuntimeSubsystem::TeardownActiveProjection() {}
    void UGV2RuntimeSubsystem::PublishActiveProjection() {}
    bool UGV2RuntimeSubsystem::HandleDocumentRequested(const FGV2UiDocumentViewModel& Document, const FGV2PresentationPrepareContext& PrepareContext) {
        UClass* Cls = PrepareContext.GetGameShellClass();
        return true;
    }
    """
    errs = validate_subsystem(mock_subsystem_settings, "")
    if not any("must NOT synchronously load GameShellClass from settings" in e for e in errs):
        print("FAIL: Expected error for settings lookup in StartSession")
        all_passed = False

    if all_passed:
        print("All self-tests passed!")
    return all_passed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true", help="Run self-tests")
    args = parser.parse_args()

    if args.self_test:
        if not run_self_tests():
            return 1

    coord_header = COORDINATOR_HEADER_PATH.read_text(encoding="utf-8")
    coord_impl = COORDINATOR_IMPL_PATH.read_text(encoding="utf-8")
    subsystem_header = SUBSYSTEM_HEADER_PATH.read_text(encoding="utf-8")
    subsystem_impl = SUBSYSTEM_IMPL_PATH.read_text(encoding="utf-8")

    all_errors: list[str] = []
    all_errors.extend(validate_coordinator_header(coord_header))
    all_errors.extend(validate_coordinator_impl(coord_impl))
    all_errors.extend(validate_subsystem(subsystem_impl, subsystem_header))

    if all_errors:
        print("Validation errors found:", file=sys.stderr)
        for err in all_errors:
            print(f"  - {err}", file=sys.stderr)
        return 1

    print("Session replacement ownership validation passed cleanly.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
