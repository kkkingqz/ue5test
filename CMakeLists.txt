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
endif()
