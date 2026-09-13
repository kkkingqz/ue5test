#!/usr/bin/env python3
"""Run Unreal Engine Automation Tests via MCP in the running editor.

Usage:
    python3 Tools/MCP/run_ue_tests.py
    python3 Tools/MCP/run_ue_tests.py --filter "StartsWith:GV2"
    python3 Tools/MCP/run_ue_tests.py --list
"""

from __future__ import annotations

import argparse
import os
import sys
import time
from pathlib import Path

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(os.path.dirname(SCRIPT_DIR))
for p in [SCRIPT_DIR, REPO_ROOT]:
    if p not in sys.path:
        sys.path.insert(0, p)

from mcp_client import UnrealMcpClient
from Tools.Testing.ue_test_report import (
    compute_run_identity,
    normalize_mcp_report,
    validate_run,
)


def main() -> int:
    parser = argparse.ArgumentParser(description="Run Unreal Engine automation tests via MCP.")
    parser.add_argument(
        "--filter",
        "-f",
        default="StartsWith:GV2",
        help="Filter expression (default: 'StartsWith:GV2')."
    )
    parser.add_argument(
        "--list",
        "-l",
        action="store_true",
        help="List available tests matching filter without running them."
    )
    parser.add_argument(
        "--rediscover",
        action="store_true",
        help="Force rediscover automation tests."
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Print detailed output including per-test durations and warnings."
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=300.0,
        help="Max timeout in seconds for test execution (default: 300s)."
    )
    parser.add_argument(
        "--require-fresh-binaries",
        action="store_true",
        help="Compatibility flag; runtime identity is always validated against the current checkout."
    )
    parser.add_argument(
        "--url",
        default=os.environ.get("UNREAL_MCP_URL", "http://127.0.0.1:8000/mcp"),
        help="MCP server endpoint URL."
    )
    args = parser.parse_args()

    try:
        client = UnrealMcpClient(url=args.url)
    except Exception as e:
        print(f"ERROR: Could not connect to Unreal Editor MCP server: {e}", file=sys.stderr)
        print("Please ensure Unreal Editor is running with ModelContextProtocol enabled on port 8000.", file=sys.stderr)
        return 1

    print("Discovering automation tests in Unreal Editor...")
    client.discover_tests(force_rediscover=args.rediscover)
    time.sleep(1.0)

    # Derive name prefix for full discovery listing
    filter_expr = args.filter
    if filter_expr.startswith("StartsWith:"):
        prefix = filter_expr.split("StartsWith:", 1)[1]
    elif filter_expr.startswith("Contains:"):
        prefix = filter_expr.split("Contains:", 1)[1]
    else:
        prefix = filter_expr

    discovered_list = client.list_tests(name_filter=prefix, limit=0)
    discovered = set(discovered_list)

    if args.list:
        print(f"\nFound {len(discovered_list)} tests matching '{prefix}':")
        for t in discovered_list:
            print(f"  - {t}")
        return 0

    if not discovered:
        print(f"ERROR: No automation tests discovered matching '{prefix}'.", file=sys.stderr)
        return 1

    print(f"Discovered {len(discovered)} tests to run.")
    print(f"Running automation tests with filter: '{args.filter}'...")
    start_time = time.time()
    results = client.run_tests_by_filter(args.filter)

    # If the API returns an async task handle or indicates tests are in progress, wait for terminal result
    is_async_execution = (
        isinstance(results, dict)
        and (
            results.get("status") in ("InProcess", "Running")
            or results.get("state") in ("InProcess", "Running")
            or ("taskId" in results and "tests" not in results)
            or ("task_id" in results and "tests" not in results)
        )
    )

    task_id: Optional[str] = None
    if is_async_execution:
        raw_tid = results.get("taskId") or results.get("task_id")
        if not raw_tid or not isinstance(raw_tid, str) or not raw_tid.strip():
            print(
                "ERROR: Automation test execution indicated in-process state but returned no valid task_id.\n"
                "Unbound/generic asynchronous execution is forbidden.",
                file=sys.stderr,
            )
            return 1
        task_id = raw_tid.strip()

        start_poll = time.time()
        while True:
            if time.time() - start_poll > args.timeout:
                print(f"ERROR: Automation test execution timed out after {args.timeout}s for task '{task_id}'.", file=sys.stderr)
                return 1
            time.sleep(1.0)
            try:
                status = client.get_test_status(task_id)

                is_running = (
                    isinstance(status, dict)
                    and (status.get("state") in ("InProcess", "Running") or status.get("bIsRunning") is True)
                )
                if not is_running:
                    candidate_results = client.get_test_results(task_id)

                    if not isinstance(candidate_results, dict):
                        print(
                            f"ERROR: GetTestResults returned non-dict payload for task '{task_id}': {type(candidate_results).__name__}",
                            file=sys.stderr,
                        )
                        return 1

                    if "tests" in candidate_results:
                        results = candidate_results
                        break
            except Exception as err:
                print(f"ERROR: Exception while polling async test status for task '{task_id}': {err}", file=sys.stderr)
                return 1

    elapsed = time.time() - start_time

    if not isinstance(results, dict):
        print(f"ERROR: Expected dict test results, got {type(results).__name__}", file=sys.stderr)
        return 1

    if task_id:
        run_id = f"mcp-task:{task_id}"
    else:
        response_id = getattr(client, "last_response_id", None)
        if response_id is None:
            print("ERROR: MCP client did not expose a verified JSON-RPC response id.", file=sys.stderr)
            return 1
        run_id = f"mcp-rpc:{response_id}"
    expected_identity = compute_run_identity(project_root=Path(REPO_ROOT), run_id=run_id)

    try:
        normalized_report = normalize_mcp_report(
            results,
            run_id=run_id,
            repo_root=Path(REPO_ROOT),
            expected_task_id=task_id,
        )
    except Exception as e:
        print(f"ERROR: Failed to normalize MCP test report: {e}", file=sys.stderr)
        return 1

    diagnostics = validate_run(discovered, normalized_report, expected_identity)


    tests = normalized_report.get("tests", [])
    passed = normalized_report.get("passed", 0)
    failed = normalized_report.get("failed", 0)
    skipped = normalized_report.get("skipped", 0)
    total = normalized_report.get("total", len(tests))
    duration = normalized_report.get("duration", elapsed)

    print("\n" + "=" * 70)
    print(f"UNREAL AUTOMATION TEST RESULTS ({duration:.2f}s)")
    print("=" * 70)
    print(f"Total: {total} | Passed: {passed} | Failed: {failed} | Skipped: {skipped}\n")

    for t in tests:
        name = t.get("name", "Unknown")
        state = t.get("state", "Unknown")
        d = t.get("duration", 0.0)
        errors = t.get("errors", [])
        warnings = t.get("warnings", [])

        if state == "Success":
            if args.verbose:
                print(f"  [PASS] {name} ({d:.3f}s)")
        else:
            print(f"  [FAIL] {name} ({d:.3f}s)")
            for err in errors:
                print(f"         Error: {err}")

        if args.verbose and warnings:
            for w in warnings:
                print(f"         Warning: {w}")

    print("-" * 70)
    if diagnostics:
        print("VALIDATION FAILED:", file=sys.stderr)
        for d in diagnostics:
            print(f"  - {d}", file=sys.stderr)
        return 1

    print(f"SUCCESS: All {len(discovered)}/{len(discovered)} discovered tests passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
