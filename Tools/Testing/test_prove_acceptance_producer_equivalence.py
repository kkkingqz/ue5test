#!/usr/bin/env python3
"""Unit tests for prove_acceptance_producer_equivalence.compare_bundles.

These test the pure comparison logic on synthetic bundles — they do not spawn
UnrealEditor-Cmd. The actual AEP-06 evidence (two real bundles from two real
producers) is gathered by running prove_acceptance_producer_equivalence.py
itself, which needs an installed engine and is not part of the portable
suite; this file is what keeps the comparison rule itself covered by CTest.
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path
from typing import Any, Dict, Iterable

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from Tools.Testing.prove_acceptance_producer_equivalence import (
    EQUIVALENCE_EXCLUDED_IDENTITY_KEYS,
    EQUIVALENCE_REQUIRED_IDENTITY_KEYS,
    compare_bundles,
)


def _make_bundle(discovered: Iterable[str], run_identity: Dict[str, str]) -> Dict[str, Any]:
    return {
        "bundle_schema_version": "gv2-acceptance-evidence-bundle-v1",
        "discovered": sorted(discovered),
        "run_identity": dict(run_identity),
        "report": {},
    }


class TestCompareBundles(unittest.TestCase):
    def setUp(self) -> None:
        self.identity_a = {
            "run_id": "producer-a-run",
            "source_revision": "a" * 40,
            "source_diff_hash": "clean",
            "build_fingerprint": "fingerprint-1",
            "engine_version": "5.8",
        }
        # run_id deliberately different from identity_a — proves it is
        # excluded from the comparison, not accidentally matching.
        self.identity_b = {
            "run_id": "producer-b-run",
            "source_revision": "a" * 40,
            "source_diff_hash": "clean",
            "build_fingerprint": "fingerprint-1",
            "engine_version": "5.8",
        }
        self.discovered = {"GV2.Test.A", "GV2.Test.B"}

    def test_equivalence_key_groups_are_disjoint_and_recorded(self) -> None:
        self.assertEqual(EQUIVALENCE_EXCLUDED_IDENTITY_KEYS, ("run_id",))
        self.assertEqual(set(EQUIVALENCE_REQUIRED_IDENTITY_KEYS), {"build_fingerprint", "engine_version"})
        self.assertEqual(
            set(EQUIVALENCE_EXCLUDED_IDENTITY_KEYS) & set(EQUIVALENCE_REQUIRED_IDENTITY_KEYS),
            set(),
        )

    def test_equivalent_bundles_with_different_run_id_pass(self) -> None:
        bundle_a = _make_bundle(self.discovered, self.identity_a)
        bundle_b = _make_bundle(self.discovered, self.identity_b)
        self.assertEqual(compare_bundles(bundle_a, bundle_b), [])

    def test_mutation_discovered_set_mismatch_is_detected(self) -> None:
        bundle_a = _make_bundle(self.discovered, self.identity_a)
        bundle_b = _make_bundle({"GV2.Test.A", "GV2.Test.C"}, self.identity_b)
        diagnostics = compare_bundles(bundle_a, bundle_b)
        self.assertTrue(any("Discovered test id sets differ" in d for d in diagnostics))
        self.assertTrue(any("GV2.Test.B" in d for d in diagnostics))
        self.assertTrue(any("GV2.Test.C" in d for d in diagnostics))

    def test_mutation_build_fingerprint_mismatch_is_detected(self) -> None:
        bundle_a = _make_bundle(self.discovered, self.identity_a)
        other_identity_b = dict(self.identity_b)
        other_identity_b["build_fingerprint"] = "fingerprint-2"
        bundle_b = _make_bundle(self.discovered, other_identity_b)
        diagnostics = compare_bundles(bundle_a, bundle_b)
        self.assertTrue(any("'build_fingerprint' differs" in d for d in diagnostics))

    def test_mutation_engine_version_mismatch_is_detected(self) -> None:
        bundle_a = _make_bundle(self.discovered, self.identity_a)
        other_identity_b = dict(self.identity_b)
        other_identity_b["engine_version"] = "5.9"
        bundle_b = _make_bundle(self.discovered, other_identity_b)
        diagnostics = compare_bundles(bundle_a, bundle_b)
        self.assertTrue(any("'engine_version' differs" in d for d in diagnostics))

    def test_comparing_counts_alone_would_hide_a_real_divergence(self) -> None:
        """Regression guard for 'не считается закрытием: сравнить только число тестов'."""
        bundle_a = _make_bundle({"GV2.Test.A", "GV2.Test.B"}, self.identity_a)
        bundle_b = _make_bundle({"GV2.Test.A", "GV2.Test.C"}, self.identity_b)  # same COUNT, different SET
        self.assertEqual(len(bundle_a["discovered"]), len(bundle_b["discovered"]))
        diagnostics = compare_bundles(bundle_a, bundle_b)
        self.assertNotEqual(diagnostics, [], "Equal counts with a different set must still be caught.")

    def test_missing_discovered_field_is_treated_as_empty_not_a_crash(self) -> None:
        bundle_a = _make_bundle(self.discovered, self.identity_a)
        bundle_b = {"run_identity": dict(self.identity_b)}
        diagnostics = compare_bundles(bundle_a, bundle_b)
        self.assertTrue(any("Discovered test id sets differ" in d for d in diagnostics))


def main() -> int:
    suite = unittest.TestSuite()
    suite.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(TestCompareBundles))
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
