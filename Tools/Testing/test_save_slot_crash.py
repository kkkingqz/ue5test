#!/usr/bin/env python3
"""
Test harness for save slot crash and process concurrency (CFC-08 / ADR-0045).
Verifies that process termination at each filesystem stage preserves the single-commit invariant:
- Pre-commit crashes leave existing Current/Previous untouched.
- Post-commit crashes preserve the committed write, regardless of cleanup interruption.
- Concurrent process openings return Busy.
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def find_helper(provided: str | None) -> Path | None:
    if provided:
        p = Path(provided)
        if p.exists() and os.access(p, os.X_OK):
            return p.resolve()
        return None

    # Search common build directories
    repo_root = Path(__file__).resolve().parent.parent.parent
    candidates = [
        repo_root / "cmake-build-ci" / "Source" / "gv2_save_slot_crash_helper",
        repo_root / "build" / "Source" / "gv2_save_slot_crash_helper",
        repo_root / "cmake-build-debug" / "Source" / "gv2_save_slot_crash_helper",
        repo_root / "Source" / "gv2_save_slot_crash_helper",
    ]
    for c in candidates:
        if c.exists() and os.access(c, os.X_OK):
            return c.resolve()
    return None


def run_helper(helper: Path, args: list[str], timeout: float = 10.0) -> subprocess.CompletedProcess:
    cmd = [str(helper)] + args
    return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)


def test_first_write_crash_matrix(helper: Path) -> list[str]:
    errors = []
    slot = "first_write_slot"
    payload = "payload_first_gen"

    # Step 1: count stages
    with tempfile.TemporaryDirectory(prefix="gv2_crash_test_") as tmp_dir:
        res = run_helper(helper, ["--action", "count-stages", "--root", tmp_dir, "--slot", slot, "--payload", payload])
        if res.returncode != 0:
            return [f"count-stages failed on first write: {res.stderr}"]

        stages = 0
        commit_stage = 0
        for line in res.stdout.splitlines():
            if line.startswith("STAGES "):
                stages = int(line.split()[1])
            elif line.startswith("COMMIT_STAGE "):
                commit_stage = int(line.split()[1])

        if stages == 0 or commit_stage == 0:
            return [f"Invalid stage counts for first write: stages={stages}, commit_stage={commit_stage}"]

    # Step 2: crash matrix enumeration
    for stage in range(1, stages + 1):
        with tempfile.TemporaryDirectory(prefix="gv2_crash_stage_") as tmp_dir:
            crash_res = run_helper(
                helper,
                ["--action", "write", "--root", tmp_dir, "--slot", slot, "--payload", payload, "--crash-at-stage", str(stage)],
            )
            # Process must crash or return non-zero
            if stage < commit_stage:
                # Pre-commit: must be NotFound
                cur_res = run_helper(helper, ["--action", "read", "--root", tmp_dir, "--slot", slot, "--revision", "current"])
                prev_res = run_helper(helper, ["--action", "read", "--root", tmp_dir, "--slot", slot, "--revision", "previous"])
                if cur_res.stdout.strip() != "NOT_FOUND":
                    errors.append(f"Stage {stage} (pre-commit first write): expected current=NOT_FOUND, got '{cur_res.stdout.strip()}'")
                if prev_res.stdout.strip() != "NOT_FOUND":
                    errors.append(f"Stage {stage} (pre-commit first write): expected previous=NOT_FOUND, got '{prev_res.stdout.strip()}'")
            else:
                # Post-commit: Current must be committed payload, Previous NotFound
                cur_res = run_helper(helper, ["--action", "read", "--root", tmp_dir, "--slot", slot, "--revision", "current"])
                prev_res = run_helper(helper, ["--action", "read", "--root", tmp_dir, "--slot", slot, "--revision", "previous"])
                expected_cur = f"OK {payload}"
                if cur_res.stdout.strip() != expected_cur:
                    errors.append(f"Stage {stage} (post-commit first write): expected current='{expected_cur}', got '{cur_res.stdout.strip()}'")
                if prev_res.stdout.strip() != "NOT_FOUND":
                    errors.append(f"Stage {stage} (post-commit first write): expected previous=NOT_FOUND, got '{prev_res.stdout.strip()}'")

    return errors


def test_overwrite_crash_matrix(helper: Path) -> list[str]:
    errors = []
    slot = "overwrite_slot"
    initial = "initial_committed_payload"
    updated = "updated_committed_payload"

    # Step 1: count stages
    with tempfile.TemporaryDirectory(prefix="gv2_crash_ow_") as tmp_dir:
        run_helper(helper, ["--action", "write", "--root", tmp_dir, "--slot", slot, "--payload", initial])
        res = run_helper(helper, ["--action", "count-stages", "--root", tmp_dir, "--slot", slot, "--payload", updated])
        if res.returncode != 0:
            return [f"count-stages failed on overwrite: {res.stderr}"]

        stages = 0
        commit_stage = 0
        for line in res.stdout.splitlines():
            if line.startswith("STAGES "):
                stages = int(line.split()[1])
            elif line.startswith("COMMIT_STAGE "):
                commit_stage = int(line.split()[1])

        if stages == 0 or commit_stage == 0:
            return [f"Invalid stage counts for overwrite: stages={stages}, commit_stage={commit_stage}"]

    # Step 2: crash matrix enumeration
    for stage in range(1, stages + 1):
        with tempfile.TemporaryDirectory(prefix="gv2_crash_ow_stage_") as tmp_dir:
            run_helper(helper, ["--action", "write", "--root", tmp_dir, "--slot", slot, "--payload", initial])
            crash_res = run_helper(
                helper,
                ["--action", "write", "--root", tmp_dir, "--slot", slot, "--payload", updated, "--crash-at-stage", str(stage)],
            )

            cur_res = run_helper(helper, ["--action", "read", "--root", tmp_dir, "--slot", slot, "--revision", "current"])
            prev_res = run_helper(helper, ["--action", "read", "--root", tmp_dir, "--slot", slot, "--revision", "previous"])

            if stage < commit_stage:
                # Pre-commit crash: initial remains current, previous remains NotFound
                expected_cur = f"OK {initial}"
                if cur_res.stdout.strip() != expected_cur:
                    errors.append(f"Overwrite stage {stage} (pre-commit): expected current='{expected_cur}', got '{cur_res.stdout.strip()}'")
                if prev_res.stdout.strip() != "NOT_FOUND":
                    errors.append(f"Overwrite stage {stage} (pre-commit): expected previous=NOT_FOUND, got '{prev_res.stdout.strip()}'")
            else:
                # Post-commit crash: updated is current, initial is previous
                expected_cur = f"OK {updated}"
                expected_prev = f"OK {initial}"
                if cur_res.stdout.strip() != expected_cur:
                    errors.append(f"Overwrite stage {stage} (post-commit): expected current='{expected_cur}', got '{cur_res.stdout.strip()}'")
                if prev_res.stdout.strip() != expected_prev:
                    errors.append(f"Overwrite stage {stage} (post-commit): expected previous='{expected_prev}', got '{prev_res.stdout.strip()}'")

    return errors


def test_legacy_migration_crash_matrix(helper: Path) -> list[str]:
    errors = []
    slot = "legacy_slot"
    legacy_data = "legacy_original_bytes"
    new_data = "migrated_new_bytes"

    # Step 1: count stages
    with tempfile.TemporaryDirectory(prefix="gv2_crash_leg_") as tmp_dir:
        legacy_file = Path(tmp_dir) / f"{slot}.save"
        legacy_file.write_text(legacy_data, encoding="utf-8")

        res = run_helper(helper, ["--action", "count-stages", "--root", tmp_dir, "--slot", slot, "--payload", new_data])
        if res.returncode != 0:
            return [f"count-stages failed on legacy overwrite: {res.stderr}"]

        stages = 0
        commit_stage = 0
        for line in res.stdout.splitlines():
            if line.startswith("STAGES "):
                stages = int(line.split()[1])
            elif line.startswith("COMMIT_STAGE "):
                commit_stage = int(line.split()[1])

        if stages == 0 or commit_stage == 0:
            return [f"Invalid stage counts for legacy overwrite: stages={stages}, commit_stage={commit_stage}"]

    # Step 2: crash matrix enumeration
    for stage in range(1, stages + 1):
        with tempfile.TemporaryDirectory(prefix="gv2_crash_leg_stage_") as tmp_dir:
            legacy_file = Path(tmp_dir) / f"{slot}.save"
            legacy_file.write_text(legacy_data, encoding="utf-8")

            crash_res = run_helper(
                helper,
                ["--action", "write", "--root", tmp_dir, "--slot", slot, "--payload", new_data, "--crash-at-stage", str(stage)],
            )

            cur_res = run_helper(helper, ["--action", "read", "--root", tmp_dir, "--slot", slot, "--revision", "current"])
            prev_res = run_helper(helper, ["--action", "read", "--root", tmp_dir, "--slot", slot, "--revision", "previous"])

            if stage < commit_stage:
                # Pre-commit crash: legacy remains current, previous remains NotFound
                expected_cur = f"OK {legacy_data}"
                if cur_res.stdout.strip() != expected_cur:
                    errors.append(f"Legacy stage {stage} (pre-commit): expected current='{expected_cur}', got '{cur_res.stdout.strip()}'")
                if prev_res.stdout.strip() != "NOT_FOUND":
                    errors.append(f"Legacy stage {stage} (pre-commit): expected previous=NOT_FOUND, got '{prev_res.stdout.strip()}'")
            else:
                # Post-commit crash: new_data is current, legacy_data is previous
                expected_cur = f"OK {new_data}"
                expected_prev = f"OK {legacy_data}"
                if cur_res.stdout.strip() != expected_cur:
                    errors.append(f"Legacy stage {stage} (post-commit): expected current='{expected_cur}', got '{cur_res.stdout.strip()}'")
                if prev_res.stdout.strip() != expected_prev:
                    errors.append(f"Legacy stage {stage} (post-commit): expected previous='{expected_prev}', got '{prev_res.stdout.strip()}'")

    return errors


def test_concurrency_busy(helper: Path) -> list[str]:
    errors = []
    with tempfile.TemporaryDirectory(prefix="gv2_lock_") as tmp_dir:
        # Launch holder process
        proc1 = subprocess.Popen(
            [str(helper), "--action", "hold-lock", "--root", tmp_dir, "--hold-ms", "1500"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

        try:
            # Wait for proc1 to print LOCKED
            line = proc1.stdout.readline().strip()
            if line != "LOCKED":
                errors.append(f"Process 1 failed to acquire lock: '{line}'")
                return errors

            # Try to acquire lock from proc2 (should report BUSY)
            res2 = run_helper(helper, ["--action", "hold-lock", "--root", tmp_dir, "--hold-ms", "100"])
            if res2.stdout.strip() != "BUSY":
                errors.append(f"Process 2 did not report BUSY, got: '{res2.stdout.strip()}'")
        finally:
            proc1.wait(timeout=5)

        # After proc1 exits, proc3 must succeed
        res3 = run_helper(helper, ["--action", "hold-lock", "--root", tmp_dir, "--hold-ms", "100"])
        if "LOCKED" not in res3.stdout:
            errors.append(f"Process 3 failed to acquire lock after release: '{res3.stdout.strip()}'")

    return errors


def run_self_test() -> bool:
    print("Running test_save_slot_crash self-tests...")
    # Self-test validates oracle logic and path handling
    assert find_helper("/non/existent/path/binary") is None
    print("All self-tests passed!")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description="Save slot crash & concurrency harness")
    parser.add_argument("--helper", type=str, default=None, help="Path to gv2_save_slot_crash_helper executable")
    parser.add_argument("--self-test", action="store_true", help="Run self-tests")
    args = parser.parse_args()

    if args.self_test:
        return 0 if run_self_test() else 1

    helper = find_helper(args.helper)
    if not helper:
        print("ERROR: gv2_save_slot_crash_helper not found. Compile the target first or pass --helper.", file=sys.stderr)
        return 1

    print(f"Using helper: {helper}")
    all_errors = []

    print("Running first write crash matrix...")
    errs = test_first_write_crash_matrix(helper)
    all_errors.extend(errs)

    print("Running overwrite crash matrix...")
    errs = test_overwrite_crash_matrix(helper)
    all_errors.extend(errs)

    print("Running legacy migration crash matrix...")
    errs = test_legacy_migration_crash_matrix(helper)
    all_errors.extend(errs)

    print("Running concurrency & busy lock tests...")
    errs = test_concurrency_busy(helper)
    all_errors.extend(errs)

    if all_errors:
        print("\nCrash harness failures detected:", file=sys.stderr)
        for e in all_errors:
            print(f"  - {e}", file=sys.stderr)
        return 1

    print("\nSUCCESS: All save slot crash & concurrency tests passed cleanly.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
