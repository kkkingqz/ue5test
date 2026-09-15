#!/usr/bin/env python3
"""Verify the checked-in production-path session operation measurement.

The observation is produced by UE automation test
GV2.Runtime.Session.OperationProfileMeasurement. This script is deliberately
read-only: it validates the independent golden and its derivation into the C++
and lifecycle-contract constants, but never fabricates or rewrites measurements.
"""

from __future__ import annotations

import argparse
import copy
import json
import re
import sys
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
TRANSITION_HEADER = REPO_ROOT / "Source" / "GV2" / "Private" / "Application" / "GV2SessionTransition.h"
LIFECYCLE_DOC = REPO_ROOT / "Docs" / "Architecture" / "BootstrapAndSessionLifecycle.md"
PROFILE_PATH = REPO_ROOT / "Docs" / "Status" / "Measurements" / "session_operation_profile.json"

EXPECTED_SCHEMA = "2.0.0"
EXPECTED_KIND = "production_public_api_trace"
EXPECTED_TEST = "GV2.Runtime.Session.OperationProfileMeasurement"
EXPECTED_SCENARIO = "session_lifecycle_v1"
RETENTION_WINDOWS_POLICY = 3


def parse_named_constant(path: Path, pattern: str, label: str) -> int:
    text = path.read_text(encoding="utf-8")
    match = re.search(pattern, text)
    if match is None:
        raise ValueError(f"Could not find {label} in {path}")
    return int(match.group(1))


def validate_profile(profile: dict[str, Any], header_limit: int, doc_limit: int) -> None:
    if profile.get("schema_version") != EXPECTED_SCHEMA:
        raise ValueError(f"schema_version must be {EXPECTED_SCHEMA}")
    if profile.get("measurement_kind") != EXPECTED_KIND:
        raise ValueError("measurement_kind must identify the production public API trace")
    if profile.get("source_test") != EXPECTED_TEST:
        raise ValueError("source_test must name the UE production-path measurement test")
    if profile.get("scenario_id") != EXPECTED_SCENARIO:
        raise ValueError("scenario_id must identify the reviewed lifecycle scenario")

    observed = profile.get("observed")
    if not isinstance(observed, dict):
        raise ValueError("observed must be an object")

    integer_fields = (
        "first_operation_id",
        "last_operation_id",
        "terminal_operations",
        "completed",
        "failed",
        "cancelled",
        "superseded",
    )
    for field in integer_fields:
        value = observed.get(field)
        if not isinstance(value, int) or isinstance(value, bool) or value < 0:
            raise ValueError(f"observed.{field} must be a non-negative integer")

    first_id = observed["first_operation_id"]
    last_id = observed["last_operation_id"]
    terminal = observed["terminal_operations"]
    if first_id != 1 or last_id < first_id:
        raise ValueError("the isolated scenario must enumerate its operation-id domain from 1")
    if terminal != last_id - first_id + 1:
        raise ValueError("terminal_operations must enumerate every allocated scenario operation")
    classified = sum(observed[name] for name in ("completed", "failed", "cancelled", "superseded"))
    if terminal != classified:
        raise ValueError("terminal_operations must equal the sum of terminal outcome classes")

    windows = profile.get("retention_windows")
    if windows != RETENTION_WINDOWS_POLICY:
        raise ValueError(f"retention_windows must equal policy value {RETENTION_WINDOWS_POLICY}")
    derived_limit = profile.get("derived_default_max_retained_outcomes")
    if derived_limit != terminal * windows:
        raise ValueError("derived limit must equal observed terminal operations times retention windows")
    if header_limit != derived_limit:
        raise ValueError(f"C++ default {header_limit} differs from measured derived limit {derived_limit}")
    if doc_limit != derived_limit:
        raise ValueError(f"contract default {doc_limit} differs from measured derived limit {derived_limit}")


def sample_profile() -> dict[str, Any]:
    return {
        "schema_version": EXPECTED_SCHEMA,
        "measurement_kind": EXPECTED_KIND,
        "source_test": EXPECTED_TEST,
        "scenario_id": EXPECTED_SCENARIO,
        "observed": {
            "first_operation_id": 1,
            "last_operation_id": 6,
            "terminal_operations": 6,
            "completed": 3,
            "failed": 3,
            "cancelled": 0,
            "superseded": 0,
        },
        "retention_windows": RETENTION_WINDOWS_POLICY,
        "derived_default_max_retained_outcomes": 18,
    }


def expect_rejected(profile: dict[str, Any], header_limit: int = 18, doc_limit: int = 18) -> None:
    try:
        validate_profile(profile, header_limit, doc_limit)
    except ValueError:
        return
    raise AssertionError("invalid profile was accepted")


def run_self_tests() -> None:
    profile = sample_profile()
    validate_profile(profile, 18, 18)

    mutation = copy.deepcopy(profile)
    mutation["measurement_kind"] = "modeled_constants"
    expect_rejected(mutation)

    mutation = copy.deepcopy(profile)
    mutation["observed"]["terminal_operations"] = 5
    expect_rejected(mutation)

    mutation = copy.deepcopy(profile)
    mutation["observed"]["completed"] = 2
    expect_rejected(mutation)

    mutation = copy.deepcopy(profile)
    mutation["derived_default_max_retained_outcomes"] = 160
    expect_rejected(mutation)

    expect_rejected(profile, header_limit=160)
    expect_rejected(profile, doc_limit=160)
    print("PASS: session operation profile negative self-tests")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="verify the checked-in profile without writing")
    parser.add_argument("--self-test", action="store_true", help="run negative verifier tests")
    args = parser.parse_args()

    if args.self_test:
        run_self_tests()
        return 0
    if not args.check:
        parser.error("--check is required; measurement artifacts are never generated by this verifier")

    try:
        profile = json.loads(PROFILE_PATH.read_text(encoding="utf-8"))
        header_limit = parse_named_constant(
            TRANSITION_HEADER,
            r"static\s+constexpr\s+int32\s+DefaultMaxRetainedOutcomes\s*=\s*(\d+)\s*;",
            "DefaultMaxRetainedOutcomes",
        )
        doc_limit = parse_named_constant(
            LIFECYCLE_DOC,
            r"DefaultMaxRetainedOutcomes\s*=\s*`?(\d+)`?",
            "documented DefaultMaxRetainedOutcomes",
        )
        validate_profile(profile, header_limit, doc_limit)
    except (OSError, json.JSONDecodeError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1

    observed = profile["observed"]
    print(
        "PASS: checked-in UE production trace "
        f"observed {observed['terminal_operations']} terminal operations; "
        f"{profile['retention_windows']} windows derive limit "
        f"{profile['derived_default_max_retained_outcomes']}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
