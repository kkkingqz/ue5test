#!/usr/bin/env python3
"""Ratchet gate validating test suite content coupling boundaries and file size limits.

Implements TSR-02 (ADR-0046, Plan TestSuiteRestructuring):
  1. Enforces that test files in Source/**/Tests do not introduce any new content
     couplings to game packages (Class 1 namespaced IDs or Class 2 game content roots).
     Couplings are strictly ratcheted against Tools/Testing/test_content_coupling_baseline.json.
  2. Enforces that the baseline cannot grow or contain phantom entries. When a coupling
     is removed from code, the baseline must be ratcheted down in the same change set.
  3. Enforces a file size ceiling (max_file_lines) for all test files, preventing the
     creation of new oversized monoliths during test suite restructuring.
  4. Requires explicit justifications for designated smoke files and file size exceptions.
"""

from __future__ import annotations

import argparse
import json
import sys
import tempfile
from collections import Counter
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple

# Re-use the authoritative inventory extractor from TSR-01
from inventory_test_content_coupling import (
    CouplingOccurrence,
    InventoryReport,
    SCANNED_EXTENSIONS,
    discover_test_files,
    scan_inventory,
)


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
DEFAULT_BASELINE_PATH = REPO_ROOT / "Tools" / "Testing" / "test_content_coupling_baseline.json"


def load_baseline(baseline_path: Path) -> Dict[str, Any]:
    """Load and validate the test content coupling baseline JSON."""
    if not baseline_path.exists():
        raise FileNotFoundError(f"Baseline file not found: {baseline_path}")

    content = baseline_path.read_text(encoding="utf-8")
    data = json.loads(content)

    if not isinstance(data, dict):
        raise ValueError(f"Baseline root must be a JSON object: {baseline_path}")

    if "max_file_lines" not in data or not isinstance(data["max_file_lines"], int):
        raise ValueError(f"Baseline missing integer 'max_file_lines': {baseline_path}")

    if "allowed_couplings" not in data or not isinstance(data["allowed_couplings"], list):
        raise ValueError(f"Baseline missing list 'allowed_couplings': {baseline_path}")

    return data


def validate_file_sizes(
    repo_root: Path, baseline: Dict[str, Any]
) -> List[str]:
    """Validate that test files do not exceed the baseline ceiling unless explicitly excepted."""
    max_lines = baseline["max_file_lines"]
    exceptions_list = baseline.get("max_file_lines_exceptions", [])
    
    # Map exception relative path to justification reason
    exceptions: Dict[str, str] = {}
    for exc in exceptions_list:
        if isinstance(exc, dict) and "file" in exc and "reason" in exc:
            f_path = exc["file"]
            reason = exc["reason"]
            if not reason or not reason.strip():
                return [f"Baseline error: exception for {f_path} has empty reason."]
            exceptions[f_path] = reason

    test_files = discover_test_files(repo_root)
    violations: List[str] = []

    for path in test_files:
        rel_path = path.relative_to(repo_root).as_posix()
        try:
            line_count = len(path.read_text(encoding="utf-8").splitlines())
        except (UnicodeDecodeError, OSError):
            continue

        if line_count > max_lines:
            if rel_path not in exceptions:
                violations.append(
                    f"{rel_path}: error [TEST_FILE_SIZE_CEILING]: File has {line_count} lines, "
                    f"exceeding the baseline ceiling of {max_lines} lines without an explicit exception."
                )

    return violations


def validate_couplings(
    repo_root: Path, baseline: Dict[str, Any]
) -> List[str]:
    """Validate that current content couplings strictly match baseline without additions or growth."""
    report = scan_inventory(repo_root)
    violations: List[str] = []

    # Current occurrences of Class 1 and Class 2
    current_occurrences = [
        o for o in report.occurrences if o.category in ("class_1", "class_2")
    ]

    current_counts: Counter[Tuple[str, str, str]] = Counter(
        (o.category, o.file_path, o.token) for o in current_occurrences
    )

    # Baseline couplings
    baseline_list = baseline.get("allowed_couplings", [])
    baseline_counts: Counter[Tuple[str, str, str]] = Counter()

    for item in baseline_list:
        cat = item.get("category", "")
        f_path = item.get("file", "")
        tok = item.get("token", "")
        if not cat or not f_path or not tok:
            violations.append(f"Baseline error: malformed allowed_coupling entry: {item}")
            continue
        baseline_counts[(cat, f_path, tok)] += 1

    # Check 1: Any new coupling in code that is not in baseline (or count increased)
    for occ in current_occurrences:
        key = (occ.category, occ.file_path, occ.token)
        curr_num = current_counts[key]
        base_num = baseline_counts.get(key, 0)
        if curr_num > base_num:
            violations.append(
                f"{occ.file_path}:{occ.line_number}: error [TEST_CONTENT_COUPLING_RULE]: "
                f"New content coupling detected in test code: '{occ.token}' ({occ.category}, {occ.matched_detail}). "
                f"Count in code ({curr_num}) exceeds baseline ({base_num}). (ADR-0046)"
            )

    # Check 2: Ratchet check -- baseline must not contain phantom couplings or grown count
    for key, base_num in baseline_counts.items():
        curr_num = current_counts.get(key, 0)
        if base_num > curr_num:
            cat, f_path, tok = key
            violations.append(
                f"error [TEST_CONTENT_COUPLING_RATCHET]: Baseline contains coupling not present in code "
                f"or removed without updating baseline: '{tok}' ({cat}) in {f_path}. "
                f"Baseline count ({base_num}) exceeds code count ({curr_num}). Baseline must be ratcheted down. (ADR-0046)"
            )

    # Check 3: Smoke files justification check
    smoke_files = baseline.get("smoke_files", [])
    for sf in smoke_files:
        if not isinstance(sf, dict) or "file" not in sf or "reason" not in sf:
            violations.append(f"Baseline error: malformed smoke_files entry: {sf}")
            continue
        if not sf["reason"] or not sf["reason"].strip():
            violations.append(f"Baseline error: smoke_file '{sf.get('file')}' has empty reason.")

    return violations


def validate_repository(
    repo_root: Path, baseline_path: Path
) -> List[str]:
    """Perform full test suite content coupling and line limit validation."""
    violations: List[str] = []

    try:
        baseline = load_baseline(baseline_path)
    except Exception as e:
        return [f"Failed to load baseline: {e}"]

    violations.extend(validate_file_sizes(repo_root, baseline))
    violations.extend(validate_couplings(repo_root, baseline))
    return sorted(violations)


def run_self_tests() -> bool:
    """Run comprehensive negative and positive self-tests for TSR-02 ratchet gate."""
    print("[*] Running validate_test_content_coupling self-tests...")

    # 1. Positive check: current repository must pass with current baseline
    actual_violations = validate_repository(REPO_ROOT, DEFAULT_BASELINE_PATH)
    if actual_violations:
        print("FAILED: actual repository has violations against baseline:\n" + "\n".join(actual_violations))
        return False
    print("  [+] Positive test: actual repository matches baseline with 0 violations.")

    with tempfile.TemporaryDirectory() as tmpdir:
        fake_root = Path(tmpdir)

        # Create package manifests
        core_dir = fake_root / "GameData" / "core"
        core_dir.mkdir(parents=True)
        (core_dir / "package.json5").write_text(
            '{\n  package_id: "core",\n  namespace: "core",\n  ue_content_roots: ["/Game/core", "/Game/UI"],\n}\n',
            encoding="utf-8",
        )

        game_dir = fake_root / "GameData" / "alpha"
        game_dir.mkdir(parents=True)
        (game_dir / "package.json5").write_text(
            '{\n  package_id: "alpha",\n  namespace: "alpha",\n  ue_content_roots: ["/Game/Alpha"],\n}\n',
            encoding="utf-8",
        )

        test_dir = fake_root / "Source" / "Module" / "Private" / "Tests"
        test_dir.mkdir(parents=True)

        # File 1: Contract file with 1 allowed coupling
        test_file1 = test_dir / "TestAlphaContract.cpp"
        test_file1.write_text(
            '// Contract test\n'
            'const char* id = "alpha:screen.allowed";\n',
            encoding="utf-8",
        )

        # Baseline JSON
        fake_baseline_path = fake_root / "baseline.json"
        baseline_data = {
            "version": 1,
            "max_file_lines": 50,
            "max_file_lines_exceptions": [
                {
                    "file": "Source/Module/Private/Tests/TestLargeException.cpp",
                    "reason": "Temporary monolith exception"
                }
            ],
            "smoke_files": [
                {
                    "file": "Source/Module/Private/Tests/TestSmoke.cpp",
                    "reason": "Legitimate smoke test"
                }
            ],
            "allowed_couplings": [
                {
                    "category": "class_1",
                    "file": "Source/Module/Private/Tests/TestAlphaContract.cpp",
                    "line": 2,
                    "token": "alpha:screen.allowed",
                }
            ],
        }
        fake_baseline_path.write_text(json.dumps(baseline_data, indent=2), encoding="utf-8")

        # Verify initial clean state in synthetic env
        init_violations = validate_repository(fake_root, fake_baseline_path)
        if init_violations:
            print(f"FAILED: synthetic clean state had violations: {init_violations}")
            return False
        print("  [+] Synthetic baseline matching verified.")

        # 2. Negative test: new coupling added to contract file (not in baseline)
        test_file1.write_text(
            '// Contract test\n'
            'const char* id = "alpha:screen.allowed";\n'
            'const char* new_bad = "alpha:screen.new_forbidden";\n',
            encoding="utf-8",
        )
        new_coupling_violations = validate_repository(fake_root, fake_baseline_path)
        if not any("alpha:screen.new_forbidden" in v for v in new_coupling_violations):
            print(f"FAILED: gate did not reject new coupling added to contract file: {new_coupling_violations}")
            return False
        print("  [+] Negative self-test: gate rejects new coupling in contract file.")

        # Revert file 1
        test_file1.write_text(
            '// Contract test\n'
            'const char* id = "alpha:screen.allowed";\n',
            encoding="utf-8",
        )

        # 3. Negative test: baseline grown with phantom coupling (not in code)
        grown_baseline = dict(baseline_data)
        grown_baseline["allowed_couplings"] = list(baseline_data["allowed_couplings"]) + [
            {
                "category": "class_1",
                "file": "Source/Module/Private/Tests/TestAlphaContract.cpp",
                "line": 3,
                "token": "alpha:screen.phantom_entry",
            }
        ]
        fake_baseline_path.write_text(json.dumps(grown_baseline, indent=2), encoding="utf-8")
        grown_violations = validate_repository(fake_root, fake_baseline_path)
        if not any("alpha:screen.phantom_entry" in v and "TEST_CONTENT_COUPLING_RATCHET" in v for v in grown_violations):
            print(f"FAILED: gate did not reject grown/phantom baseline: {grown_violations}")
            return False
        print("  [+] Negative self-test: gate rejects grown baseline / phantom couplings.")

        # Restore baseline
        fake_baseline_path.write_text(json.dumps(baseline_data, indent=2), encoding="utf-8")

        # 4. Negative test: file size exceeds ceiling without exception
        oversized_file = test_dir / "TestOversized.cpp"
        oversized_file.write_text("\n" * 60, encoding="utf-8")  # 61 lines > 50 ceiling
        size_violations = validate_repository(fake_root, fake_baseline_path)
        if not any("TestOversized.cpp" in v and "TEST_FILE_SIZE_CEILING" in v for v in size_violations):
            print(f"FAILED: gate did not reject oversized file: {size_violations}")
            return False
        print("  [+] Negative self-test: gate rejects file exceeding max_file_lines ceiling.")

        # 5. File size with exception is accepted
        oversized_file.unlink()
        excepted_file = test_dir / "TestLargeException.cpp"
        excepted_file.write_text("\n" * 60, encoding="utf-8")
        excepted_violations = validate_repository(fake_root, fake_baseline_path)
        if excepted_violations:
            print(f"FAILED: excepted oversized file produced violations: {excepted_violations}")
            return False
        print("  [+] File size exception in baseline is honored.")

    print("ALL RATCHET GATE SELF-TESTS PASSED SUCCESSFULLY!")
    return True


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="Validate test suite content coupling against baseline ratchet (TSR-02 / ADR-0046)."
    )
    parser.add_argument(
        "--self-test", action="store_true", help="Run self-tests and negative validation tests."
    )
    parser.add_argument(
        "--baseline", type=Path, default=DEFAULT_BASELINE_PATH, help="Path to baseline JSON file."
    )
    parser.add_argument(
        "--repo-root", type=Path, default=REPO_ROOT, help="Override repository root path."
    )

    args = parser.parse_args(argv)

    if args.self_test:
        return 0 if run_self_tests() else 1

    violations = validate_repository(args.repo_root, args.baseline)

    if violations:
        print(
            f"Test content coupling validation FAILED ({len(violations)} violation(s)):",
            file=sys.stderr,
        )
        for v in violations:
            print(f"  {v}", file=sys.stderr)
        return 1

    print("SUCCESS: test content coupling validation passed against baseline ratchet.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
