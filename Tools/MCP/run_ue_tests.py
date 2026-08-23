#!/usr/bin/env python3
"""Run Unreal Engine Automation Tests via MCP in the running editor.

Usage:
    python3 Tools/MCP/run_ue_tests.py
    python3 Tools/MCP/run_ue_tests.py --filter "StartsWith:GV2.Runtime.Presentation"
    python3 Tools/MCP/run_ue_tests.py --list
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from mcp_client import UnrealMcpClient


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

    if args.list:
        tests = client.list_tests(name_filter="GV2")
        print(f"\nFound {len(tests)} GV2 tests:")
        for t in tests:
            print(f"  - {t}")
        return 0

    print(f"Running automation tests with filter: '{args.filter}'...")
    start_time = time.time()
    results = client.run_tests_by_filter(args.filter)
    elapsed = time.time() - start_time

    if not isinstance(results, dict) or "tests" not in results:
        # Fallback to GetTestResults if RunTestsByFilter returned a wrapper
        res_fallback = client.get_test_results()
        if isinstance(res_fallback, dict) and "tests" in res_fallback:
            results = res_fallback

    tests = results.get("tests", []) if isinstance(results, dict) else []
    passed = results.get("passed", 0) if isinstance(results, dict) else 0
    failed = results.get("failed", 0) if isinstance(results, dict) else 0
    skipped = results.get("skipped", 0) if isinstance(results, dict) else 0
    total = results.get("total", len(tests)) if isinstance(results, dict) else len(tests)
    duration = results.get("duration", elapsed) if isinstance(results, dict) else elapsed

    print("\n" + "=" * 70)
    print(f"UNREAL AUTOMATION TEST RESULTS ({duration:.2f}s)")
    print("=" * 70)
    print(f"Total: {total} | Passed: {passed} | Failed: {failed} | Skipped: {skipped}\n")

    failed_tests = []
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
            failed_tests.append(t)

        if args.verbose and warnings:
            for w in warnings:
                print(f"         Warning: {w}")

    print("-" * 70)
    if failed == 0 and total > 0:
        print(f"SUCCESS: All {passed}/{total} tests passed.")
        return 0
    elif total == 0:
        print("WARNING: No tests were executed.")
        return 1
    else:
        print(f"FAILURE: {failed}/{total} tests failed.")
        return 1


if __name__ == "__main__":
    sys.exit(main())
