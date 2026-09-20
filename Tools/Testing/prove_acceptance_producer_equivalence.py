#!/usr/bin/env python3
"""AEP-06: prove two independent acceptance producers give equivalent evidence.

Portability of evidence (ADR/ContainerizedExecutionEnvironmentProposal decision B) is a
universal statement — "any producer executing the normative argv counts as GV2
acceptance" — and a universal statement requires an enumerator, not reasoning
about the argv being "the same". The enumerator here is two actual bundles:

  Producer A — the local runner: Tools/Testing/run_ue_acceptance.py's own
  run_acceptance(), called unmodified.

  Producer B — this script's own control flow: builds the normative argv via
  build_acceptance_argv() (AEP-05) directly, launches UnrealEditor-Cmd itself
  (its own subprocess.run, its own report-directory bookkeeping, its own log
  parsing), and assembles its own bundle from its own artifacts. It does NOT
  call run_acceptance() or any of run_ue_acceptance.py's orchestration —
  collecting bundle B by calling the local runner with a different flag would
  make it one producer wearing two hats, not two producers.

  Both producers share the same underlying building blocks (the argv
  template, the identity computer, the log parser, the report normalizer,
  the bundle writer/reader/validator) — sharing those is the point: they are
  exactly the normative surface a real remote producer (a ue-build-service)
  would also have to reproduce to be recognizable as GV2 acceptance at all.
  What must NOT be shared is run_ue_acceptance.py's own top-level orchestration
  (run_acceptance() itself), and it is not.

Compares: automation test id SETS (not counts) and build_fingerprint and
engine_version between the two bundles. run_id is excluded from the
comparison by construction — each producer's run_id is its own
execution-correlation id, not expected to match another producer's.

Usage:
    python3 Tools/Testing/prove_acceptance_producer_equivalence.py
    python3 Tools/Testing/prove_acceptance_producer_equivalence.py --filter "GV2.UI"
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time
import uuid
from pathlib import Path
from typing import Any, Dict, List, Optional

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from Tools.Testing.run_ue_acceptance import (
    DEFAULT_UE_ROOT,
    build_acceptance_argv,
    parse_discovery_from_log,
    run_acceptance,
)
from Tools.Testing.ue_test_report import (
    compute_run_identity,
    load_evidence_bundle,
    normalize_ue_json_report,
    validate_evidence_bundle,
    write_evidence_bundle,
)


# AEP-06: run_id is each producer's own execution-correlation id and is
# excluded from the equivalence comparison by construction — recorded here,
# not left to be inferred from the comparison code's own omissions.
EQUIVALENCE_EXCLUDED_IDENTITY_KEYS = ("run_id",)
# These two fields describe the executor (which binary actually ran) and MUST
# agree for two producers to count as equivalent — the same revision built by
# a different engine, or a stale binary, is exactly the divergence this
# equivalence proof exists to catch.
EQUIVALENCE_REQUIRED_IDENTITY_KEYS = ("build_fingerprint", "engine_version")


def run_second_producer(
    filter_expr: str,
    ue_root: Path,
    project_path: Path,
    report_dir: Path,
    log_path: Path,
    timeout_sec: float = 1800.0,
) -> Dict[str, Any]:
    """Producer B: its own control flow, sharing only the normative building blocks.

    Deliberately does not call run_acceptance() — reimplements launch, wait,
    and artifact collection independently so run_ue_acceptance.py is not both
    producers wearing different flags.
    """
    editor_cmd = ue_root / "Engine" / "Binaries" / "Linux" / "UnrealEditor-Cmd"
    if not editor_cmd.is_file() or not os.access(editor_cmd, os.X_OK):
        raise RuntimeError(f"UnrealEditor-Cmd not found or not executable at: {editor_cmd}")
    if not project_path.is_file():
        raise RuntimeError(f"Project file not found: {project_path}")

    report_dir = report_dir.resolve()
    if report_dir.exists():
        for child in report_dir.iterdir():
            if child.is_file():
                child.unlink()
    report_dir.mkdir(parents=True, exist_ok=True)

    log_path = log_path.resolve()
    log_path.parent.mkdir(parents=True, exist_ok=True)
    if log_path.exists():
        log_path.unlink()

    run_id = f"second-producer-{uuid.uuid4().hex[:8]}"
    run_identity = compute_run_identity(project_root=REPO_ROOT, run_id=run_id)

    argv = build_acceptance_argv(editor_cmd, project_path, filter_expr, report_dir, log_path)

    print(f"[Producer B] Executing: {' '.join(argv)}")
    start_time = time.time()
    proc = subprocess.run(argv, capture_output=True, text=True, timeout=timeout_sec)
    elapsed = time.time() - start_time
    print(f"[Producer B] UnrealEditor-Cmd completed in {elapsed:.2f}s (exit code {proc.returncode}).")

    if proc.returncode != 0:
        raise RuntimeError(f"Producer B's UnrealEditor-Cmd exited with non-zero code: {proc.returncode}")

    if not log_path.is_file():
        raise RuntimeError(f"Producer B's automation log was not generated at: {log_path}")
    log_content = log_path.read_text(encoding="utf-8", errors="replace")

    if "**** TEST COMPLETE. EXIT CODE: 0 ****" not in log_content:
        raise RuntimeError("Producer B's engine did not reach the clean completion marker.")

    discovered = parse_discovery_from_log(log_content)
    if not discovered:
        raise RuntimeError("Producer B found no discovered tests in its own automation log.")
    print(f"[Producer B] Discovered {len(discovered)} tests from its own automation inventory.")

    index_json_path = report_dir / "index.json"
    if not index_json_path.is_file():
        raise RuntimeError(f"Producer B's machine report index.json was not found at: {index_json_path}")

    normalized_report = normalize_ue_json_report(
        index_json_path,
        run_id=run_id,
        report_file_path=index_json_path,
    )

    bundle_path = report_dir / "evidence_bundle.json"
    write_evidence_bundle(bundle_path, discovered, normalized_report, run_identity)
    print(f"[Producer B] Evidence bundle: {bundle_path}")
    return load_evidence_bundle(bundle_path)


def compare_bundles(bundle_a: Dict[str, Any], bundle_b: Dict[str, Any]) -> List[str]:
    """AEP-06 equivalence check. Returns diagnostics; empty means the two bundles are equivalent."""
    diagnostics: List[str] = []

    discovered_a = set(bundle_a.get("discovered") or [])
    discovered_b = set(bundle_b.get("discovered") or [])
    if discovered_a != discovered_b:
        only_a = sorted(discovered_a - discovered_b)
        only_b = sorted(discovered_b - discovered_a)
        diagnostics.append(
            "Discovered test id sets differ between producers "
            f"(only in A: {only_a}; only in B: {only_b})."
        )

    identity_a = bundle_a.get("run_identity") or {}
    identity_b = bundle_b.get("run_identity") or {}
    for key in EQUIVALENCE_REQUIRED_IDENTITY_KEYS:
        value_a = identity_a.get(key)
        value_b = identity_b.get(key)
        if value_a != value_b:
            diagnostics.append(f"'{key}' differs between producers: A={value_a!r}, B={value_b!r}.")

    return diagnostics


def main() -> int:
    parser = argparse.ArgumentParser(
        description="AEP-06: prove two independent acceptance producers are equivalent."
    )
    parser.add_argument("--filter", "-f", default="GV2.UI", help="Automation test filter (default: 'GV2.UI').")
    parser.add_argument("--ue-root", default=os.environ.get("UE_ROOT", DEFAULT_UE_ROOT))
    parser.add_argument("--project", default=str(REPO_ROOT / "GV2.uproject"))
    parser.add_argument("--timeout", type=float, default=1800.0)
    args = parser.parse_args()

    ue_root = Path(args.ue_root)
    project_path = Path(args.project)

    producer_a_report_dir = REPO_ROOT / "Saved" / "Automation" / "ReportsProducerA"
    producer_a_log_path = REPO_ROOT / "Saved" / "Logs" / "GV2AcceptanceProducerA.log"
    producer_b_report_dir = REPO_ROOT / "Saved" / "Automation" / "ReportsProducerB"
    producer_b_log_path = REPO_ROOT / "Saved" / "Logs" / "GV2AcceptanceProducerB.log"

    print("=" * 70)
    print("PRODUCER A — local runner (run_ue_acceptance.run_acceptance)")
    print("=" * 70)
    exit_a = run_acceptance(
        filter_expr=args.filter,
        ue_root=ue_root,
        project_path=project_path,
        report_dir=producer_a_report_dir,
        log_path=producer_a_log_path,
        timeout_sec=args.timeout,
    )
    if exit_a != 0:
        print("ERROR: Producer A (local runner) failed; cannot prove equivalence.", file=sys.stderr)
        return 1
    bundle_a = load_evidence_bundle(producer_a_report_dir / "evidence_bundle.json")

    print()
    print("=" * 70)
    print("PRODUCER B — independent control flow, normative argv only")
    print("=" * 70)
    try:
        bundle_b = run_second_producer(
            filter_expr=args.filter,
            ue_root=ue_root,
            project_path=project_path,
            report_dir=producer_b_report_dir,
            log_path=producer_b_log_path,
            timeout_sec=args.timeout,
        )
    except Exception as e:
        print(f"ERROR: Producer B failed: {e}", file=sys.stderr)
        return 1

    print()
    print("=" * 70)
    print("VALIDATING PRODUCER B'S BUNDLE INDEPENDENTLY")
    print("=" * 70)
    diagnostics_b = validate_evidence_bundle(bundle_b)
    if diagnostics_b:
        print("ERROR: Producer B's own bundle failed validation, with no rule relaxed for it:", file=sys.stderr)
        for d in diagnostics_b:
            print(f"  - {d}", file=sys.stderr)
        return 1
    print("Producer B's bundle validates cleanly — no rule was relaxed for a bundle collected outside the runner.")

    print()
    print("=" * 70)
    print("COMPARING PRODUCER A AND PRODUCER B")
    print("=" * 70)
    discovered_a = set(bundle_a.get("discovered") or [])
    discovered_b = set(bundle_b.get("discovered") or [])
    identity_a = bundle_a.get("run_identity") or {}
    identity_b = bundle_b.get("run_identity") or {}
    print(f"Producer A discovered {len(discovered_a)} test ids; Producer B discovered {len(discovered_b)}.")
    print(f"Producer A build_fingerprint: {identity_a.get('build_fingerprint')}")
    print(f"Producer B build_fingerprint: {identity_b.get('build_fingerprint')}")
    print(f"Producer A engine_version:    {identity_a.get('engine_version')}")
    print(f"Producer B engine_version:    {identity_b.get('engine_version')}")
    print(
        "run_id excluded from comparison by construction: "
        f"A={identity_a.get('run_id')!r}, B={identity_b.get('run_id')!r}"
    )

    comparison_diagnostics = compare_bundles(bundle_a, bundle_b)
    if comparison_diagnostics:
        print("\nEQUIVALENCE FAILED:", file=sys.stderr)
        for d in comparison_diagnostics:
            print(f"  - {d}", file=sys.stderr)
        return 1

    print("\nSUCCESS: Producer A and Producer B are equivalent.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
