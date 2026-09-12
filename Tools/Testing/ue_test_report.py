#!/usr/bin/env python3
"""Unified report validator and normalizer for Unreal Engine automation test runs.

Validates that:
1. Discovered tests strictly match completed unique records (no missing, no extra, no duplicates).
2. Every test record has state 'Success' and zero errors.
3. Summary counters (total, passed, failed, skipped) are consistent with records.
4. Run identity (run_id, source_revision, source_diff_hash, build_fingerprint)
   matches the expected current source/build state.
"""

from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys
import uuid
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Union


REQUIRED_IDENTITY_KEYS = (
    "run_id",
    "source_revision",
    "source_diff_hash",
    "build_fingerprint",
)


def compute_source_revision(repo_root: Path) -> str:
    """Returns current git commit hash (HEAD) or 'unknown_revision'."""
    try:
        res = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            cwd=str(repo_root),
            capture_output=True,
            text=True,
            check=True,
            timeout=10,
        )
        return res.stdout.strip()
    except Exception:
        return "unknown_revision"


def compute_source_diff_hash(repo_root: Path) -> str:
    """Returns sha256 hash of git diff against HEAD, or 'clean' if no changes."""
    try:
        res = subprocess.run(
            ["git", "diff", "HEAD"],
            cwd=str(repo_root),
            capture_output=True,
            text=True,
            check=True,
            timeout=10,
        )
        diff_text = res.stdout
        if not diff_text.strip():
            return "clean"
        return hashlib.sha256(diff_text.encode("utf-8")).hexdigest()
    except Exception:
        return "unknown_diff"


def compute_build_fingerprint(repo_root: Path) -> str:
    """Computes a SHA-256 fingerprint over built Unreal Engine binaries in Binaries/Linux."""
    binaries_dir = repo_root / "Binaries" / "Linux"
    if not binaries_dir.is_dir():
        return "missing_binaries"

    # Find project-owned .so files
    so_files = sorted(binaries_dir.glob("libUnrealEditor-GV2*.so"))
    if not so_files:
        so_files = sorted(binaries_dir.glob("*.so"))

    if not so_files:
        return "no_so_binaries"

    h = hashlib.sha256()
    for f in so_files:
        try:
            stat = f.stat()
            # Include filename, size, and mtime_ns
            h.update(f"{f.name}:{stat.st_size}:{stat.st_mtime_ns}".encode("utf-8"))
            # Sample first and last 64KB
            with open(f, "rb") as fp:
                head = fp.read(65536)
                h.update(head)
                if stat.st_size > 65536:
                    fp.seek(max(0, stat.st_size - 65536))
                    tail = fp.read(65536)
                    h.update(tail)
        except Exception as e:
            h.update(f"error:{f.name}:{e}".encode("utf-8"))

    return h.hexdigest()


def compute_run_identity(
    project_root: Optional[Union[str, Path]] = None,
    run_id: Optional[str] = None,
) -> Dict[str, str]:
    """Computes run identity dictionary containing all required identity keys."""
    if project_root is None:
        # Default to repository root relative to this script
        project_root = Path(__file__).resolve().parent.parent.parent
    else:
        project_root = Path(project_root).resolve()

    return {
        "run_id": run_id or uuid.uuid4().hex,
        "source_revision": compute_source_revision(project_root),
        "source_diff_hash": compute_source_diff_hash(project_root),
        "build_fingerprint": compute_build_fingerprint(project_root),
    }


def normalize_mcp_report(raw_report: Any, run_identity: Dict[str, str]) -> Dict[str, Any]:
    """Adapts MCP RunTestsByFilter response into normalized report dict.

    Raises ValueError if raw_report does not match expected schema.
    """
    if not isinstance(raw_report, dict):
        raise ValueError(f"Invalid MCP report: expected dict, got {type(raw_report).__name__}")

    if "tests" not in raw_report or not isinstance(raw_report["tests"], list):
        raise ValueError("Invalid MCP report: 'tests' field is missing or not a list")

    normalized_tests: List[Dict[str, Any]] = []
    for item in raw_report["tests"]:
        if not isinstance(item, dict):
            raise ValueError(f"Invalid test record in MCP report: {item}")

        name = item.get("name") or item.get("fullTestPath")
        if not name or not isinstance(name, str):
            raise ValueError(f"MCP test record missing name: {item}")

        state = item.get("state", "Unknown")
        duration = float(item.get("duration", 0.0))
        errors = item.get("errors", [])
        if isinstance(errors, (str, int)):
            errors = [str(errors)] if errors else []
        elif not isinstance(errors, list):
            errors = [str(errors)]

        warnings = item.get("warnings", [])
        if isinstance(warnings, (str, int)):
            warnings = [str(warnings)] if warnings else []
        elif not isinstance(warnings, list):
            warnings = [str(warnings)]

        normalized_tests.append({
            "name": name,
            "state": str(state),
            "duration": duration,
            "errors": errors,
            "warnings": warnings,
        })

    total = int(raw_report.get("total", len(normalized_tests)))
    passed = int(raw_report.get("passed", 0))
    failed = int(raw_report.get("failed", 0))
    skipped = int(raw_report.get("skipped", 0))
    duration = float(raw_report.get("duration", 0.0))

    return {
        "run_identity": dict(run_identity),
        "tests": normalized_tests,
        "total": total,
        "passed": passed,
        "failed": failed,
        "skipped": skipped,
        "duration": duration,
    }


def normalize_ue_json_report(
    ue_data: Union[Dict[str, Any], str, Path],
    run_identity: Dict[str, str],
) -> Dict[str, Any]:
    """Adapts Unreal Engine index.json (from -ReportExportPath) into normalized report dict.

    Raises ValueError if ue_data does not match expected schema or cannot be parsed.
    """
    if isinstance(ue_data, (str, Path)):
        p = Path(ue_data)
        if not p.is_file():
            raise FileNotFoundError(f"UE report file not found: {p}")
        try:
            with open(p, "r", encoding="utf-8-sig") as f:
                data = json.load(f)
        except json.JSONDecodeError as e:
            raise ValueError(f"Invalid JSON in UE report file {p}: {e}") from e
    elif isinstance(ue_data, dict):
        data = ue_data
    else:
        raise ValueError(f"Invalid UE report data: expected dict or path, got {type(ue_data).__name__}")

    if "tests" not in data or not isinstance(data["tests"], list):
        raise ValueError("Invalid UE report: 'tests' field is missing or not a list")

    normalized_tests: List[Dict[str, Any]] = []
    for item in data["tests"]:
        if not isinstance(item, dict):
            raise ValueError(f"Invalid test record in UE report: {item}")

        name = item.get("fullTestPath") or item.get("testDisplayName") or item.get("name")
        if not name or not isinstance(name, str):
            raise ValueError(f"UE test record missing name: {item}")

        state = item.get("state", "Unknown")
        duration = float(item.get("duration", 0.0))

        # Extract errors and warnings from entries or fields
        errors: List[str] = []
        warnings: List[str] = []
        entries = item.get("entries", [])
        if isinstance(entries, list):
            for entry in entries:
                if isinstance(entry, dict):
                    event = entry.get("event", {})
                    if isinstance(event, dict):
                        ev_type = event.get("type", "")
                        ev_msg = event.get("message", "")
                        if ev_type == "Error":
                            errors.append(ev_msg or "Error event")
                        elif ev_type == "Warning":
                            warnings.append(ev_msg or "Warning event")

        # Fallback to direct error count if entries did not specify message
        direct_errors = item.get("errors", 0)
        if isinstance(direct_errors, int) and direct_errors > len(errors):
            errors.extend([f"Unspecified error #{i+1}" for i in range(direct_errors - len(errors))])

        normalized_tests.append({
            "name": name,
            "state": str(state),
            "duration": duration,
            "errors": errors,
            "warnings": warnings,
        })

    succeeded = int(data.get("succeeded", 0))
    succeeded_with_warnings = int(data.get("succeededWithWarnings", 0))
    passed = succeeded + succeeded_with_warnings
    failed = int(data.get("failed", 0))
    not_run = int(data.get("notRun", 0))
    in_process = int(data.get("inProcess", 0))
    total = passed + failed + not_run + in_process
    duration = float(data.get("totalDuration", 0.0))

    return {
        "run_identity": dict(run_identity),
        "tests": normalized_tests,
        "total": total,
        "passed": passed,
        "failed": failed,
        "skipped": not_run,
        "in_process": in_process,
        "duration": duration,
    }


def validate_run(
    discovered: Set[str],
    report: Dict[str, Any],
    run_identity: Dict[str, str],
) -> List[str]:
    """Validates an automation test run report against discovered tests and expected run identity.

    Returns:
        List of error diagnostic strings.
        An empty list [] signifies complete, terminal success.
    """
    diagnostics: List[str] = []

    # 1. Validate discovered set
    if not isinstance(discovered, (set, frozenset)):
        diagnostics.append(f"Discovered tests must be a set, got {type(discovered).__name__}.")
    elif not discovered:
        diagnostics.append("Discovered test set is empty.")

    # 2. Validate expected run identity
    if not isinstance(run_identity, dict):
        diagnostics.append(f"Expected run_identity must be a dict, got {type(run_identity).__name__}.")
    else:
        for k in REQUIRED_IDENTITY_KEYS:
            val = run_identity.get(k)
            if not val or not isinstance(val, str) or not val.strip():
                diagnostics.append(f"Expected run_identity has missing or empty key '{k}'.")

    # 3. Validate report structure and identity
    if not isinstance(report, dict):
        diagnostics.append(f"Report must be a dict, got {type(report).__name__}.")
        return diagnostics

    rep_identity = report.get("run_identity")
    if not isinstance(rep_identity, dict):
        diagnostics.append("Report is missing 'run_identity' dictionary.")
    elif isinstance(run_identity, dict):
        for k in REQUIRED_IDENTITY_KEYS:
            expected_v = run_identity.get(k)
            actual_v = rep_identity.get(k)
            if actual_v != expected_v:
                diagnostics.append(
                    f"Run identity mismatch for '{k}': expected '{expected_v}', got '{actual_v}'."
                )

    # 4. Validate test records
    tests = report.get("tests")
    if not isinstance(tests, list):
        diagnostics.append("Report 'tests' is missing or not a list.")
        return diagnostics
    if not tests:
        diagnostics.append("Report contains no test records.")
        return diagnostics

    seen_names: Set[str] = set()
    completed_names: Set[str] = set()

    for idx, t in enumerate(tests):
        if not isinstance(t, dict):
            diagnostics.append(f"Test record #{idx} is not a dict: {t}")
            continue

        name = t.get("name")
        if not name or not isinstance(name, str):
            diagnostics.append(f"Test record #{idx} has missing or non-string 'name': {t}")
            continue

        if name in seen_names:
            diagnostics.append(f"Duplicate test record in report: '{name}'.")
        seen_names.add(name)
        completed_names.add(name)

        state = t.get("state")
        if state != "Success":
            diagnostics.append(f"Test '{name}' has non-success state '{state}'.")

        errors = t.get("errors", [])
        if errors:
            diagnostics.append(f"Test '{name}' reported errors (state='{state}'): {errors}")

    # 5. Verify discovered set exactly matches completed set
    if isinstance(discovered, (set, frozenset)) and discovered:
        missing = discovered - completed_names
        if missing:
            diagnostics.append(
                f"Tests discovered but missing from completed report ({len(missing)}): {sorted(missing)}"
            )

        extra = completed_names - discovered
        if extra:
            diagnostics.append(
                f"Tests in report but not in discovered set ({len(extra)}): {sorted(extra)}"
            )

    # 6. Verify summary counters consistency
    record_count = len(tests)
    if "total" in report:
        total = report["total"]
        if total != record_count:
            diagnostics.append(
                f"Report counter total ({total}) does not match test records count ({record_count})."
            )
    else:
        diagnostics.append("Report is missing required counter 'total'.")

    if "passed" in report:
        passed = report["passed"]
        if passed != record_count:
            diagnostics.append(
                f"Report counter passed ({passed}) does not match test records count ({record_count})."
            )
    else:
        diagnostics.append("Report is missing required counter 'passed'.")

    if "failed" in report:
        failed = report["failed"]
        if failed != 0:
            diagnostics.append(f"Report counter failed is non-zero: {failed}.")
    else:
        diagnostics.append("Report is missing required counter 'failed'.")

    if "skipped" in report:
        skipped = report["skipped"]
        if skipped != 0:
            diagnostics.append(f"Report counter skipped is non-zero: {skipped}.")
    else:
        diagnostics.append("Report is missing required counter 'skipped'.")

    if "in_process" in report:
        in_process = report["in_process"]
        if in_process != 0:
            diagnostics.append(f"Report counter in_process is non-zero: {in_process}.")

    return diagnostics
