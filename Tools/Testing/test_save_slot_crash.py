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


# ADR-0045 D2 & D5 independent contract specifications for filesystem operations
FIRST_WRITE_CONTRACT_STAGES = [
    (1, "check_head_exists"),
    (2, "check_legacy_exists"),
    (3, "write_new_temp_generation"),
    (4, "commit_new_generation"),
    (5, "write_temp_head"),
    (6, "commit_head"),  # Commit point
    (7, "cleanup_list_root"),
]
FIRST_WRITE_COMMIT_STAGE = 6

OVERWRITE_CONTRACT_STAGES = [
    (1, "check_head_exists"),
    (2, "check_head_is_regular"),
    (3, "write_read_head"),
    (4, "write_new_temp_generation"),
    (5, "commit_new_generation"),
    (6, "write_temp_head"),
    (7, "commit_head"),  # Commit point
    (8, "cleanup_list_root"),
]
OVERWRITE_COMMIT_STAGE = 7

LEGACY_MIGRATION_CONTRACT_STAGES = [
    (1, "check_head_exists"),
    (2, "check_legacy_exists"),
    (3, "check_legacy_is_regular"),
    (4, "read_legacy_for_migration"),
    (5, "write_legacy_temp_generation"),
    (6, "commit_legacy_generation"),
    (7, "write_new_temp_generation"),
    (8, "commit_new_generation"),
    (9, "write_temp_head"),
    (10, "commit_head"),  # Commit point
    (11, "cleanup_migrated_legacy_file"),
    (12, "cleanup_list_root"),
]
LEGACY_MIGRATION_COMMIT_STAGE = 10


def test_first_write_crash_matrix(helper: Path) -> list[str]:
    errors = []
    slot = "first_write_slot"
    payload = "payload_first_gen"

    # Step 1: count stages and verify trace matches contract
    with tempfile.TemporaryDirectory(prefix="gv2_crash_test_") as tmp_dir:
        res = run_helper(helper, ["--action", "count-stages", "--root", tmp_dir, "--slot", slot, "--payload", payload])
        if res.returncode != 0:
            return [f"count-stages failed on first write: {res.stderr}"]

        stages = 0
        actual_stages = []
        for line in res.stdout.splitlines():
            if line.startswith("STAGES "):
                stages = int(line.split()[1])
            elif line.startswith("STAGE "):
                parts = line.split(maxsplit=2)
                if len(parts) == 3:
                    actual_stages.append((int(parts[1]), parts[2]))

        if stages != len(FIRST_WRITE_CONTRACT_STAGES):
            return [f"Stage count mismatch on first write: expected {len(FIRST_WRITE_CONTRACT_STAGES)}, got {stages}"]

        for (exp_ord, exp_desc), (act_ord, act_desc) in zip(FIRST_WRITE_CONTRACT_STAGES, actual_stages):
            if exp_ord != act_ord or exp_desc != act_desc:
                return [f"Contract stage mismatch on first write: expected ({exp_ord}, {exp_desc}), got ({act_ord}, {act_desc})"]

    # Step 2: crash matrix enumeration across contract stages
    for stage in range(1, len(FIRST_WRITE_CONTRACT_STAGES) + 1):
        with tempfile.TemporaryDirectory(prefix="gv2_crash_stage_") as tmp_dir:
            crash_res = run_helper(
                helper,
                ["--action", "write", "--root", tmp_dir, "--slot", slot, "--payload", payload, "--crash-at-stage", str(stage)],
            )
            # Process must crash or return non-zero
            if stage < FIRST_WRITE_COMMIT_STAGE:
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

    # Step 1: count stages and verify trace matches contract
    with tempfile.TemporaryDirectory(prefix="gv2_crash_ow_") as tmp_dir:
        run_helper(helper, ["--action", "write", "--root", tmp_dir, "--slot", slot, "--payload", initial])
        res = run_helper(helper, ["--action", "count-stages", "--root", tmp_dir, "--slot", slot, "--payload", updated])
        if res.returncode != 0:
            return [f"count-stages failed on overwrite: {res.stderr}"]

        stages = 0
        actual_stages = []
        for line in res.stdout.splitlines():
            if line.startswith("STAGES "):
                stages = int(line.split()[1])
            elif line.startswith("STAGE "):
                parts = line.split(maxsplit=2)
                if len(parts) == 3:
                    actual_stages.append((int(parts[1]), parts[2]))

        if stages != len(OVERWRITE_CONTRACT_STAGES):
            return [f"Stage count mismatch on overwrite: expected {len(OVERWRITE_CONTRACT_STAGES)}, got {stages}"]

        for (exp_ord, exp_desc), (act_ord, act_desc) in zip(OVERWRITE_CONTRACT_STAGES, actual_stages):
            if exp_ord != act_ord or exp_desc != act_desc:
                return [f"Contract stage mismatch on overwrite: expected ({exp_ord}, {exp_desc}), got ({act_ord}, {act_desc})"]

    # Step 2: crash matrix enumeration across contract stages
    for stage in range(1, len(OVERWRITE_CONTRACT_STAGES) + 1):
        with tempfile.TemporaryDirectory(prefix="gv2_crash_ow_stage_") as tmp_dir:
            run_helper(helper, ["--action", "write", "--root", tmp_dir, "--slot", slot, "--payload", initial])
            crash_res = run_helper(
                helper,
                ["--action", "write", "--root", tmp_dir, "--slot", slot, "--payload", updated, "--crash-at-stage", str(stage)],
            )

            cur_res = run_helper(helper, ["--action", "read", "--root", tmp_dir, "--slot", slot, "--revision", "current"])
            prev_res = run_helper(helper, ["--action", "read", "--root", tmp_dir, "--slot", slot, "--revision", "previous"])

            if stage < OVERWRITE_COMMIT_STAGE:
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

    # Step 1: count stages and verify trace matches contract
    with tempfile.TemporaryDirectory(prefix="gv2_crash_leg_") as tmp_dir:
        legacy_file = Path(tmp_dir) / f"{slot}.save"
        legacy_file.write_text(legacy_data, encoding="utf-8")

        res = run_helper(helper, ["--action", "count-stages", "--root", tmp_dir, "--slot", slot, "--payload", new_data])
        if res.returncode != 0:
            return [f"count-stages failed on legacy overwrite: {res.stderr}"]

        stages = 0
        actual_stages = []
        for line in res.stdout.splitlines():
            if line.startswith("STAGES "):
                stages = int(line.split()[1])
            elif line.startswith("STAGE "):
                parts = line.split(maxsplit=2)
                if len(parts) == 3:
                    actual_stages.append((int(parts[1]), parts[2]))

        if stages != len(LEGACY_MIGRATION_CONTRACT_STAGES):
            return [f"Stage count mismatch on legacy overwrite: expected {len(LEGACY_MIGRATION_CONTRACT_STAGES)}, got {stages}"]

        for (exp_ord, exp_desc), (act_ord, act_desc) in zip(LEGACY_MIGRATION_CONTRACT_STAGES, actual_stages):
            if exp_ord != act_ord or exp_desc != act_desc:
                return [f"Contract stage mismatch on legacy overwrite: expected ({exp_ord}, {exp_desc}), got ({act_ord}, {act_desc})"]

    # Step 2: crash matrix enumeration across contract stages
    for stage in range(1, len(LEGACY_MIGRATION_CONTRACT_STAGES) + 1):
        with tempfile.TemporaryDirectory(prefix="gv2_crash_leg_stage_") as tmp_dir:
            legacy_file = Path(tmp_dir) / f"{slot}.save"
            legacy_file.write_text(legacy_data, encoding="utf-8")

            crash_res = run_helper(
                helper,
                ["--action", "write", "--root", tmp_dir, "--slot", slot, "--payload", new_data, "--crash-at-stage", str(stage)],
            )

            cur_res = run_helper(helper, ["--action", "read", "--root", tmp_dir, "--slot", slot, "--revision", "current"])
            prev_res = run_helper(helper, ["--action", "read", "--root", tmp_dir, "--slot", slot, "--revision", "previous"])

            if stage < LEGACY_MIGRATION_COMMIT_STAGE:
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

    # Validate contract tables integrity and commit points
    assert len(FIRST_WRITE_CONTRACT_STAGES) == 7
    assert FIRST_WRITE_COMMIT_STAGE == 6
    assert FIRST_WRITE_CONTRACT_STAGES[FIRST_WRITE_COMMIT_STAGE - 1] == (6, "commit_head")

    assert len(OVERWRITE_CONTRACT_STAGES) == 8
    assert OVERWRITE_COMMIT_STAGE == 7
    assert OVERWRITE_CONTRACT_STAGES[OVERWRITE_COMMIT_STAGE - 1] == (7, "commit_head")

    assert len(LEGACY_MIGRATION_CONTRACT_STAGES) == 12
    assert LEGACY_MIGRATION_COMMIT_STAGE == 10
    assert LEGACY_MIGRATION_CONTRACT_STAGES[LEGACY_MIGRATION_COMMIT_STAGE - 1] == (10, "commit_head")

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
