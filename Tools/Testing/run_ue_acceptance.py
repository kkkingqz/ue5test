#!/usr/bin/env python3
"""Run Unreal Engine Acceptance Automation Tests from a fresh Editor process.

Executes:
    UnrealEditor-Cmd GV2.uproject \
        -unattended -nop4 -nosound -nullrhi \
        -abslog=<log_path> \
        -ReportExportPath=<report_dir> \
        -ExecCmds="Automation RunTests <filter>; Quit" \
        -TestExit="Automation Test Queue Empty"

Extracts the discovered test set from the automation engine discovery output,
normalizes the machine-readable index.json exported by Unreal Engine, and passes
both to ue_test_report.validate_run for fail-closed verification.

Usage:
    python3 Tools/Testing/run_ue_acceptance.py
    python3 Tools/Testing/run_ue_acceptance.py --filter "GV2"
    python3 Tools/Testing/run_ue_acceptance.py --self-test
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import time
import uuid
from pathlib import Path
from typing import Optional, Set, Tuple

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from Tools.Testing.ue_test_report import (
    compute_run_identity,
    normalize_ue_json_report,
    validate_run,
)


def parse_discovery_from_log(log_text: str) -> Set[str]:
    """Extracts the set of discovered test names from Unreal Engine automation log output.

    Unreal Engine logs discovery in the form:
        LogAutomationCommandLine: Display: Found <N> automation tests based on '<filter>'
        LogAutomationCommandLine: Display: \t<TestName>
    """
    discovered: Set[str] = set()
    in_discovery = False
    expected_count: Optional[int] = None

    for line in log_text.splitlines():
        if "LogAutomationCommandLine: Display: Found " in line and "automation tests based on" in line:
            in_discovery = True
            m = re.search(r"Found\s+(\d+)\s+automation tests based on", line)
            if m:
                expected_count = int(m.group(1))
            continue

        if in_discovery:
            if "LogAutomationCommandLine: Display: \t" in line:
                for chunk in line.split("LogAutomationCommandLine: Display: \t")[1:]:
                    match = re.match(r"^([A-Za-z0-9_.]+)", chunk.strip())
                    if match and "." in match.group(1):
                        discovered.add(match.group(1))
            elif (
                "Sending RunTest" in line
                or "Test Started" in line
                or "Automation: RunTests" in line
                or "Automation test started" in line
            ):
                in_discovery = False

    if expected_count is not None and len(discovered) != expected_count:
        raise ValueError(
            f"Automation discovery count mismatch: engine announced {expected_count} tests, "
            f"but log parser extracted {len(discovered)} tests."
        )

    return discovered


def run_acceptance(
    filter_expr: str = "GV2",
    ue_root: Optional[Path] = None,
    project_path: Optional[Path] = None,
    report_dir: Optional[Path] = None,
    log_path: Optional[Path] = None,
    timeout_sec: float = 1800.0,
    verbose: bool = False,
) -> int:
    repo_root = REPO_ROOT
    if ue_root is None:
        ue_root_env = os.environ.get("UE_ROOT", "/opt/unreal-engine")
        ue_root = Path(ue_root_env)

    editor_cmd = ue_root / "Engine" / "Binaries" / "Linux" / "UnrealEditor-Cmd"
    if not editor_cmd.is_file() or not os.access(editor_cmd, os.X_OK):
        print(
            f"ERROR: UnrealEditor-Cmd executable not found or not executable at: {editor_cmd}\n"
            f"Please verify that UE_ROOT is set correctly (current: {ue_root}).",
            file=sys.stderr,
        )
        return 1

    if project_path is None:
        project_path = repo_root / "GV2.uproject"
    if not project_path.is_file():
        print(f"ERROR: Project file not found: {project_path}", file=sys.stderr)
        return 1

    if report_dir is None:
        report_dir = repo_root / "Saved" / "Automation" / "Reports"
    report_dir = report_dir.resolve()
    # Clean previous report directory if present
    if report_dir.exists():
        shutil.rmtree(report_dir)
    report_dir.mkdir(parents=True, exist_ok=True)

    if log_path is None:
        log_path = repo_root / "Saved" / "Logs" / "GV2Automation.log"
    log_path = log_path.resolve()
    log_path.parent.mkdir(parents=True, exist_ok=True)
    if log_path.exists():
        log_path.unlink()

    stdout_path = log_path.with_suffix(".stdout.log")
    if stdout_path.exists():
        stdout_path.unlink()

    run_id = f"fresh-{uuid.uuid4().hex[:8]}"
    run_identity = compute_run_identity(project_root=repo_root, run_id=run_id)

    cmd = [
        str(editor_cmd),
        str(project_path),
        "-unattended",
        "-nopause",
        "-nosplash",
        "-nop4",
        "-nosound",
        "-nullrhi",
        "-FORCELOGFLUSH",
        f"-abslog={log_path}",
        f"-ReportExportPath={report_dir}",
        f"-ExecCmds=Automation RunTests {filter_expr}; Quit",
    ]

    print("=" * 70)
    print("UNREAL ENGINE ACCEPTANCE TEST RUNNER (FRESH PROCESS)")
    print("=" * 70)
    print(f"Run ID:            {run_identity['run_id']}")
    print(f"Source Revision:   {run_identity['source_revision']}")
    print(f"Source Diff:       {run_identity['source_diff_hash']}")
    print(f"Build Fingerprint: {run_identity['build_fingerprint']}")
    print(f"Filter Expression: {filter_expr}")
    print(f"Report Directory:  {report_dir}")
    print(f"Log File:          {log_path}")
    print("=" * 70)
    print(f"Executing: {' '.join(cmd)}\n")

    start_time = time.time()
    try:
        with open(stdout_path, "w", encoding="utf-8") as stdout_file:
            proc = subprocess.run(
                cmd,
                stdout=stdout_file,
                stderr=subprocess.STDOUT,
                timeout=timeout_sec,
            )
            proc_exit = proc.returncode
    except subprocess.TimeoutExpired:
        print(f"ERROR: Unreal Editor automation timed out after {timeout_sec}s.", file=sys.stderr)
        return 1
    except Exception as e:
        print(f"ERROR: Failed to launch Unreal Editor process: {e}", file=sys.stderr)
        return 1

    elapsed = time.time() - start_time
    print(f"\nUnreal Editor process completed in {elapsed:.2f}s (process exit code: {proc_exit}).")

    if proc_exit != 0:
        print(f"ERROR: Unreal Editor process exited with non-zero exit code: {proc_exit}", file=sys.stderr)
        return proc_exit if proc_exit > 0 else 1

    if not log_path.is_file():
        print(f"ERROR: Automation log file was not generated at: {log_path}", file=sys.stderr)
        return 1

    try:
        log_content = log_path.read_text(encoding="utf-8", errors="replace")
    except Exception as e:
        print(f"ERROR: Could not read automation log: {e}", file=sys.stderr)
        return 1

    # Check for engine clean completion marker
    if "**** TEST COMPLETE. EXIT CODE: 0 ****" not in log_content:
        print(
            "ERROR: Engine did not reach '**** TEST COMPLETE. EXIT CODE: 0 ****'.\n"
            "Automation run terminated abnormally or early.",
            file=sys.stderr,
        )
        return 1

    # Extract discovery inventory
    try:
        discovered = parse_discovery_from_log(log_content)
    except ValueError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 1

    if not discovered:
        print(
            "ERROR: Could not obtain automation discovery inventory from Unreal Engine output.\n"
            "Runner requires verified discovery inventory and will not infer it from completed reports.",
            file=sys.stderr,
        )
        return 1

    print(f"Discovered {len(discovered)} tests from engine automation inventory.")

    index_json_path = report_dir / "index.json"
    if not index_json_path.is_file():
        print(
            f"ERROR: Machine report index.json was not found at: {index_json_path}\n"
            f"Please verify -ReportExportPath was processed by Unreal Engine.",
            file=sys.stderr,
        )
        return 1

    # Check that report was generated in the current run (not stale)
    if index_json_path.stat().st_mtime < start_time:
        print(
            f"ERROR: Stale index.json found at {index_json_path} (mtime older than run start).",
            file=sys.stderr,
        )
        return 1

    try:
        normalized_report = normalize_ue_json_report(
            index_json_path,
            run_id=run_id,
            report_file_path=index_json_path,
        )
    except Exception as e:
        print(f"ERROR: Failed to normalize UE index.json report: {e}", file=sys.stderr)
        return 1

    diagnostics = validate_run(discovered, normalized_report, run_identity)


    tests = normalized_report.get("tests", [])
    passed = normalized_report.get("passed", 0)
    failed = normalized_report.get("failed", 0)
    skipped = normalized_report.get("skipped", 0)
    total = normalized_report.get("total", len(tests))
    duration = normalized_report.get("duration", elapsed)

    print("\n" + "=" * 70)
    print(f"AUTOMATION RESULTS SUMMARY ({duration:.2f}s)")
    print("=" * 70)
    print(f"Total: {total} | Passed: {passed} | Failed: {failed} | Skipped: {skipped}\n")

    if verbose or diagnostics:
        for t in tests:
            name = t.get("name", "Unknown")
            state = t.get("state", "Unknown")
            d = t.get("duration", 0.0)
            errors = t.get("errors", [])
            warnings = t.get("warnings", [])

            if state == "Success":
                if verbose:
                    print(f"  [PASS] {name} ({d:.3f}s)")
            else:
                print(f"  [FAIL] {name} ({d:.3f}s)")
                for err in errors:
                    print(f"         Error: {err}")

            if verbose and warnings:
                for w in warnings:
                    print(f"         Warning: {w}")

    print("-" * 70)
    if diagnostics:
        print("VALIDATION FAILED (FAIL-CLOSED):", file=sys.stderr)
        for d in diagnostics:
            print(f"  - {d}", file=sys.stderr)
        return 1

    print(f"SUCCESS: All {len(discovered)}/{len(discovered)} discovered tests passed validation.")
    return 0


def run_self_test() -> int:
    """Self-test verifying log discovery parsing and validator integration."""
    sample_log = """
LogAutomationCommandLine: Display: Ready to start automation
LogAutomationCommandLine: Display: Found 3 automation tests based on 'GV2'
LogAutomationCommandLine: Display: \tGV2.Test.A
LogAutomationCommandLine: Display: \tGV2.Test.B
LogAutomationCommandLine: Display: \tGV2.Test.C
LogAutomationCommandLine: Display: Automation test started
"""
    discovered = parse_discovery_from_log(sample_log)
    assert discovered == {"GV2.Test.A", "GV2.Test.B", "GV2.Test.C"}, f"Unexpected discovery: {discovered}"

    # Verify missing discovery returns empty set
    empty_discovered = parse_discovery_from_log("LogInit: Running engine\n")
    assert empty_discovered == set(), f"Expected empty set, got {empty_discovered}"

    print("run_ue_acceptance self-test: PASSED")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Unreal Engine acceptance test runner.")
    parser.add_argument(
        "--filter",
        "-f",
        default="GV2",
        help="Test filter expression (default: 'GV2').",
    )
    parser.add_argument(
        "--fresh-process",
        action="store_true",
        default=True,
        help="Run tests in a fresh UnrealEditor-Cmd process (default: True).",
    )
    parser.add_argument(
        "--ue-root",
        default=os.environ.get("UE_ROOT", "/opt/unreal-engine"),
        help="Path to Unreal Engine installation root.",
    )
    parser.add_argument(
        "--project",
        default=str(REPO_ROOT / "GV2.uproject"),
        help="Path to GV2.uproject file.",
    )
    parser.add_argument(
        "--report-dir",
        default=str(REPO_ROOT / "Saved" / "Automation" / "Reports"),
        help="Directory to output Unreal machine report (-ReportExportPath).",
    )
    parser.add_argument(
        "--log-path",
        default=str(REPO_ROOT / "Saved" / "Logs" / "GV2Automation.log"),
        help="Path to save Unreal automation log (-abslog).",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=1800.0,
        help="Max timeout in seconds for execution (default: 1800s).",
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Print verbose output for all tests.",
    )
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="Run self-tests and exit.",
    )
    args = parser.parse_args()

    if args.self_test:
        return run_self_test()

    return run_acceptance(
        filter_expr=args.filter,
        ue_root=Path(args.ue_root),
        project_path=Path(args.project),
        report_dir=Path(args.report_dir),
        log_path=Path(args.log_path),
        timeout_sec=args.timeout,
        verbose=args.verbose,
    )


if __name__ == "__main__":
    sys.exit(main())
