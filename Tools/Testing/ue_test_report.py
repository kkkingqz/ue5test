#!/usr/bin/env python3
"""Unified report validator and normalizer for Unreal Engine automation test runs.

Validates that:
1. Discovered tests strictly match completed unique records (no missing, no extra, no duplicates).
2. Every test record has state 'Success' and zero errors.
3. Summary counters (total, passed, failed, skipped) are consistent with records.
4. Binary identity (source_revision, source_diff_hash, build_fingerprint, engine_version) matches the
   current source/build state, while run_id carries process/transport correlation.
"""

from __future__ import annotations

import ast
import hashlib
import inspect
import json
import os
import re
import subprocess
import sys
import textwrap
import uuid
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Set, Union


REQUIRED_IDENTITY_KEYS = (
    "run_id",
    "source_revision",
    "source_diff_hash",
    "build_fingerprint",
    "engine_version",
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
    """Returns sha256 hash of git diff against HEAD and untracked files, or 'clean' if no changes."""
    try:
        diff_res = subprocess.run(
            ["git", "diff", "HEAD"],
            cwd=str(repo_root),
            capture_output=True,
            text=True,
            check=True,
            timeout=15,
        )
        diff_text = diff_res.stdout

        untracked_res = subprocess.run(
            ["git", "ls-files", "--others", "--exclude-standard"],
            cwd=str(repo_root),
            capture_output=True,
            text=True,
            check=True,
            timeout=15,
        )
        untracked_files = [line.strip() for line in untracked_res.stdout.splitlines() if line.strip()]

        if not diff_text.strip() and not untracked_files:
            return "clean"

        h = hashlib.sha256()
        h.update(diff_text.encode("utf-8"))

        for rel_path_str in sorted(untracked_files):
            file_path = repo_root / rel_path_str
            if file_path.is_file():
                try:
                    file_bytes = file_path.read_bytes()
                    file_hash = hashlib.sha256(file_bytes).hexdigest()
                    h.update(f"\nuntracked:{rel_path_str}:{file_hash}\n".encode("utf-8"))
                except Exception as e:
                    return f"error:untracked:{rel_path_str}:{e}"
            elif file_path.exists():
                h.update(f"\nuntracked:{rel_path_str}:special\n".encode("utf-8"))

        return h.hexdigest()
    except Exception:
        return "unknown_diff"


def compute_build_fingerprint(repo_root: Path) -> str:
    """Computes a deterministic SHA-256 fingerprint over built Unreal Engine binaries in Binaries/Linux."""
    binaries_dir = repo_root / "Binaries" / "Linux"
    if not binaries_dir.is_dir():
        return "missing_binaries"

    # Find project-owned .so files
    so_files = sorted(binaries_dir.glob("libUnrealEditor-GV2*.so"))
    if not so_files:
        return "missing_binaries"

    h = hashlib.sha256()
    for f in so_files:
        try:
            stat = f.stat()
            h.update(f.name.encode("utf-8"))
            h.update(b":")
            h.update(str(stat.st_size).encode("utf-8"))
            h.update(b":")
            with open(f, "rb") as fp:
                while chunk := fp.read(65536):
                    h.update(chunk)
        except Exception as e:
            return f"error:{f.name}:{e}"

    return h.hexdigest()


def compute_engine_version(repo_root: Path) -> str:
    """Reads the engine version the project declares it is built against.

    The expected side must not come from the engine the binary happened to load:
    it comes from GV2.uproject, which is versioned and reviewable, so moving to a
    new engine is a diff someone signs off on rather than a silent rebuild. The
    runtime side reports ENGINE_MAJOR_VERSION/ENGINE_MINOR_VERSION compiled into
    the loaded module, so the two disagree exactly when the build used another
    engine. Patch-level moves within one minor version are not bound.
    """
    uproject = repo_root / "GV2.uproject"
    if not uproject.is_file():
        return "unknown_uproject_missing"
    try:
        data = json.loads(uproject.read_text(encoding="utf-8"))
    except Exception as e:
        return f"error:GV2.uproject:{e}"
    association = data.get("EngineAssociation")
    if not isinstance(association, str) or not re.fullmatch(r"\d+\.\d+", association.strip()):
        # Source builds put a GUID here. Fail closed rather than guess a version.
        return "unknown_engine_association"
    return association.strip()


def compute_run_identity(
    project_root: Optional[Union[str, Path]] = None,
    run_id: Optional[str] = None,
) -> Dict[str, str]:
    """Builds expected binary identity plus the caller-proven execution correlation id."""
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
        "engine_version": compute_engine_version(project_root),
    }


def extract_runtime_identity_from_ue_data(
    ue_data: Dict[str, Any],
    report_file_path: Optional[Path] = None,
) -> Dict[str, str]:
    """Extracts runtime-originated identity from UE automation test entries or sibling artifact."""
    if "run_identity" in ue_data and isinstance(ue_data["run_identity"], dict) and ue_data["run_identity"]:
        return dict(ue_data["run_identity"])

    # 1. Look for GV2.Runtime.ModuleIdentity test entries
    tests = ue_data.get("tests", [])
    if isinstance(tests, list):
        for item in tests:
            if not isinstance(item, dict):
                continue
            name = item.get("fullTestPath") or item.get("testDisplayName") or item.get("name")
            if name == "GV2.Runtime.ModuleIdentity":
                for entry in item.get("entries", []):
                    if isinstance(entry, dict):
                        ev = entry.get("event", {})
                        if isinstance(ev, dict):
                            msg = ev.get("message", "")
                            if "GV2_RUNTIME_IDENTITY:" in msg:
                                json_part = msg.split("GV2_RUNTIME_IDENTITY:", 1)[1].strip()
                                try:
                                    parsed = json.loads(json_part)
                                    if isinstance(parsed, dict):
                                        return parsed
                                except Exception:
                                    pass

    # 2. Look for sibling runtime_identity.json next to index.json
    if report_file_path is not None:
        sibling = report_file_path.parent / "runtime_identity.json"
        if sibling.is_file():
            try:
                with open(sibling, "r", encoding="utf-8") as f:
                    parsed = json.load(f)
                    if isinstance(parsed, dict):
                        return parsed
            except Exception:
                pass

    return {}


def extract_runtime_identity_from_mcp_data(
    raw_report: Dict[str, Any],
    repo_root: Optional[Path] = None,
) -> Dict[str, str]:
    """Extracts runtime-originated identity from MCP test report or Saved automation artifacts."""
    if "run_identity" in raw_report and isinstance(raw_report["run_identity"], dict) and raw_report["run_identity"]:
        return dict(raw_report["run_identity"])

    tests = raw_report.get("tests", [])
    if isinstance(tests, list):
        for item in tests:
            if not isinstance(item, dict):
                continue
            name = item.get("name") or item.get("fullTestPath")
            if name == "GV2.Runtime.ModuleIdentity":
                candidates = item.get("entries", []) + item.get("warnings", []) + item.get("errors", [])
                for entry in candidates:
                    msg = entry.get("message", "") if isinstance(entry, dict) else str(entry)
                    if "GV2_RUNTIME_IDENTITY:" in msg:
                        json_part = msg.split("GV2_RUNTIME_IDENTITY:", 1)[1].strip()
                        try:
                            parsed = json.loads(json_part)
                            if isinstance(parsed, dict):
                                return parsed
                        except Exception:
                            pass

    if repo_root is not None:
        saved_identity = repo_root / "Saved" / "Automation" / "Reports" / "runtime_identity.json"
        if saved_identity.is_file():
            try:
                with open(saved_identity, "r", encoding="utf-8") as f:
                    parsed = json.load(f)
                    if isinstance(parsed, dict):
                        return parsed
            except Exception:
                pass

    return {}



SUPPORTED_REPORT_SCHEMA_VERSIONS = (1, "1", "1.0", "gv2-mcp-report-v1", "gv2-ue-report-v1")


def normalize_mcp_report(
    raw_report: Any,
    run_id: Optional[Union[str, Dict[str, str]]] = None,
    repo_root: Optional[Path] = None,
    run_identity: Optional[Dict[str, str]] = None,
    expected_task_id: Optional[str] = None,
) -> Dict[str, Any]:
    """Adapts MCP RunTestsByFilter response into normalized report dict.

    Extracts runtime-originated identity from test results or saved artifacts,
    or accepts explicit identity for testing/fixtures.
    Strictly validates all required fields, types, and schema version.
    Zero synthetic defaults.
    Raises ValueError if raw_report does not match expected schema.
    """
    if not isinstance(raw_report, dict):
        raise ValueError(f"Invalid MCP report: expected dict, got {type(raw_report).__name__}")

    # 1. Require schema version
    if "schema_version" not in raw_report and "schemaVersion" not in raw_report:
        raise ValueError("Invalid MCP report: missing required 'schema_version'")
    schema_ver = raw_report.get("schema_version", raw_report.get("schemaVersion"))
    if schema_ver not in SUPPORTED_REPORT_SCHEMA_VERSIONS:
        raise ValueError(f"Invalid MCP report: unsupported schema_version {schema_ver!r}")

    # 2. Validate task_id if present or expected
    raw_task_id = raw_report.get("task_id") or raw_report.get("taskId")
    if raw_task_id is not None:
        if not isinstance(raw_task_id, str) or not raw_task_id.strip():
            raise ValueError(f"Invalid MCP report: 'task_id' must be a non-empty string, got {raw_task_id!r}")
    if expected_task_id is not None:
        if not isinstance(expected_task_id, str) or not expected_task_id.strip():
            raise ValueError(f"expected_task_id must be a non-empty string, got {expected_task_id!r}")
        if raw_task_id is None:
            raise ValueError(
                f"MCP report is not bound to expected task_id '{expected_task_id}': task_id is missing"
            )
        if raw_task_id != expected_task_id:
            raise ValueError(
                f"MCP report task_id mismatch: expected '{expected_task_id}', got '{raw_task_id}'"
            )

    # 3. Require tests list
    if "tests" not in raw_report or not isinstance(raw_report["tests"], list):
        raise ValueError("Invalid MCP report: 'tests' field is missing or not a list")

    effective_run_id: Optional[str] = None
    if isinstance(run_id, dict):
        run_identity = run_id
    elif isinstance(run_id, str):
        effective_run_id = run_id

    # 4. Strictly validate each test item (zero synthetic defaults)
    normalized_tests: List[Dict[str, Any]] = []
    for idx, item in enumerate(raw_report["tests"]):
        if not isinstance(item, dict):
            raise ValueError(f"Invalid test record #{idx} in MCP report: expected dict, got {type(item).__name__}")

        name = item.get("name") or item.get("fullTestPath")
        if not name or not isinstance(name, str) or not name.strip():
            raise ValueError(f"MCP test record #{idx} missing or invalid name: {item}")

        if "state" not in item:
            raise ValueError(f"MCP test record #{idx} ('{name}') missing required 'state'")
        state = item["state"]
        if not isinstance(state, str) or not state.strip():
            raise ValueError(f"MCP test record #{idx} ('{name}') invalid 'state': {state!r}")

        if "duration" not in item:
            raise ValueError(f"MCP test record #{idx} ('{name}') missing required 'duration'")
        item_dur = item["duration"]
        if not isinstance(item_dur, (int, float)) or isinstance(item_dur, bool) or item_dur < 0.0:
            raise ValueError(f"MCP test record #{idx} ('{name}') invalid 'duration': {item_dur!r}")

        if "errors" not in item:
            raise ValueError(f"MCP test record #{idx} ('{name}') missing required 'errors'")
        errors = item["errors"]
        if not isinstance(errors, list) or not all(isinstance(e, str) for e in errors):
            raise ValueError(
                f"MCP test record #{idx} ('{name}') 'errors' must be a list of strings, got {type(errors).__name__}"
            )

        if "warnings" not in item:
            raise ValueError(f"MCP test record #{idx} ('{name}') missing required 'warnings'")
        warnings = item["warnings"]
        if not isinstance(warnings, list) or not all(isinstance(w, str) for w in warnings):
            raise ValueError(
                f"MCP test record #{idx} ('{name}') 'warnings' must be a list of strings, got {type(warnings).__name__}"
            )

        normalized_tests.append({
            "name": name,
            "state": state,
            "duration": float(item_dur),
            "errors": list(errors),
            "warnings": list(warnings),
        })

    # 5. Strictly validate counters and duration on root report (zero synthetic defaults)
    for req_counter in ("total", "passed", "failed", "skipped"):
        if req_counter not in raw_report:
            raise ValueError(f"Invalid MCP report: missing required counter '{req_counter}'")
        c_val = raw_report[req_counter]
        if not isinstance(c_val, int) or isinstance(c_val, bool) or c_val < 0:
            raise ValueError(f"Invalid MCP report: counter '{req_counter}' must be a non-negative integer, got {c_val!r}")

    total = raw_report["total"]
    passed = raw_report["passed"]
    failed = raw_report["failed"]
    skipped = raw_report["skipped"]

    if "duration" not in raw_report:
        raise ValueError("Invalid MCP report: missing required field 'duration'")
    rep_dur = raw_report["duration"]
    if not isinstance(rep_dur, (int, float)) or isinstance(rep_dur, bool) or rep_dur < 0.0:
        raise ValueError(f"Invalid MCP report: 'duration' must be a non-negative number, got {rep_dur!r}")
    duration = float(rep_dur)

    if run_identity is not None:
        extracted_identity = dict(run_identity)
    else:
        extracted_identity = extract_runtime_identity_from_mcp_data(raw_report, repo_root=repo_root)
        if effective_run_id:
            extracted_identity["run_id"] = effective_run_id

    result = {
        "schema_version": 1,
        "run_identity": extracted_identity,
        "tests": normalized_tests,
        "total": total,
        "passed": passed,
        "failed": failed,
        "skipped": skipped,
        "duration": duration,
    }
    effective_task_id = expected_task_id or raw_task_id
    if effective_task_id:
        result["task_id"] = effective_task_id
    return result


def normalize_ue_json_report(
    ue_data: Union[Dict[str, Any], str, Path],
    run_id: Optional[Union[str, Dict[str, str]]] = None,
    report_file_path: Optional[Path] = None,
    run_identity: Optional[Dict[str, str]] = None,
) -> Dict[str, Any]:
    """Adapts Unreal Engine index.json (from -ReportExportPath) into normalized report dict.

    Extracts runtime-originated identity from test results or saved artifacts,
    or accepts explicit identity for testing/fixtures.
    Strictly validates all required fields and types with zero synthetic defaults.
    Raises ValueError if ue_data does not match expected schema or cannot be parsed.
    """
    file_path: Optional[Path] = None
    if isinstance(ue_data, (str, Path)):
        file_path = Path(ue_data)
        if not file_path.is_file():
            raise FileNotFoundError(f"UE report file not found: {file_path}")
        try:
            with open(file_path, "r", encoding="utf-8-sig") as f:
                data = json.load(f)
        except json.JSONDecodeError as e:
            raise ValueError(f"Invalid JSON in UE report file {file_path}: {e}") from e
    elif isinstance(ue_data, dict):
        data = ue_data
        file_path = report_file_path
    else:
        raise ValueError(f"Invalid UE report data: expected dict or path, got {type(ue_data).__name__}")

    # 1. Validate schema version if present
    if "schema_version" in data or "schemaVersion" in data:
        s_ver = data.get("schema_version", data.get("schemaVersion"))
        if s_ver not in SUPPORTED_REPORT_SCHEMA_VERSIONS:
            raise ValueError(f"Invalid UE report: unsupported schema_version {s_ver!r}")

    # 2. Require tests list
    if "tests" not in data or not isinstance(data["tests"], list):
        raise ValueError("Invalid UE report: 'tests' field is missing or not a list")

    effective_run_id: Optional[str] = None
    if isinstance(run_id, dict):
        run_identity = run_id
    elif isinstance(run_id, str):
        effective_run_id = run_id

    # 3. Strictly validate each test item (zero synthetic defaults)
    normalized_tests: List[Dict[str, Any]] = []
    for idx, item in enumerate(data["tests"]):
        if not isinstance(item, dict):
            raise ValueError(f"Invalid test record #{idx} in UE report: expected dict, got {type(item).__name__}")

        name = item.get("fullTestPath") or item.get("testDisplayName") or item.get("name")
        if not name or not isinstance(name, str) or not name.strip():
            raise ValueError(f"UE test record #{idx} missing or invalid name: {item}")

        if "state" not in item:
            raise ValueError(f"UE test record #{idx} ('{name}') missing required 'state'")
        state = item["state"]
        if not isinstance(state, str) or not state.strip():
            raise ValueError(f"UE test record #{idx} ('{name}') invalid 'state': {state!r}")

        if "duration" not in item:
            raise ValueError(f"UE test record #{idx} ('{name}') missing required 'duration'")
        item_dur = item["duration"]
        if not isinstance(item_dur, (int, float)) or isinstance(item_dur, bool) or item_dur < 0.0:
            raise ValueError(f"UE test record #{idx} ('{name}') invalid 'duration': {item_dur!r}")

        # Extract errors and warnings from entries or fields
        errors: List[str] = []
        warnings: List[str] = []
        if "entries" in item:
            entries = item["entries"]
            if not isinstance(entries, list):
                raise ValueError(
                    f"UE test record #{idx} ('{name}') 'entries' must be a list, got {type(entries).__name__}"
                )
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
        if "errors" in item:
            direct_errors = item["errors"]
            if not isinstance(direct_errors, int) or isinstance(direct_errors, bool) or direct_errors < 0:
                raise ValueError(
                    f"UE test record #{idx} ('{name}') 'errors' counter must be a non-negative integer, got {direct_errors!r}"
                )
            if direct_errors > len(errors):
                errors.extend([f"Unspecified error #{i+1}" for i in range(direct_errors - len(errors))])

        if "warnings" in item:
            direct_warnings = item["warnings"]
            if not isinstance(direct_warnings, int) or isinstance(direct_warnings, bool) or direct_warnings < 0:
                raise ValueError(
                    f"UE test record #{idx} ('{name}') 'warnings' counter must be a non-negative integer, got {direct_warnings!r}"
                )

        normalized_tests.append({
            "name": name,
            "state": state,
            "duration": float(item_dur),
            "errors": errors,
            "warnings": warnings,
        })

    # 4. Strictly validate all required counters and duration on root report (zero synthetic defaults)
    for req_counter in ("succeeded", "succeededWithWarnings", "failed", "notRun", "inProcess"):
        if req_counter not in data:
            raise ValueError(f"Invalid UE report: missing required counter '{req_counter}'")
        c_val = data[req_counter]
        if not isinstance(c_val, int) or isinstance(c_val, bool) or c_val < 0:
            raise ValueError(f"Invalid UE report: counter '{req_counter}' must be a non-negative integer, got {c_val!r}")

    succeeded = data["succeeded"]
    succeeded_with_warnings = data["succeededWithWarnings"]
    passed = succeeded + succeeded_with_warnings
    failed = data["failed"]
    not_run = data["notRun"]
    in_process = data["inProcess"]
    total = passed + failed + not_run + in_process

    if "totalDuration" not in data:
        raise ValueError("Invalid UE report: missing required field 'totalDuration'")
    tot_dur = data["totalDuration"]
    if not isinstance(tot_dur, (int, float)) or isinstance(tot_dur, bool) or tot_dur < 0.0:
        raise ValueError(f"Invalid UE report: 'totalDuration' must be a non-negative number, got {tot_dur!r}")
    duration = float(tot_dur)

    if run_identity is not None:
        extracted_identity = dict(run_identity)
    else:
        extracted_identity = extract_runtime_identity_from_ue_data(data, report_file_path=file_path)
        if run_id:
            extracted_identity["run_id"] = run_id

    return {
        "schema_version": 1,
        "run_identity": extracted_identity,
        "tests": normalized_tests,
        "total": total,
        "passed": passed,
        "failed": failed,
        "skipped": not_run,
        "in_process": in_process,
        "duration": duration,
    }



# AEP-03: the named set of sentinel values an identity computer can return
# when it could not determine the real value. Filled in for exactly the
# literal strings compute_source_revision/compute_source_diff_hash/
# compute_build_fingerprint/compute_engine_version can produce today — not a
# speculative guess at future shapes. The dynamic "error:..." family (which
# bakes in the caught exception's own text and so cannot be an exact literal)
# is the one prefix this set still needs; SENTINEL_IDENTITY_PREFIXES carries it.
#
# validate_identity_sentinel_registration() below is the gate that keeps this
# set honest: it parses the four computers' own source and fails if any of
# them can return a bare string literal that is neither a member of
# SENTINEL_IDENTITY_VALUES nor covered by SENTINEL_IDENTITY_PREFIXES — so a new
# fallback sentinel added without registering it here fails that gate, rather
# than silently sailing through validate_run's identity check as if it were a
# real value.
SENTINEL_IDENTITY_VALUES = frozenset({
    "unknown_revision",
    "unknown_diff",
    "missing_binaries",
    "unknown_uproject_missing",
    "unknown_engine_association",
})

SENTINEL_IDENTITY_PREFIXES = ("error:",)


def is_sentinel_identity_value(value: str) -> bool:
    """True if `value` is a registered sentinel — exact member or dynamic-error prefix."""
    return value in SENTINEL_IDENTITY_VALUES or any(value.startswith(p) for p in SENTINEL_IDENTITY_PREFIXES)


# The exact functions the registration gate below scans. Order does not
# matter; completeness does — every identity computer belongs here.
IDENTITY_SENTINEL_COMPUTERS: tuple = (
    compute_source_revision,
    compute_source_diff_hash,
    compute_build_fingerprint,
    compute_engine_version,
)


# compute_source_diff_hash's own legitimate success value ("no diff against
# HEAD, no untracked files") is a bare string literal too, exactly like a
# sentinel is — the two are told apart by meaning, not shape, so the scan
# below must be told about this one explicitly rather than mistake it for an
# unregistered fallback. It is not, and must never become, a member of
# SENTINEL_IDENTITY_VALUES: validate_run has to keep accepting it as real.
KNOWN_NON_SENTINEL_LITERAL_RETURNS = frozenset({"clean"})


def extract_literal_string_returns(func: Callable[..., str]) -> List[str]:
    """Extracts every bare string literal one of `func`'s `return` statements can produce.

    A real, successful return in these computers is a *computed* value
    (`res.stdout.strip()`, `association.strip()`, a hex digest) in every case
    but one: `compute_source_diff_hash`'s own `"clean"`, excluded explicitly via
    KNOWN_NON_SENTINEL_LITERAL_RETURNS. Every other literal (or f-string) return
    is, by construction, a fallback sentinel, and this function's result is
    exactly the set validate_identity_sentinel_registration checks for
    registration. For an f-string (e.g. `f"error:{f.name}:{e}"`), only the
    static leading segment is returned (`"error:"`) — the interpolated part is
    the caught exception's own text and cannot be a registered literal.
    """
    source = textwrap.dedent(inspect.getsource(func))
    tree = ast.parse(source)
    literals: List[str] = []
    for node in ast.walk(tree):
        if not isinstance(node, ast.Return) or node.value is None:
            continue
        value = node.value
        if isinstance(value, ast.Constant) and isinstance(value.value, str):
            if value.value not in KNOWN_NON_SENTINEL_LITERAL_RETURNS:
                literals.append(value.value)
        elif isinstance(value, ast.JoinedStr) and value.values:
            first = value.values[0]
            if isinstance(first, ast.Constant) and isinstance(first.value, str):
                literals.append(first.value)
    return literals


def validate_identity_sentinel_registration() -> List[str]:
    """AEP-03 gate: every fallback literal an identity computer can return must be registered.

    Returns a list of diagnostics (empty means the four computers introduce no
    unregistered sentinel). Run as part of the ordinary test suite so a new
    fallback return added later without updating SENTINEL_IDENTITY_VALUES /
    SENTINEL_IDENTITY_PREFIXES fails here, not silently at runtime.
    """
    diagnostics: List[str] = []
    for func in IDENTITY_SENTINEL_COMPUTERS:
        for literal in extract_literal_string_returns(func):
            if not is_sentinel_identity_value(literal):
                diagnostics.append(
                    f"{func.__name__} can return unregistered literal {literal!r}; "
                    f"add it to SENTINEL_IDENTITY_VALUES or SENTINEL_IDENTITY_PREFIXES."
                )
    return diagnostics


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
            elif is_sentinel_identity_value(val):
                diagnostics.append(f"Expected run_identity has rejected incomplete/error value for '{k}': '{val}'.")

    # 3. Validate report structure, schema version, and identity
    if not isinstance(report, dict):
        diagnostics.append(f"Report must be a dict, got {type(report).__name__}.")
        return diagnostics

    if "schema_version" not in report:
        diagnostics.append("Report is missing required 'schema_version'.")
    elif report["schema_version"] not in SUPPORTED_REPORT_SCHEMA_VERSIONS:
        diagnostics.append(f"Report has unsupported schema_version: {report['schema_version']!r}.")

    rep_identity = report.get("run_identity")
    if not isinstance(rep_identity, dict):
        diagnostics.append("Report is missing 'run_identity' dictionary.")
    else:
        for k in REQUIRED_IDENTITY_KEYS:
            act_v = rep_identity.get(k)
            if not act_v or not isinstance(act_v, str) or not act_v.strip():
                diagnostics.append(f"Report run_identity has missing or empty key '{k}'.")
            elif is_sentinel_identity_value(act_v):
                diagnostics.append(f"Report run_identity has rejected incomplete/error value for '{k}': '{act_v}'.")
            elif isinstance(run_identity, dict):
                exp_v = run_identity.get(k)
                if act_v != exp_v:
                    diagnostics.append(
                        f"Run identity mismatch for '{k}': expected '{exp_v}', got '{act_v}'."
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


# AEP-01: evidence bundle — the single artifact carrying validate_run's three
# inputs (expected identity, discovered set, normalized report) by value, so a
# producer and the consumer that checks its work can be different processes on
# different machines. Versioned separately from the report's own
# SUPPORTED_REPORT_SCHEMA_VERSIONS: the bundle wraps a report rather than being
# one, and a version mismatch here must fail before the wrapped report's own
# version is even inspected.
EVIDENCE_BUNDLE_SCHEMA_VERSION = "gv2-acceptance-evidence-bundle-v1"


def build_evidence_bundle(
    discovered: Set[str],
    report: Dict[str, Any],
    run_identity: Dict[str, str],
) -> Dict[str, Any]:
    """Packages validate_run's three inputs into one bundle, by value.

    `discovered` is stored as a sorted list (JSON has no set type); every other
    part is stored as-is. No part is stored by reference to a producer-local
    path — a bundle is self-contained once serialized.
    """
    return {
        "bundle_schema_version": EVIDENCE_BUNDLE_SCHEMA_VERSION,
        "run_identity": dict(run_identity) if isinstance(run_identity, dict) else run_identity,
        "discovered": sorted(discovered) if isinstance(discovered, (set, frozenset)) else discovered,
        "report": report,
    }


def write_evidence_bundle(
    path: Union[str, Path],
    discovered: Set[str],
    report: Dict[str, Any],
    run_identity: Dict[str, str],
) -> None:
    """Writes build_evidence_bundle's result to `path` as JSON."""
    bundle = build_evidence_bundle(discovered, report, run_identity)
    Path(path).write_text(json.dumps(bundle, indent=2, sort_keys=True), encoding="utf-8")


def load_evidence_bundle(path: Union[str, Path]) -> Dict[str, Any]:
    """Reads and schema-checks an evidence bundle from `path`.

    Raises FileNotFoundError if the file does not exist, and ValueError if it
    is not valid JSON, not an object, or carries an unsupported
    bundle_schema_version — a version mismatch is a typed failure here, not an
    attempt to parse a shape the reader was not built for.
    """
    bundle_path = Path(path)
    if not bundle_path.is_file():
        raise FileNotFoundError(f"Evidence bundle not found: {bundle_path}")

    try:
        with open(bundle_path, "r", encoding="utf-8") as f:
            bundle = json.load(f)
    except json.JSONDecodeError as e:
        raise ValueError(f"Evidence bundle at {bundle_path} is not valid JSON: {e}") from e

    if not isinstance(bundle, dict):
        raise ValueError(
            f"Evidence bundle at {bundle_path} must be a JSON object, got {type(bundle).__name__}."
        )

    version = bundle.get("bundle_schema_version")
    if version != EVIDENCE_BUNDLE_SCHEMA_VERSION:
        raise ValueError(
            f"Evidence bundle at {bundle_path} has unsupported bundle_schema_version {version!r}; "
            f"expected {EVIDENCE_BUNDLE_SCHEMA_VERSION!r}."
        )

    return bundle


# AEP-02: the enumerator for "a bundle is complete" — every part a consumer
# needs to run validate_run without substituting a default. Absence of one of
# these keys is a distinct failure from a part that is present but malformed
# or empty; the latter is reported by validate_run's own deeper checks (and,
# for run_identity, by its own REQUIRED_IDENTITY_KEYS enumerator).
REQUIRED_BUNDLE_PARTS = ("run_identity", "discovered", "report")


# AEP-04: REQUIRED_IDENTITY_KEYS split by who is authoritative for a field —
# recorded explicitly here, not inferred from a name prefix. A bundle's own
# "expected" run_identity is producer-supplied; for a remote producer, trusting
# it wholesale would let evidence collected on another revision (or a dirty
# producer tree) validate as if it matched the consumer's own checkout,
# invisibly, because internally self-consistent.
#
# source_revision/source_diff_hash describe the TREE being checked, not the
# run — the consumer can and must derive them from its own checkout rather
# than trust the bundle's claim.
CONSUMER_DERIVED_IDENTITY_KEYS = ("source_revision", "source_diff_hash")
# build_fingerprint/engine_version describe the EXECUTOR that produced the
# report — which binary actually ran. The consumer has no way to derive these
# independently; they stay producer-supplied and are checked only by AEP-03's
# sentinel rule plus validate_run's own report-vs-expected comparison.
EXECUTOR_PROVIDED_IDENTITY_KEYS = ("build_fingerprint", "engine_version")
# run_id is a correlation id, not a property of the tree or the executor; it
# belongs to neither group and is excluded from both on purpose.

assert set(CONSUMER_DERIVED_IDENTITY_KEYS) | set(EXECUTOR_PROVIDED_IDENTITY_KEYS) | {"run_id"} == set(
    REQUIRED_IDENTITY_KEYS
), "Every REQUIRED_IDENTITY_KEYS field must be classified as consumer-derived, executor-provided, or run_id."
assert not set(CONSUMER_DERIVED_IDENTITY_KEYS) & set(EXECUTOR_PROVIDED_IDENTITY_KEYS)


def derive_consumer_identity_overrides(
    repo_root: Optional[Union[str, Path]] = None,
) -> Dict[str, str]:
    """Computes the identity fields the consumer — not the bundle's producer — is authoritative for.

    Only CONSUMER_DERIVED_IDENTITY_KEYS: derived fresh from the consumer's own
    checkout every time this is called, regardless of what a bundle's own
    "expected" run_identity claims for these two fields.
    """
    if repo_root is None:
        resolved_root = Path(__file__).resolve().parent.parent.parent
    else:
        resolved_root = Path(repo_root).resolve()
    return {
        "source_revision": compute_source_revision(resolved_root),
        "source_diff_hash": compute_source_diff_hash(resolved_root),
    }


def validate_evidence_bundle(
    bundle: Dict[str, Any],
    consumer_repo_root: Optional[Union[str, Path]] = None,
) -> List[str]:
    """Validates an evidence bundle produced by build_evidence_bundle/write_evidence_bundle.

    Unpacks the bundle's by-value parts and defers the actual content check to
    validate_run, which stays the single place that decides pass/fail — this
    function additionally proves that a bundle round-tripped through JSON
    validates identically to the in-memory values it was built from, that a
    bundle missing one of REQUIRED_BUNDLE_PARTS entirely fails closed with a
    diagnostic naming the missing part, and (AEP-04) that CONSUMER_DERIVED_IDENTITY_KEYS
    are always taken from THIS process's own checkout — never from the
    bundle's own "expected" run_identity — before validate_run ever compares
    them against the report's own claimed identity.
    """
    if not isinstance(bundle, dict):
        return [f"Evidence bundle must be a dict, got {type(bundle).__name__}."]

    version = bundle.get("bundle_schema_version")
    if version != EVIDENCE_BUNDLE_SCHEMA_VERSION:
        return [
            f"Evidence bundle has unsupported bundle_schema_version {version!r}; "
            f"expected {EVIDENCE_BUNDLE_SCHEMA_VERSION!r}."
        ]

    # No defaults are substituted for an absent part: a missing key stops here
    # with its own named diagnostic, distinct from the same part being present
    # but empty/wrong-typed (which the checks below report separately).
    missing_parts = [part for part in REQUIRED_BUNDLE_PARTS if part not in bundle]
    if missing_parts:
        return [f"Evidence bundle is missing required part '{part}'." for part in missing_parts]

    discovered_raw = bundle.get("discovered")
    discovered: Any = set(discovered_raw) if isinstance(discovered_raw, list) else discovered_raw

    # AEP-04: the bundle's own "expected" run_identity is producer-supplied.
    # source_revision/source_diff_hash are overridden with values this process
    # derives from its own checkout — a bundle collected on another revision,
    # or a dirty producer tree, is caught here because what gets compared
    # against the report's claimed identity is no longer self-attested.
    bundle_identity = bundle.get("run_identity")
    if isinstance(bundle_identity, dict):
        expected_identity: Any = dict(bundle_identity)
        expected_identity.update(derive_consumer_identity_overrides(consumer_repo_root))
    else:
        expected_identity = bundle_identity

    return validate_run(discovered, bundle.get("report"), expected_identity)
