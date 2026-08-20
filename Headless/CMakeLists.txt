add_executable(gv2-headless Source/main.cpp)
target_compile_features(gv2-headless PRIVATE cxx_std_20)
target_link_libraries(gv2-headless PRIVATE gv2_runtime_core gv2_content_core gv2_content_host_support gv2_test_support)

set(GV2_GOLDEN_DIGEST_PATH
    "${PROJECT_SOURCE_DIR}/Tests/Fixtures/GoldenRuns/golden_headless_10_seed_42.digest.json5"
)

function(gv2_add_headless_json_contract_test TEST_NAME MODE)
    list(JOIN ARGN ";" TEST_ARGUMENTS)
    add_test(
        NAME ${TEST_NAME}
        COMMAND
            "${CMAKE_COMMAND}"
            "-DGV2_PROGRAM=$<TARGET_FILE:gv2-headless>"
            "-DGV2_ARGUMENTS=${TEST_ARGUMENTS}"
            "-DGV2_MODE=${MODE}"
            "-DGV2_GOLDEN_DIGEST=${GV2_GOLDEN_DIGEST_PATH}"
            "-DGV2_WORKING_DIRECTORY=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/Tools/Testing/AssertHeadlessJson.cmake"
    )
endfunction()

add_test(NAME gv2_headless_self_test COMMAND gv2-headless --self-test)
set_tests_properties(gv2_headless_self_test PROPERTIES
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

# PCC-35: missing/invalid repository content must block session startup
# before Lua bootstrap even runs.
gv2_add_cli_contract_test(
    NAME gv2_headless_rejects_missing_content_root
    TARGET gv2-headless
    EXPECTED_EXIT_CODE 9
    EXPECTED_OUTPUT_REGEX "core:diagnostic.package.discovery.root_not_found"
    ARGS --content-root=${PROJECT_SOURCE_DIR}/Tests/Fixtures/PortableContentCore/does_not_exist
)
gv2_add_cli_contract_test(
    NAME gv2_headless_rejects_invalid_content_root
    TARGET gv2-headless
    EXPECTED_EXIT_CODE 9
    EXPECTED_OUTPUT_REGEX "core:diagnostic.json5.duplicate_key"
    ARGS --content-root=${PROJECT_SOURCE_DIR}/Tests/Fixtures/PortableContentCore/invalid/duplicate_key/core
)

gv2_add_headless_json_contract_test(
    gv2_headless_outputs_manifest_and_digest
    GOLDEN_RUN
    --commands=10
    --seed=42
    --content-root=${GV2_PCC_FIXTURE_ROOT}/valid/core
)

# TAS-08: the golden run is pinned against the frozen test corpus
# (Tests/Fixtures/PortableContentCore/valid/core), not the live GameData/core
# — adding gameplay content never perturbs this digest.
gv2_add_headless_json_contract_test(
    gv2_headless_golden_replay_matches_digest
    GOLDEN_RUN
    --manifest=${PROJECT_SOURCE_DIR}/Tests/Fixtures/GoldenRuns/golden_headless_10_seed_42.manifest.json5
    --content-root=${GV2_PCC_FIXTURE_ROOT}/valid/core
)

gv2_add_headless_json_contract_test(
    gv2_headless_check_scripts
    GOLDEN_CHECK_SCRIPTS
    --check-scripts
    --content-root=${GV2_PCC_FIXTURE_ROOT}/valid/core
)

gv2_add_headless_json_contract_test(
    gv2_headless_live_run_smoke
    LIVE_RUN_SMOKE
    --commands=1
    --seed=42
)
gv2_add_headless_json_contract_test(
    gv2_headless_check_scripts_live_smoke
    LIVE_CHECK_SCRIPTS_SMOKE
    --check-scripts
)

find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(NAME gv2_headless_script_feedback_python
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tools/Content/test_script_feedback.py" "$<TARGET_FILE:gv2-headless>"
    )
endif()
