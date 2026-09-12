#!/usr/bin/env python3
"""Unit and fixture tests for ue_test_report.py.

Verifies:
- Positive success run (empty diagnostics)
- Positive success run with warnings (empty diagnostics)
- Negative: empty discovered set
- Negative: empty report tests
- Negative: missing tests from discovered set
- Negative: extra tests in report
- Negative: duplicate test names in report
- Negative: non-success states (NotRun, InProcess, Fail, Unknown, Skipped)
- Negative: errors reported even during 'Success' state
- Negative: mismatched counters (total, passed, failed, skipped, in_process)
- Negative: stale / mismatched run identity (run_id, source_revision, source_diff_hash, build_fingerprint)
- Negative: missing run identity
- Negative: truncated / malformed JSON in adapters
- Real fixtures: MCP results and UE index.json adaptation
"""

from __future__ import annotations

import json
import os
import sys
import unittest
from pathlib import Path

# Add repo root to sys.path
REPO_ROOT = Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from Tools.Testing.ue_test_report import (
    compute_run_identity,
    normalize_mcp_report,
    normalize_ue_json_report,
    validate_run,
)


class TestUeTestReport(unittest.TestCase):
    def setUp(self) -> None:
        self.identity = {
            "run_id": "run-test-12345",
            "source_revision": "abcdef1234567890abcdef1234567890abcdef12",
            "source_diff_hash": "clean",
            "build_fingerprint": "fingerprint-linux-dev-v1",
        }

    def _make_valid_report(self, test_names: list[str], with_warnings: bool = False) -> dict:
        tests = []
        for name in test_names:
            t = {
                "name": name,
                "state": "Success",
                "duration": 0.05,
                "errors": [],
                "warnings": ["LogTest: warning message"] if with_warnings else [],
            }
            tests.append(t)
        return {
            "run_identity": dict(self.identity),
            "tests": tests,
            "total": len(tests),
            "passed": len(tests),
            "failed": 0,
            "skipped": 0,
            "duration": 0.05 * len(tests),
        }

    def test_plan_algorithm_check(self) -> None:
        """Verifies the four algorithm assertions explicitly listed in Acceptance.md CFC-02."""
        valid_success_report = self._make_valid_report(["GV2.A"])
        matching_identity = dict(self.identity)
        stale_identity = {**self.identity, "source_revision": "old_commit_hash_1111"}
        not_run_report = {
            "run_identity": dict(self.identity),
            "tests": [{"name": "GV2.A", "state": "NotRun", "duration": 0, "errors": [], "warnings": []}],
            "total": 1,
            "passed": 0,
            "failed": 0,
            "skipped": 1,
            "duration": 0.0,
        }

        # 1. Matching single test -> []
        self.assertEqual(validate_run({"GV2.A"}, valid_success_report, matching_identity), [])
        # 2. Missing test B -> truthy non-empty diagnostics
        self.assertTrue(validate_run({"GV2.A", "GV2.B"}, valid_success_report, matching_identity))
        # 3. NotRun state -> truthy non-empty diagnostics
        self.assertTrue(validate_run({"GV2.A"}, not_run_report, matching_identity))
        # 4. Stale identity -> truthy non-empty diagnostics
        self.assertTrue(validate_run({"GV2.A"}, valid_success_report, stale_identity))

    def test_positive_success(self) -> None:
        discovered = {"GV2.Test1", "GV2.Test2", "GV2.Test3"}
        report = self._make_valid_report(list(discovered))
        diags = validate_run(discovered, report, self.identity)
        self.assertEqual(diags, [])

    def test_positive_success_with_warnings(self) -> None:
        discovered = {"GV2.WarnTest1", "GV2.WarnTest2"}
        report = self._make_valid_report(list(discovered), with_warnings=True)
        diags = validate_run(discovered, report, self.identity)
        self.assertEqual(diags, [])

    def test_negative_empty_discovered(self) -> None:
        report = self._make_valid_report(["GV2.Test1"])
        diags = validate_run(set(), report, self.identity)
        self.assertTrue(any("Discovered test set is empty" in d for d in diags))

    def test_negative_empty_report(self) -> None:
        report = {
            "run_identity": dict(self.identity),
            "tests": [],
            "total": 0,
            "passed": 0,
            "failed": 0,
            "skipped": 0,
        }
        diags = validate_run({"GV2.Test1"}, report, self.identity)
        self.assertTrue(any("Report contains no test records" in d for d in diags))

    def test_negative_missing_tests(self) -> None:
        discovered = {"GV2.Test1", "GV2.Test2", "GV2.Test3"}
        report = self._make_valid_report(["GV2.Test1", "GV2.Test2"])
        diags = validate_run(discovered, report, self.identity)
        self.assertTrue(any("missing from completed report" in d for d in diags))
        self.assertTrue(any("GV2.Test3" in d for d in diags))

    def test_negative_extra_tests(self) -> None:
        discovered = {"GV2.Test1"}
        report = self._make_valid_report(["GV2.Test1", "GV2.ExtraTest"])
        diags = validate_run(discovered, report, self.identity)
        self.assertTrue(any("not in discovered set" in d for d in diags))
        self.assertTrue(any("GV2.ExtraTest" in d for d in diags))

    def test_negative_duplicate_names(self) -> None:
        discovered = {"GV2.Test1"}
        report = {
            "run_identity": dict(self.identity),
            "tests": [
                {"name": "GV2.Test1", "state": "Success", "errors": [], "warnings": []},
                {"name": "GV2.Test1", "state": "Success", "errors": [], "warnings": []},
            ],
            "total": 2,
            "passed": 2,
            "failed": 0,
            "skipped": 0,
        }
        diags = validate_run(discovered, report, self.identity)
        self.assertTrue(any("Duplicate test record" in d for d in diags))

    def test_negative_non_success_states(self) -> None:
        for bad_state in ["Fail", "NotRun", "InProcess", "Unknown", "Skipped"]:
            report = {
                "run_identity": dict(self.identity),
                "tests": [{"name": "GV2.Bad", "state": bad_state, "errors": [], "warnings": []}],
                "total": 1,
                "passed": 0,
                "failed": 1 if bad_state == "Fail" else 0,
                "skipped": 1 if bad_state in ("NotRun", "Skipped") else 0,
            }
            diags = validate_run({"GV2.Bad"}, report, self.identity)
            self.assertTrue(
                any(f"non-success state '{bad_state}'" in d for d in diags),
                f"State '{bad_state}' should be rejected as non-success",
            )

    def test_negative_errors_during_success(self) -> None:
        report = {
            "run_identity": dict(self.identity),
            "tests": [
                {
                    "name": "GV2.ErrorTest",
                    "state": "Success",
                    "errors": ["Fatal assertion failed in background"],
                    "warnings": [],
                }
            ],
            "total": 1,
            "passed": 1,
            "failed": 0,
            "skipped": 0,
        }
        diags = validate_run({"GV2.ErrorTest"}, report, self.identity)
        self.assertTrue(any("reported errors" in d for d in diags))

    def test_negative_mismatched_totals(self) -> None:
        # total counter mismatch
        report = self._make_valid_report(["GV2.Test1"])
        report["total"] = 5
        diags = validate_run({"GV2.Test1"}, report, self.identity)
        self.assertTrue(any("Report counter total" in d for d in diags))

        # passed counter mismatch
        report = self._make_valid_report(["GV2.Test1"])
        report["passed"] = 0
        diags = validate_run({"GV2.Test1"}, report, self.identity)
        self.assertTrue(any("Report counter passed" in d for d in diags))

        # failed counter non-zero
        report = self._make_valid_report(["GV2.Test1"])
        report["failed"] = 1
        diags = validate_run({"GV2.Test1"}, report, self.identity)
        self.assertTrue(any("Report counter failed is non-zero" in d for d in diags))

        # skipped counter non-zero
        report = self._make_valid_report(["GV2.Test1"])
        report["skipped"] = 1
        diags = validate_run({"GV2.Test1"}, report, self.identity)
        self.assertTrue(any("Report counter skipped is non-zero" in d for d in diags))

    def test_negative_missing_counters(self) -> None:
        for counter in ["total", "passed", "failed", "skipped"]:
            report = self._make_valid_report(["GV2.Test1"])
            del report[counter]
            diags = validate_run({"GV2.Test1"}, report, self.identity)
            self.assertTrue(
                any(f"missing required counter '{counter}'" in d for d in diags),
                f"Missing counter '{counter}' must produce diagnostic",
            )

    def test_negative_stale_identity(self) -> None:
        for key in ["run_id", "source_revision", "source_diff_hash", "build_fingerprint"]:
            stale = dict(self.identity)
            stale[key] = f"stale-{key}"
            report = self._make_valid_report(["GV2.Test1"])
            diags = validate_run({"GV2.Test1"}, report, stale)
            self.assertTrue(
                any(f"Run identity mismatch for '{key}'" in d for d in diags),
                f"Mismatch on {key} should produce diagnostic",
            )

    def test_negative_missing_identity(self) -> None:
        report = self._make_valid_report(["GV2.Test1"])
        del report["run_identity"]
        diags = validate_run({"GV2.Test1"}, report, self.identity)
        self.assertTrue(any("missing 'run_identity'" in d for d in diags))

    def test_negative_truncated_or_malformed_json_adapters(self) -> None:
        # MCP report adapter with invalid types
        with self.assertRaises(ValueError):
            normalize_mcp_report("not a dict", self.identity)

        with self.assertRaises(ValueError):
            normalize_mcp_report({"tests": "not a list"}, self.identity)

        with self.assertRaises(ValueError):
            normalize_mcp_report({"tests": [{"missing_name": 123}]}, self.identity)

        # UE json report adapter with invalid types
        with self.assertRaises(ValueError):
            normalize_ue_json_report(12345, self.identity)

        with self.assertRaises(ValueError):
            normalize_ue_json_report({"tests": "not a list"}, self.identity)

        with self.assertRaises(ValueError):
            normalize_ue_json_report({"tests": [{"state": "Success"}]}, self.identity)

    def test_ue_index_json_adapter(self) -> None:
        sample_ue_data = {
            "succeeded": 1,
            "succeededWithWarnings": 1,
            "failed": 0,
            "notRun": 0,
            "inProcess": 0,
            "totalDuration": 1.5,
            "tests": [
                {
                    "fullTestPath": "GV2.Sample.Test1",
                    "state": "Success",
                    "duration": 0.5,
                    "warnings": 0,
                    "errors": 0,
                    "entries": [],
                },
                {
                    "fullTestPath": "GV2.Sample.Test2",
                    "state": "Success",
                    "duration": 1.0,
                    "warnings": 1,
                    "errors": 0,
                    "entries": [
                        {"event": {"type": "Warning", "message": "Benign warning"}}
                    ],
                },
            ],
        }
        normalized = normalize_ue_json_report(sample_ue_data, self.identity)
        self.assertEqual(normalized["passed"], 2)
        self.assertEqual(normalized["total"], 2)
        self.assertEqual(normalized["failed"], 0)
        self.assertEqual(normalized["skipped"], 0)
        discovered = {"GV2.Sample.Test1", "GV2.Sample.Test2"}
        diags = validate_run(discovered, normalized, self.identity)
        self.assertEqual(diags, [])

    def test_mcp_report_adapter(self) -> None:
        sample_mcp_data = {
            "total": 2,
            "passed": 2,
            "failed": 0,
            "skipped": 0,
            "duration": 0.8,
            "tests": [
                {
                    "name": "GV2.Mcp.Test1",
                    "state": "Success",
                    "duration": 0.3,
                    "errors": [],
                    "warnings": [],
                },
                {
                    "name": "GV2.Mcp.Test2",
                    "state": "Success",
                    "duration": 0.5,
                    "errors": [],
                    "warnings": ["Warning"],
                },
            ],
        }
        normalized = normalize_mcp_report(sample_mcp_data, self.identity)
        self.assertEqual(normalized["passed"], 2)
        self.assertEqual(normalized["total"], 2)
        discovered = {"GV2.Mcp.Test1", "GV2.Mcp.Test2"}
        diags = validate_run(discovered, normalized, self.identity)
        self.assertEqual(diags, [])


class TestRunnersIntegration(unittest.TestCase):
    """Tests that local and CI runners fail closed and return correct exit codes."""

    def test_run_ue_tests_verif_af_02_fail_closed(self) -> None:
        """VERIFY-AF-02: Runner must exit non-zero when total=1, skipped=1, NotRun."""
        from Tools.MCP import run_ue_tests

        class FakeClient:
            def __init__(self, **kw): pass
            def discover_tests(self, **kw): return {}
            def list_tests(self, **kw): return ["GV2.Audit.Synthetic"]
            def run_tests_by_filter(self, *a):
                return {
                    "total": 1,
                    "passed": 0,
                    "failed": 0,
                    "skipped": 1,
                    "tests": [{
                        "name": "GV2.Audit.Synthetic",
                        "state": "NotRun",
                        "duration": 0,
                        "errors": [],
                        "warnings": []
                    }],
                }

        old_client = run_ue_tests.UnrealMcpClient
        old_sleep = run_ue_tests.time.sleep
        old_argv = sys.argv
        try:
            run_ue_tests.UnrealMcpClient = FakeClient
            run_ue_tests.time.sleep = lambda *_: None
            sys.argv = ["run_ue_tests.py", "--filter", "StartsWith:GV2"]
            exit_code = run_ue_tests.main()
            self.assertEqual(exit_code, 1, "VERIFY-AF-02: NotRun test must cause runner to exit with 1")
        finally:
            run_ue_tests.UnrealMcpClient = old_client
            run_ue_tests.time.sleep = old_sleep
            sys.argv = old_argv

    def test_run_ue_tests_success_exit_zero(self) -> None:
        """Runner must exit 0 when all discovered tests succeed with zero errors."""
        from Tools.MCP import run_ue_tests

        class FakeSuccessClient:
            def __init__(self, **kw): pass
            def discover_tests(self, **kw): return {}
            def list_tests(self, **kw): return ["GV2.Test.A"]
            def run_tests_by_filter(self, *a):
                return {
                    "total": 1,
                    "passed": 1,
                    "failed": 0,
                    "skipped": 0,
                    "tests": [{
                        "name": "GV2.Test.A",
                        "state": "Success",
                        "duration": 0.05,
                        "errors": [],
                        "warnings": []
                    }],
                }

        old_client = run_ue_tests.UnrealMcpClient
        old_sleep = run_ue_tests.time.sleep
        old_argv = sys.argv
        try:
            run_ue_tests.UnrealMcpClient = FakeSuccessClient
            run_ue_tests.time.sleep = lambda *_: None
            sys.argv = ["run_ue_tests.py", "--filter", "StartsWith:GV2"]
            exit_code = run_ue_tests.main()
            self.assertEqual(exit_code, 0, "Successful test run must exit with 0")
        finally:
            run_ue_tests.UnrealMcpClient = old_client
            run_ue_tests.time.sleep = old_sleep
            sys.argv = old_argv

    def test_run_ue_tests_empty_discovery_exit_one(self) -> None:
        """Runner must exit 1 when discovery returns empty list."""
        from Tools.MCP import run_ue_tests

        class FakeEmptyClient:
            def __init__(self, **kw): pass
            def discover_tests(self, **kw): return {}
            def list_tests(self, **kw): return []
            def run_tests_by_filter(self, *a): return {"tests": []}

        old_client = run_ue_tests.UnrealMcpClient
        old_sleep = run_ue_tests.time.sleep
        old_argv = sys.argv
        try:
            run_ue_tests.UnrealMcpClient = FakeEmptyClient
            run_ue_tests.time.sleep = lambda *_: None
            sys.argv = ["run_ue_tests.py", "--filter", "StartsWith:NonExistent"]
            exit_code = run_ue_tests.main()
            self.assertEqual(exit_code, 1, "Empty discovery must cause runner to exit with 1")
        finally:
            run_ue_tests.UnrealMcpClient = old_client
            run_ue_tests.time.sleep = old_sleep
            sys.argv = old_argv

    def test_run_ue_acceptance_discovery_parser(self) -> None:
        """Verifies log discovery parsing in run_ue_acceptance."""
        from Tools.Testing.run_ue_acceptance import parse_discovery_from_log

        sample_log = """
[2026.09.11-20.06.08:782][597]LogAutomationCommandLine: Display: Found 2 automation tests based on 'GV2'
[2026.09.11-20.06.08:782][597]LogAutomationCommandLine: Display: \tGV2.FirstTest
[2026.09.11-20.06.08:782][597]LogAutomationCommandLine: Display: \tGV2.SecondTest
[2026.09.11-20.06.08:783][597]LogAutomationCommandLine: Display: Starting automation...
"""
        discovered = parse_discovery_from_log(sample_log)
        self.assertEqual(discovered, {"GV2.FirstTest", "GV2.SecondTest"})

        empty_log = "[2026.09.11-20.06.08:782][597]LogInit: Engine started\n"
        self.assertEqual(parse_discovery_from_log(empty_log), set())

    def test_run_ue_acceptance_discovery_parser_count_mismatch_raises(self) -> None:
        """Verifies parse_discovery_from_log raises ValueError when announced count != parsed count."""
        from Tools.Testing.run_ue_acceptance import parse_discovery_from_log

        mismatched_log = """
[2026.09.11-20.06.08:782][597]LogAutomationCommandLine: Display: Found 3 automation tests based on 'GV2'
[2026.09.11-20.06.08:782][597]LogAutomationCommandLine: Display: \tGV2.FirstTest
[2026.09.11-20.06.08:782][597]LogAutomationCommandLine: Display: \tGV2.SecondTest
[2026.09.11-20.06.08:783][597]LogAutomationCommandLine: Display: Starting automation...
"""
        with self.assertRaises(ValueError) as ctx:
            parse_discovery_from_log(mismatched_log)
        self.assertIn("announced 3 tests, but log parser extracted 2", str(ctx.exception))

    def test_run_ue_tests_async_task_wait(self) -> None:
        """Runner must poll until terminal result when RunTestsByFilter returns an in-process state."""
        from Tools.MCP import run_ue_tests

        class FakeAsyncClient:
            def __init__(self, **kw):
                self.calls = 0
            def discover_tests(self, **kw): return {}
            def list_tests(self, **kw): return ["GV2.AsyncTest"]
            def run_tests_by_filter(self, *a):
                return {"status": "InProcess", "taskId": "task-async-1"}
            def call_tool(self, toolset, tool, args):
                if tool == "GetTestStatus":
                    self.calls += 1
                    # Return not running on second poll
                    return {"bIsRunning": False if self.calls >= 2 else True}
                elif tool == "GetTestResults":
                    return {
                        "returnValue": json.dumps({
                            "total": 1,
                            "passed": 1,
                            "failed": 0,
                            "skipped": 0,
                            "tests": [{
                                "name": "GV2.AsyncTest",
                                "state": "Success",
                                "duration": 0.1,
                                "errors": [],
                                "warnings": []
                            }]
                        })
                    }
                return {}

        old_client = run_ue_tests.UnrealMcpClient
        old_sleep = run_ue_tests.time.sleep
        old_argv = sys.argv
        try:
            run_ue_tests.UnrealMcpClient = FakeAsyncClient
            run_ue_tests.time.sleep = lambda *_: None
            sys.argv = ["run_ue_tests.py", "--filter", "StartsWith:GV2"]
            exit_code = run_ue_tests.main()
            self.assertEqual(exit_code, 0, "Async task must be polled until completion and exit 0")
        finally:
            run_ue_tests.UnrealMcpClient = old_client
            run_ue_tests.time.sleep = old_sleep
            sys.argv = old_argv


def main() -> int:
    suite = unittest.TestSuite()
    suite.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(TestUeTestReport))
    suite.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(TestRunnersIntegration))
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
