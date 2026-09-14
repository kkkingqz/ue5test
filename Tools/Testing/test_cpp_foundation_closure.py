#!/usr/bin/env python3
"""Contract tests for the accepted C++ foundation baseline gate."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

import validate_cpp_foundation_closure as closure


class TestCppFoundationBaselineInventory(unittest.TestCase):
    def test_repository_inventory_is_complete(self) -> None:
        self.assertEqual(closure.validate_repository(), [])

    def test_removed_baseline_check_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            empty_root = Path(temp_dir)
            (empty_root / "CMakeLists.txt").write_text("", encoding="utf-8")
            errors = closure.validate_named_checks(empty_root)

        self.assertTrue(
            any("no longer registered" in error for error in errors), errors
        )
        self.assertTrue(
            any(check in error for check in closure.BASELINE_UE_TESTS for error in errors),
            errors,
        )

    def test_targeted_mutation_check_must_stay_in_baseline(self) -> None:
        for mutation_id, row in closure.TARGETED_MUTATIONS.items():
            known = (
                closure.BASELINE_UE_TESTS
                if row.kind == "ue"
                else closure.BASELINE_CTESTS
            )
            self.assertIn(row.check, known, mutation_id)

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

    def test_lua_error_cannot_jump_over_repository_raii(self) -> None:
        unsafe_source = """
        static int RepositoryRequire(lua_State* State)
        {
            std::string Code = "not_found";
            return luaL_error(State, "%s", Code.c_str());
        }
        """

        errors = closure.validate_lua_callback_error_boundary(unsafe_source)

        self.assertTrue(any("luaL_error" in error for error in errors), errors)


if __name__ == "__main__":
    unittest.main(verbosity=2)
