cmake_minimum_required(VERSION 3.25)
project(GV2Headless LANGUAGES C CXX)

include(CTest)

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
endif()
