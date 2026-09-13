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
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

# Add repo root to sys.path
REPO_ROOT = Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from Tools.Testing.ue_test_report import (
    compute_build_fingerprint,
    compute_run_identity,
    compute_source_diff_hash,
    extract_runtime_identity_from_mcp_data,
    extract_runtime_identity_from_ue_data,
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
            "schema_version": 1,
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
            "schema_version": 1,
            "run_identity": dict(self.identity),
            "tests": [{"name": "GV2.A", "state": "NotRun", "duration": 0.0, "errors": [], "warnings": []}],
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
            "schema_version": 1,
            "run_identity": dict(self.identity),
            "tests": [],
            "total": 0,
            "passed": 0,
            "failed": 0,
            "skipped": 0,
            "duration": 0.0,
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
            "schema_version": 1,
            "run_identity": dict(self.identity),
            "tests": [
                {"name": "GV2.Test1", "state": "Success", "duration": 0.05, "errors": [], "warnings": []},
                {"name": "GV2.Test1", "state": "Success", "duration": 0.05, "errors": [], "warnings": []},
            ],
            "total": 2,
            "passed": 2,
            "failed": 0,
            "skipped": 0,
            "duration": 0.1,
        }
        diags = validate_run(discovered, report, self.identity)
        self.assertTrue(any("Duplicate test record" in d for d in diags))

    def test_negative_non_success_states(self) -> None:
        for bad_state in ["Fail", "NotRun", "InProcess", "Unknown", "Skipped"]:
            report = {
                "schema_version": 1,
                "run_identity": dict(self.identity),
                "tests": [{"name": "GV2.Bad", "state": bad_state, "duration": 0.05, "errors": [], "warnings": []}],
                "total": 1,
                "passed": 0,
                "failed": 1 if bad_state == "Fail" else 0,
                "skipped": 1 if bad_state in ("NotRun", "Skipped") else 0,
                "duration": 0.05,
            }
            diags = validate_run({"GV2.Bad"}, report, self.identity)
            self.assertTrue(
                any(f"non-success state '{bad_state}'" in d for d in diags),
                f"State '{bad_state}' should be rejected as non-success",
            )

    def test_negative_errors_during_success(self) -> None:
        report = {
            "schema_version": 1,
            "run_identity": dict(self.identity),
            "tests": [
                {
                    "name": "GV2.ErrorTest",
                    "state": "Success",
                    "duration": 0.05,
                    "errors": ["Fatal assertion failed in background"],
                    "warnings": [],
                }
            ],
            "total": 1,
            "passed": 1,
            "failed": 0,
            "skipped": 0,
            "duration": 0.05,
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

    def test_negative_rejected_identity_prefixes(self) -> None:
        """Verify validate_run rejects missing_*, unknown_*, no_*, error:* prefixes in expected or report identity."""
        bad_prefixes = ["missing_binaries", "unknown_revision", "no_git", "error:untracked_failure"]
        for bad_val in bad_prefixes:
            # 1. In expected identity
            bad_expected = dict(self.identity)
            bad_expected["build_fingerprint"] = bad_val
            report = self._make_valid_report(["GV2.Test1"])
            diags = validate_run({"GV2.Test1"}, report, bad_expected)
            self.assertTrue(
                any("rejected incomplete/error value" in d and bad_val in d for d in diags),
                f"Expected identity with '{bad_val}' must be rejected",
            )

            # 2. In report identity
            bad_report = self._make_valid_report(["GV2.Test1"])
            bad_report["run_identity"]["source_revision"] = bad_val
            diags = validate_run({"GV2.Test1"}, bad_report, self.identity)
            self.assertTrue(
                any("rejected incomplete/error value" in d and bad_val in d for d in diags),
                f"Report identity with '{bad_val}' must be rejected",
            )

    def test_compute_source_diff_hash_untracked_files(self) -> None:
        """Verify compute_source_diff_hash accounts for untracked files."""
        with tempfile.TemporaryDirectory() as tmp_dir:
            tmp_path = Path(tmp_dir)
            # Initialize git repo in tmp_path
            subprocess.run(["git", "init"], cwd=str(tmp_path), capture_output=True, check=True)
            subprocess.run(["git", "config", "user.name", "Test"], cwd=str(tmp_path), capture_output=True, check=True)
            subprocess.run(["git", "config", "user.email", "test@example.com"], cwd=str(tmp_path), capture_output=True, check=True)

            # Initial commit
            dummy_file = tmp_path / "committed.txt"
            dummy_file.write_text("hello", encoding="utf-8")
            subprocess.run(["git", "add", "committed.txt"], cwd=str(tmp_path), capture_output=True, check=True)
            subprocess.run(["git", "commit", "-m", "init"], cwd=str(tmp_path), capture_output=True, check=True)

            # Clean repo -> "clean"
            clean_hash = compute_source_diff_hash(tmp_path)
            self.assertEqual(clean_hash, "clean")

            # Add untracked file -> non-clean sha256 hash
            untracked = tmp_path / "untracked.txt"
            untracked.write_text("some content", encoding="utf-8")
            hash1 = compute_source_diff_hash(tmp_path)
            self.assertNotEqual(hash1, "clean")
            self.assertEqual(len(hash1), 64)

            # Changing untracked file content changes the hash
            untracked.write_text("changed content", encoding="utf-8")
            hash2 = compute_source_diff_hash(tmp_path)
            self.assertNotEqual(hash1, hash2)

    def test_compute_build_fingerprint_deterministic_and_missing(self) -> None:
        """Verify compute_build_fingerprint behavior with missing vs present binaries."""
        with tempfile.TemporaryDirectory() as tmp_dir:
            tmp_path = Path(tmp_dir)
            # 1. No Binaries/Linux dir
            self.assertEqual(compute_build_fingerprint(tmp_path), "missing_binaries")

            # 2. Empty Binaries/Linux dir
            bin_dir = tmp_path / "Binaries" / "Linux"
            bin_dir.mkdir(parents=True)
            self.assertEqual(compute_build_fingerprint(tmp_path), "missing_binaries")

            # 3. Binaries present
            so_a = bin_dir / "libUnrealEditor-GV2.so"
            so_b = bin_dir / "libUnrealEditor-GV2ContentCore.so"
            so_a.write_bytes(b"binary content A")
            so_b.write_bytes(b"binary content B")

            fp1 = compute_build_fingerprint(tmp_path)
            self.assertEqual(len(fp1), 64)

            # Idempotent
            self.assertEqual(compute_build_fingerprint(tmp_path), fp1)

            # Modifying content changes fingerprint
            so_a.write_bytes(b"modified binary content A")
            fp2 = compute_build_fingerprint(tmp_path)
            self.assertNotEqual(fp1, fp2)

    def test_negative_mcp_missing_failed_or_skipped(self) -> None:
        """normalize_mcp_report must reject missing failed or skipped counters."""
        valid_mcp = {
            "schema_version": 1,
            "total": 1,
            "passed": 1,
            "failed": 0,
            "skipped": 0,
            "duration": 0.05,
            "tests": [{"name": "GV2.Test", "state": "Success", "duration": 0.05, "errors": [], "warnings": []}],
        }

        # Missing failed
        bad_1 = dict(valid_mcp)
        del bad_1["failed"]
        with self.assertRaises(ValueError):
            normalize_mcp_report(bad_1, self.identity)

        # Missing skipped
        bad_2 = dict(valid_mcp)
        del bad_2["skipped"]
        with self.assertRaises(ValueError):
            normalize_mcp_report(bad_2, self.identity)

    def test_extract_runtime_identity_from_module_identity(self) -> None:
        """Verify runtime identity extraction from GV2.Runtime.ModuleIdentity test entries."""
        payload = {
            "source_revision": "rev-test-456",
            "source_diff_hash": "clean",
            "build_fingerprint": "fp-test-789",
        }
        msg = f"LogAutomationTest: Display: GV2_RUNTIME_IDENTITY:{json.dumps(payload)}"

        # 1. In UE index.json format
        ue_data = {
            "succeeded": 1,
            "succeededWithWarnings": 0,
            "failed": 0,
            "notRun": 0,
            "inProcess": 0,
            "totalDuration": 0.05,
            "tests": [
                {
                    "fullTestPath": "GV2.Runtime.ModuleIdentity",
                    "state": "Success",
                    "duration": 0.05,
                    "entries": [{"event": {"type": "Display", "message": msg}}],
                }
            ],
        }
        extracted_ue = extract_runtime_identity_from_ue_data(ue_data)
        self.assertEqual(extracted_ue, payload)

        normalized_ue = normalize_ue_json_report(ue_data, run_id="my-run-id")
        self.assertEqual(normalized_ue["run_identity"]["source_revision"], "rev-test-456")
        self.assertEqual(normalized_ue["run_identity"]["run_id"], "my-run-id")

        # 2. In MCP data format
        mcp_data = {
            "schema_version": 1,
            "total": 1,
            "passed": 1,
            "failed": 0,
            "skipped": 0,
            "duration": 0.05,
            "tests": [
                {
                    "name": "GV2.Runtime.ModuleIdentity",
                    "state": "Success",
                    "duration": 0.05,
                    "errors": [],
                    "warnings": [],
                    "entries": [{"message": msg}],
                }
            ],
        }
        extracted_mcp = extract_runtime_identity_from_mcp_data(mcp_data)
        self.assertEqual(extracted_mcp, payload)

        normalized_mcp = normalize_mcp_report(mcp_data, run_id="my-mcp-run")
        self.assertEqual(normalized_mcp["run_identity"]["source_revision"], "rev-test-456")
        self.assertEqual(normalized_mcp["run_identity"]["run_id"], "my-mcp-run")

    def test_negative_truncated_or_malformed_json_adapters(self) -> None:
        # MCP report adapter with invalid types
        with self.assertRaises(ValueError):
            normalize_mcp_report("not a dict", self.identity)

        with self.assertRaises(ValueError):
            normalize_mcp_report({"tests": "not a list"}, self.identity)

        with self.assertRaises(ValueError):
            normalize_mcp_report({"schema_version": 1, "tests": [{"missing_name": 123}]}, self.identity)

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
            "schema_version": 1,
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

    def test_negative_mcp_missing_schema_version_raises(self) -> None:
        """normalize_mcp_report must reject missing schema_version."""
        raw = {
            "total": 1,
            "passed": 1,
            "failed": 0,
            "skipped": 0,
            "duration": 0.1,
            "tests": [{"name": "GV2.Test", "state": "Success", "duration": 0.1, "errors": [], "warnings": []}],
        }
        with self.assertRaises(ValueError) as ctx:
            normalize_mcp_report(raw, self.identity)
        self.assertIn("missing required 'schema_version'", str(ctx.exception))

    def test_negative_mcp_unsupported_schema_version_raises(self) -> None:
        """normalize_mcp_report must reject unsupported schema_version."""
        raw = {
            "schema_version": "999.0",
            "total": 1,
            "passed": 1,
            "failed": 0,
            "skipped": 0,
            "duration": 0.1,
            "tests": [{"name": "GV2.Test", "state": "Success", "duration": 0.1, "errors": [], "warnings": []}],
        }
        with self.assertRaises(ValueError) as ctx:
            normalize_mcp_report(raw, self.identity)
        self.assertIn("unsupported schema_version", str(ctx.exception))

    def test_negative_mcp_missing_duration_raises(self) -> None:
        """normalize_mcp_report must reject missing duration."""
        raw = {
            "schema_version": 1,
            "total": 1,
            "passed": 1,
            "failed": 0,
            "skipped": 0,
            "tests": [{"name": "GV2.Test", "state": "Success", "duration": 0.1, "errors": [], "warnings": []}],
        }
        with self.assertRaises(ValueError) as ctx:
            normalize_mcp_report(raw, self.identity)
        self.assertIn("missing required field 'duration'", str(ctx.exception))

    def test_negative_mcp_test_missing_state_raises(self) -> None:
        """normalize_mcp_report must reject test record without state (no defaulting to Unknown)."""
        raw = {
            "schema_version": 1,
            "total": 1,
            "passed": 1,
            "failed": 0,
            "skipped": 0,
            "duration": 0.1,
            "tests": [{"name": "GV2.Test", "duration": 0.1, "errors": [], "warnings": []}],
        }
        with self.assertRaises(ValueError) as ctx:
            normalize_mcp_report(raw, self.identity)
        self.assertIn("missing required 'state'", str(ctx.exception))

    def test_negative_mcp_test_missing_duration_raises(self) -> None:
        """normalize_mcp_report must reject test record without duration (no defaulting to 0.0)."""
        raw = {
            "schema_version": 1,
            "total": 1,
            "passed": 1,
            "failed": 0,
            "skipped": 0,
            "duration": 0.1,
            "tests": [{"name": "GV2.Test", "state": "Success", "errors": [], "warnings": []}],
        }
        with self.assertRaises(ValueError) as ctx:
            normalize_mcp_report(raw, self.identity)
        self.assertIn("missing required 'duration'", str(ctx.exception))

    def test_negative_mcp_test_non_list_errors_or_warnings_raises(self) -> None:
        """normalize_mcp_report must reject non-list errors/warnings without coercion."""
        raw_bad_errors = {
            "schema_version": 1,
            "total": 1,
            "passed": 1,
            "failed": 0,
            "skipped": 0,
            "duration": 0.1,
            "tests": [{"name": "GV2.Test", "state": "Success", "duration": 0.1, "errors": "Single error string", "warnings": []}],
        }
        with self.assertRaises(ValueError) as ctx:
            normalize_mcp_report(raw_bad_errors, self.identity)
        self.assertIn("'errors' must be a list of strings", str(ctx.exception))

        raw_bad_warnings = {
            "schema_version": 1,
            "total": 1,
            "passed": 1,
            "failed": 0,
            "skipped": 0,
            "duration": 0.1,
            "tests": [{"name": "GV2.Test", "state": "Success", "duration": 0.1, "errors": [], "warnings": 42}],
        }
        with self.assertRaises(ValueError) as ctx:
            normalize_mcp_report(raw_bad_warnings, self.identity)
        self.assertIn("'warnings' must be a list of strings", str(ctx.exception))

    def test_negative_ue_missing_required_counters_raises(self) -> None:
        """normalize_ue_json_report must reject missing counters without synthetic defaults."""
        base_ue = {
            "succeeded": 1,
            "succeededWithWarnings": 0,
            "failed": 0,
            "notRun": 0,
            "inProcess": 0,
            "totalDuration": 1.0,
            "tests": [{"fullTestPath": "GV2.Test", "state": "Success", "duration": 0.1}],
        }
        for counter in ("succeeded", "succeededWithWarnings", "failed", "notRun", "inProcess"):
            bad = dict(base_ue)
            del bad[counter]
            with self.assertRaises(ValueError) as ctx:
                normalize_ue_json_report(bad, self.identity)
            self.assertIn(f"missing required counter '{counter}'", str(ctx.exception))

        bad_dur = dict(base_ue)
        del bad_dur["totalDuration"]
        with self.assertRaises(ValueError) as ctx:
            normalize_ue_json_report(bad_dur, self.identity)
        self.assertIn("missing required field 'totalDuration'", str(ctx.exception))

    def test_negative_ue_test_missing_state_or_duration_raises(self) -> None:
        """normalize_ue_json_report must reject test record without state or duration."""
        bad_state = {
            "succeeded": 1,
            "succeededWithWarnings": 0,
            "failed": 0,
            "notRun": 0,
            "inProcess": 0,
            "totalDuration": 1.0,
            "tests": [{"fullTestPath": "GV2.Test", "duration": 0.1}],
        }
        with self.assertRaises(ValueError) as ctx:
            normalize_ue_json_report(bad_state, self.identity)
        self.assertIn("missing required 'state'", str(ctx.exception))

        bad_dur = {
            "succeeded": 1,
            "succeededWithWarnings": 0,
            "failed": 0,
            "notRun": 0,
            "inProcess": 0,
            "totalDuration": 1.0,
            "tests": [{"fullTestPath": "GV2.Test", "state": "Success"}],
        }
        with self.assertRaises(ValueError) as ctx:
            normalize_ue_json_report(bad_dur, self.identity)
        self.assertIn("missing required 'duration'", str(ctx.exception))

    def test_negative_validate_run_missing_schema_version(self) -> None:
        """validate_run must report diagnostic when report is missing schema_version."""
        report = self._make_valid_report(["GV2.Test1"])
        del report["schema_version"]
        diags = validate_run({"GV2.Test1"}, report, self.identity)
        self.assertTrue(any("missing required 'schema_version'" in d for d in diags))


class TestRunnersIntegration(unittest.TestCase):
    """Tests that local and CI runners fail closed and return correct exit codes."""

    def setUp(self) -> None:
        self.identity = {
            "run_id": "run-test-12345",
            "source_revision": "abcdef1234567890abcdef1234567890abcdef12",
            "source_diff_hash": "clean",
            "build_fingerprint": "fingerprint-linux-dev-v1",
        }

    def test_mcp_client_exact_task_id_required(self) -> None:
        """UnrealMcpClient must forbid calling GetTestResults or GetTestStatus without an exact task_id."""
        from Tools.MCP.mcp_client import UnrealMcpClient

        client = UnrealMcpClient(auto_initialize=False)
        with self.assertRaises(ValueError) as ctx:
            client.get_test_results("")
        self.assertIn("Calling GetTestResults without an exact task_id is forbidden", str(ctx.exception))

        with self.assertRaises(ValueError) as ctx:
            client.get_test_results("   ")
        self.assertIn("Calling GetTestResults without an exact task_id is forbidden", str(ctx.exception))

        with self.assertRaises(ValueError) as ctx:
            client.get_test_status("")
        self.assertIn("Calling GetTestStatus without an exact task_id is forbidden", str(ctx.exception))

        with self.assertRaises(ValueError) as ctx:
            client.get_test_status("   ")
        self.assertIn("Calling GetTestStatus without an exact task_id is forbidden", str(ctx.exception))

        class MissingEchoClient(UnrealMcpClient):
            def call_tool(self, *args, **kwargs):
                return {"state": "Completed", "tests": []}

        missing_echo = MissingEchoClient(auto_initialize=False)
        with self.assertRaises(ValueError):
            missing_echo.get_test_status("task-current")
        with self.assertRaises(ValueError):
            missing_echo.get_test_results("task-current")

    def test_run_ue_tests_verif_af_02_fail_closed(self) -> None:
        """VERIFY-AF-02: Runner must exit non-zero when total=1, skipped=1, NotRun."""
        from Tools.MCP import run_ue_tests

        class FakeClient:
            def __init__(self, **kw): self.last_response_id = 11
            def discover_tests(self, **kw): return {}
            def list_tests(self, **kw): return ["GV2.Audit.Synthetic"]
            def run_tests_by_filter(self, *a):
                return {
                    "schema_version": 1,
                    "total": 1,
                    "passed": 0,
                    "failed": 0,
                    "skipped": 1,
                    "duration": 0.0,
                    "tests": [{
                        "name": "GV2.Audit.Synthetic",
                        "state": "NotRun",
                        "duration": 0.0,
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

        this_test = self

        class FakeSuccessClient:
            def __init__(self, **kw): self.last_response_id = 12
            def discover_tests(self, **kw): return {}
            def list_tests(self, **kw): return ["GV2.Test.A"]
            def run_tests_by_filter(self, *a):
                return {
                    "schema_version": 1,
                    "total": 1,
                    "passed": 1,
                    "failed": 0,
                    "skipped": 0,
                    "duration": 0.05,
                    "run_identity": dict(this_test.identity),
                    "tests": [{
                        "name": "GV2.Test.A",
                        "state": "Success",
                        "duration": 0.05,
                        "errors": [],
                        "warnings": [],
                    }],
                }

        old_client = run_ue_tests.UnrealMcpClient
        old_sleep = run_ue_tests.time.sleep
        old_compute = run_ue_tests.compute_run_identity
        old_argv = sys.argv
        observed_run_ids = []

        def capture_identity(**kw):
            observed_run_ids.append(kw.get("run_id"))
            return {
                **self.identity,
                "run_id": kw.get("run_id") or self.identity["run_id"],
            }

        try:
            run_ue_tests.UnrealMcpClient = FakeSuccessClient
            run_ue_tests.time.sleep = lambda *_: None
            run_ue_tests.compute_run_identity = capture_identity
            sys.argv = ["run_ue_tests.py", "--filter", "StartsWith:GV2"]
            exit_code = run_ue_tests.main()
            self.assertEqual(exit_code, 0, "Successful test run must exit with 0")
            self.assertEqual(observed_run_ids, ["mcp-rpc:12"])
        finally:
            run_ue_tests.UnrealMcpClient = old_client
            run_ue_tests.time.sleep = old_sleep
            run_ue_tests.compute_run_identity = old_compute
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
        """Runner must poll with exact task_id until terminal result when RunTestsByFilter returns in-process."""
        from Tools.MCP import run_ue_tests

        this_test = self

        class FakeAsyncClient:
            def __init__(self, **kw):
                self.calls = 0
                self.polled_task_ids = []
            def discover_tests(self, **kw): return {}
            def list_tests(self, **kw): return ["GV2.AsyncTest"]
            def run_tests_by_filter(self, *a):
                return {"status": "InProcess", "taskId": "task-async-1"}
            def get_test_status(self, task_id):
                self.calls += 1
                self.polled_task_ids.append(task_id)
                return {
                    "taskId": task_id,
                    "state": "Completed" if self.calls >= 2 else "Running",
                    "bIsRunning": False if self.calls >= 2 else True,
                }
            def get_test_results(self, task_id):
                self.polled_task_ids.append(task_id)
                return {
                    "schema_version": 1,
                    "taskId": task_id,
                    "total": 1,
                    "passed": 1,
                    "failed": 0,
                    "skipped": 0,
                    "duration": 0.1,
                    "run_identity": dict(this_test.identity),
                    "tests": [{
                        "name": "GV2.AsyncTest",
                        "state": "Success",
                        "duration": 0.1,
                        "errors": [],
                        "warnings": [],
                    }],
                }

        fake_client = FakeAsyncClient()
        old_client = run_ue_tests.UnrealMcpClient
        old_sleep = run_ue_tests.time.sleep
        old_compute = run_ue_tests.compute_run_identity
        old_argv = sys.argv
        try:
            run_ue_tests.UnrealMcpClient = lambda **kw: fake_client
            run_ue_tests.time.sleep = lambda *_: None
            run_ue_tests.compute_run_identity = lambda **kw: {
                **self.identity,
                "run_id": kw.get("run_id") or self.identity["run_id"],
            }
            sys.argv = ["run_ue_tests.py", "--filter", "StartsWith:GV2"]
            exit_code = run_ue_tests.main()
            self.assertEqual(exit_code, 0, "Async task must be polled until completion and exit 0")
            self.assertTrue(all(tid == "task-async-1" for tid in fake_client.polled_task_ids))
            self.assertGreaterEqual(len(fake_client.polled_task_ids), 2)
        finally:
            run_ue_tests.UnrealMcpClient = old_client
            run_ue_tests.time.sleep = old_sleep
            run_ue_tests.compute_run_identity = old_compute
            sys.argv = old_argv

    def test_run_ue_tests_async_without_task_id_fails_closed(self) -> None:
        """Runner must fail closed (exit 1) if in-process response lacks an exact task_id."""
        from Tools.MCP import run_ue_tests

        class FakeNoTaskIdClient:
            def __init__(self, **kw): pass
            def discover_tests(self, **kw): return {}
            def list_tests(self, **kw): return ["GV2.AsyncTest"]
            def run_tests_by_filter(self, *a):
                return {"status": "InProcess"}  # missing taskId!

        old_client = run_ue_tests.UnrealMcpClient
        old_sleep = run_ue_tests.time.sleep
        old_argv = sys.argv
        try:
            run_ue_tests.UnrealMcpClient = FakeNoTaskIdClient
            run_ue_tests.time.sleep = lambda *_: None
            sys.argv = ["run_ue_tests.py", "--filter", "StartsWith:GV2"]
            exit_code = run_ue_tests.main()
            self.assertEqual(exit_code, 1, "Async response without task_id must fail closed with exit 1")
        finally:
            run_ue_tests.UnrealMcpClient = old_client
            run_ue_tests.time.sleep = old_sleep
            sys.argv = old_argv

    def test_run_ue_tests_async_mismatched_results_task_id_fails_closed(self) -> None:
        """Runner must fail closed (exit 1) if GetTestResults returns a mismatched task_id."""
        from Tools.MCP import run_ue_tests

        this_test = self

        class FakeMismatchedClient:
            def __init__(self, **kw): pass
            def discover_tests(self, **kw): return {}
            def list_tests(self, **kw): return ["GV2.AsyncTest"]
            def run_tests_by_filter(self, *a):
                return {"status": "InProcess", "taskId": "task-expected-1"}
            def get_test_status(self, task_id):
                return {"taskId": task_id, "state": "Completed", "bIsRunning": False}
            def get_test_results(self, task_id):
                raise ValueError(
                    "Mismatched task_id in GetTestResults: expected "
                    f"'{task_id}', got 'task-wrong-foreign-2'"
                )

        old_client = run_ue_tests.UnrealMcpClient
        old_sleep = run_ue_tests.time.sleep
        old_compute = run_ue_tests.compute_run_identity
        old_argv = sys.argv
        try:
            run_ue_tests.UnrealMcpClient = FakeMismatchedClient
            run_ue_tests.time.sleep = lambda *_: None
            run_ue_tests.compute_run_identity = lambda **kw: {
                **self.identity,
                "run_id": kw.get("run_id") or self.identity["run_id"],
            }
            sys.argv = ["run_ue_tests.py", "--filter", "StartsWith:GV2"]
            exit_code = run_ue_tests.main()
            self.assertEqual(exit_code, 1, "Mismatched task_id in results must fail closed with exit 1")
        finally:
            run_ue_tests.UnrealMcpClient = old_client
            run_ue_tests.time.sleep = old_sleep
            run_ue_tests.compute_run_identity = old_compute
            sys.argv = old_argv

    def test_run_ue_tests_async_results_without_task_id_fails_closed(self) -> None:
        """A task-less result cannot be relabelled as belonging to the requested async run."""
        from Tools.MCP import run_ue_tests

        this_test = self

        class FakeMissingResultTaskClient:
            def __init__(self, **kw): pass
            def discover_tests(self, **kw): return {}
            def list_tests(self, **kw): return ["GV2.AsyncTest"]
            def run_tests_by_filter(self, *a):
                return {"status": "InProcess", "taskId": "task-current-1"}
            def get_test_status(self, task_id):
                return {"taskId": task_id, "state": "Completed", "bIsRunning": False}
            def get_test_results(self, task_id):
                raise ValueError(f"GetTestResults response is missing exact task_id '{task_id}'")

        old_client = run_ue_tests.UnrealMcpClient
        old_sleep = run_ue_tests.time.sleep
        old_compute = run_ue_tests.compute_run_identity
        old_argv = sys.argv
        try:
            run_ue_tests.UnrealMcpClient = FakeMissingResultTaskClient
            run_ue_tests.time.sleep = lambda *_: None
            run_ue_tests.compute_run_identity = lambda **kw: {
                **self.identity,
                "run_id": kw.get("run_id") or self.identity["run_id"],
            }
            sys.argv = ["run_ue_tests.py", "--filter", "StartsWith:GV2"]
            self.assertEqual(run_ue_tests.main(), 1)
        finally:
            run_ue_tests.UnrealMcpClient = old_client
            run_ue_tests.time.sleep = old_sleep
            run_ue_tests.compute_run_identity = old_compute
            sys.argv = old_argv

    def test_run_ue_tests_sync_report_without_schema_version_fails_closed(self) -> None:
        """The runner must not invent a schema version for an unversioned report."""
        from Tools.MCP import run_ue_tests

        this_test = self

        class FakeUnversionedClient:
            def __init__(self, **kw): self.last_response_id = 7
            def discover_tests(self, **kw): return {}
            def list_tests(self, **kw): return ["GV2.Test.A"]
            def run_tests_by_filter(self, *a):
                return {
                    "total": 1,
                    "passed": 1,
                    "failed": 0,
                    "skipped": 0,
                    "duration": 0.1,
                    "run_identity": dict(this_test.identity),
                    "tests": [{
                        "name": "GV2.Test.A",
                        "state": "Success",
                        "duration": 0.1,
                        "errors": [],
                        "warnings": [],
                    }],
                }

        old_client = run_ue_tests.UnrealMcpClient
        old_sleep = run_ue_tests.time.sleep
        old_compute = run_ue_tests.compute_run_identity
        old_argv = sys.argv
        try:
            run_ue_tests.UnrealMcpClient = FakeUnversionedClient
            run_ue_tests.time.sleep = lambda *_: None
            run_ue_tests.compute_run_identity = lambda **kw: {
                **self.identity,
                "run_id": kw.get("run_id") or self.identity["run_id"],
            }
            sys.argv = ["run_ue_tests.py", "--filter", "StartsWith:GV2"]
            self.assertEqual(run_ue_tests.main(), 1)
        finally:
            run_ue_tests.UnrealMcpClient = old_client
            run_ue_tests.time.sleep = old_sleep
            run_ue_tests.compute_run_identity = old_compute
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
