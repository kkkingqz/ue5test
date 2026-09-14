#!/usr/bin/env python3
"""Contract tests for the CFC-13 plan/evidence and native-enum inventory gate."""

from __future__ import annotations

import copy
import tempfile
import unittest
from pathlib import Path

import validate_cpp_foundation_closure as closure


class TestCppFoundationClosureInventory(unittest.TestCase):
    def test_repository_inventory_is_complete(self) -> None:
        self.assertEqual(closure.validate_repository(), [])

    def test_unmapped_task_heading_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            plan_dir = Path(temp_dir)
            (plan_dir / "Plan.md").write_text(
                "## CFC-99 — Synthetic task\n\n"
                "- [ ] CFC-99 — Synthetic task\n\n"
                "**Done:**\n"
                "- Synthetic requirement.\n\n"
                "**Evidence:** synthetic.\n",
                encoding="utf-8",
            )
            errors = closure.validate_plan_inventory(plan_dir, closure.TASK_EVIDENCE)

        self.assertTrue(any("unmapped task CFC-99" in error for error in errors), errors)

    def test_new_done_bullet_is_rejected_until_evidence_is_added(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            plan_dir = Path(temp_dir)
            (plan_dir / "Plan.md").write_text(
                "## CFC-13 — Synthetic closure\n\n"
                "- [ ] CFC-13 — Synthetic closure\n\n"
                "**Done:**\n"
                "- First requirement.\n"
                "- Unmapped requirement.\n\n"
                "**Evidence:** synthetic.\n",
                encoding="utf-8",
            )
            evidence = {"CFC-13": copy.deepcopy(closure.TASK_EVIDENCE["CFC-13"])}
            evidence["CFC-13"] = evidence["CFC-13"]._replace(done_count=1)
            errors = closure.validate_plan_inventory(plan_dir, evidence)

        self.assertTrue(any("Done count" in error for error in errors), errors)

    def test_unhandled_runtime_phase_is_rejected(self) -> None:
        header = """
        enum class ERuntimeLifecyclePhase
        {
            Registering,
            BuildingState,
            RestoringInstances,
            Starting,
            Recovering
        };
        """
        consumer = """
        switch (Phase)
        {
        case GV2RuntimeCore::ERuntimeLifecyclePhase::Registering: break;
        case GV2RuntimeCore::ERuntimeLifecyclePhase::BuildingState: break;
        case GV2RuntimeCore::ERuntimeLifecyclePhase::RestoringInstances: break;
        case GV2RuntimeCore::ERuntimeLifecyclePhase::Starting: break;
        }
        """

        errors = closure.validate_enum_dispatch(
            "ERuntimeLifecyclePhase", header, {"consumer.cpp": consumer}
        )

        self.assertTrue(any("Recovering" in error for error in errors), errors)

    def test_default_does_not_hide_missing_runtime_phase(self) -> None:
        header = "enum class ERuntimeLifecyclePhase { Registering, Starting };"
        consumer = """
        switch (Phase)
        {
        case GV2RuntimeCore::ERuntimeLifecyclePhase::Registering: break;
        default: break;
        }
        """

        errors = closure.validate_enum_dispatch(
            "ERuntimeLifecyclePhase", header, {"consumer.cpp": consumer}
        )

        self.assertTrue(any("Starting" in error for error in errors), errors)
        self.assertTrue(any("default" in error for error in errors), errors)

    def test_new_public_enum_value_requires_test_inventory(self) -> None:
        header = "enum class EOutcome { Completed, Failed, Retried };"
        tests = "EOutcome::Completed; EOutcome::Failed;"

        errors = closure.validate_enum_test_inventory("EOutcome", header, {"tests.cpp": tests})

        self.assertTrue(any("Retried" in error for error in errors), errors)


if __name__ == "__main__":
    unittest.main(verbosity=2)
