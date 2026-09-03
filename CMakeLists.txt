cmake_minimum_required(VERSION 3.25)
project(GV2Headless LANGUAGES C CXX)

include(CTest)
include("${CMAKE_CURRENT_SOURCE_DIR}/Tools/Testing/GV2CliContractTests.cmake")

set(GV2_PCC_FIXTURE_ROOT
    "${PROJECT_SOURCE_DIR}/Tests/Fixtures/PortableContentCore"
    CACHE INTERNAL "Canonical shared PortableContentCore fixture root"
)

add_subdirectory(Source)
add_subdirectory(Tools/Content)
add_subdirectory(Headless)

if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(
        NAME ctest_expected_failure_contract
        COMMAND
            "${CMAKE_COMMAND}"
            "-DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/Tools/Testing/ValidateNoWillFail.cmake"
    )
    add_test(
        NAME ctest_process_contract_self_test
        COMMAND
            "${CMAKE_COMMAND}"
            "-DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/Tools/Testing/AssertProcessSelfTest.cmake"
    )
    add_test(
        NAME ctest_headless_json_contract_self_test
        COMMAND
            "${CMAKE_COMMAND}"
            "-DGV2_HEADLESS_PROGRAM=$<TARGET_FILE:gv2-headless>"
            "-DGV2_GOLDEN_DIGEST=${PROJECT_SOURCE_DIR}/Tests/Fixtures/GoldenRuns/golden_headless_10_seed_42.digest.json5"
            "-DGV2_FIXTURE_ROOT=${GV2_PCC_FIXTURE_ROOT}"
            "-DGV2_WORKING_DIRECTORY=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/Tools/Testing/AssertHeadlessJsonSelfTest.cmake"
    )
    add_test(
        NAME documentation_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Documentation/validate_docs.py"
    )
    add_test(
        NAME documentation_contract_self_test
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Documentation/validate_docs.py"
            "--self-test"
    )
    add_test(
        NAME pcc_shared_fixture_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Content/validate_pcc_fixtures.py"
            "${GV2_PCC_FIXTURE_ROOT}"
    )
    add_test(
        NAME host_conformance_parity_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Content/validate_host_conformance_parity.py"
    )
    add_test(
        NAME core_decoupling_gate_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Content/validate_core_decoupling.py"
    )
    add_test(
        NAME core_decoupling_gate_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Content/validate_core_decoupling.py"
            "--self-test"
    )
    add_test(
        NAME core_boundary_gate_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Content/validate_core_boundary.py"
    )
    # SAS-06: the package manifest is a generated artifact. Nothing prevents it
    # drifting from the sources by hand, so the committed manifest is verified
    # against what the generator produces — the same shape as the mods.lock check.
    add_test(
        NAME rh_manifest_up_to_date_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Content/generate_manifest.py"
            "${PROJECT_SOURCE_DIR}/GameData/rh"
            "--check"
    )
    add_test(
        NAME core_boundary_gate_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Content/validate_core_boundary.py"
            "--self-test"
    )
    add_test(
        NAME authoring_metadata_gate_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Content/validate_authoring_metadata.py"
    )
    add_test(
        NAME authoring_metadata_gate_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Content/validate_authoring_metadata.py"
            "--self-test"
    )
    # UPP-11: monotonic decrease gate for the Universal UI Property Pipeline migration --
    # schema-specific Prepare/Build functions, FGV2ScreenFieldValue payload members, and
    # schema-specific DTOs in GV2BridgeTypes.h must never grow after M2.
    add_test(
        NAME ui_pipeline_legacy_gate_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Content/validate_ui_pipeline_legacy_gate.py"
    )
    add_test(
        NAME ui_pipeline_legacy_gate_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Content/validate_ui_pipeline_legacy_gate.py"
            "--self-test"
    )
    # GBF-01: derive every fallible Engine AddChild call in the Game Shell attach
    # path from source and require its result to reach the transaction as failure.
    add_test(
        NAME shell_attach_failure_consumption_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_shell_attach_failure_consumption.py"
    )
    add_test(
        NAME shell_attach_failure_consumption_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_shell_attach_failure_consumption.py"
            "--self-test"
    )
    # GBF-03: enumerate capability struct members from the source declaration and
    # compare them with the independent classification table; do not use ABI layout
    # as a surrogate for semantic completeness.
    add_test(
        NAME ui_capability_member_inventory_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_ui_capability_member_inventory.py"
    )
    add_test(
        NAME ui_capability_member_inventory_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_ui_capability_member_inventory.py"
            "--self-test"
    )
    # GBF-07: derive cancellable Commit roots from production UI source and require
    # each independent sequence boundary to have an enum-backed recovery decision.
    add_test(
        NAME ui_rollback_boundary_inventory_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_ui_rollback_boundaries.py"
    )
    add_test(
        NAME ui_rollback_boundary_inventory_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_ui_rollback_boundaries.py"
            "--self-test"
    )
    # DCA-03: a class element hardcoded behind LoadClass/FindObject with a fixed /Game/
    # path is a hidden, backwards dependency of code on content layout. Scans Source/
    # itself rather than a fixed file list, so the pattern is caught wherever it recurs.
    add_test(
        NAME no_hardcoded_asset_paths_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_no_hardcoded_asset_paths.py"
    )
    add_test(
        NAME no_hardcoded_asset_paths_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_no_hardcoded_asset_paths.py"
            "--self-test"
    )
endif()
