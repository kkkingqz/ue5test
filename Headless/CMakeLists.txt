add_executable(gv2-headless Source/main.cpp)
target_compile_features(gv2-headless PRIVATE cxx_std_20)
target_link_libraries(gv2-headless PRIVATE gv2_runtime_core gv2_content_core gv2_content_host_support gv2_test_support)

# TAS-09: single source of truth. Read out of the golden digest fixture
# itself rather than duplicating the value as a literal here.
file(READ
    "${PROJECT_SOURCE_DIR}/Tests/Fixtures/GoldenRuns/golden_headless_10_seed_42.digest.json5"
    GV2_GOLDEN_DIGEST_JSON
)
string(JSON GV2_GOLDEN_DIGEST_HASH GET "${GV2_GOLDEN_DIGEST_JSON}" digest_hash)

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

add_test(NAME gv2_headless_outputs_manifest_and_digest
    COMMAND gv2-headless --commands=10 --seed=42
)
set_tests_properties(gv2_headless_outputs_manifest_and_digest
    PROPERTIES
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        PASS_REGULAR_EXPRESSION "\"digest_hash\":\"[0-9a-f]+\""
)

# TAS-08: the golden run is pinned against the frozen test corpus
# (Tests/Fixtures/PortableContentCore/valid/core), not the live GameData/core
# — adding gameplay content never perturbs this digest.
add_test(NAME gv2_headless_golden_replay_matches_digest
    COMMAND gv2-headless
        --manifest=${PROJECT_SOURCE_DIR}/Tests/Fixtures/GoldenRuns/golden_headless_10_seed_42.manifest.json5
        --content-root=${GV2_PCC_FIXTURE_ROOT}/valid/core
)
set_tests_properties(gv2_headless_golden_replay_matches_digest
    PROPERTIES
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        PASS_REGULAR_EXPRESSION "\"digest_hash\":\"${GV2_GOLDEN_DIGEST_HASH}\""
)

add_test(NAME gv2_headless_check_scripts
    COMMAND gv2-headless --check-scripts
)
set_tests_properties(gv2_headless_check_scripts
    PROPERTIES
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        PASS_REGULAR_EXPRESSION "\"status\":\"ok\".*\"modules_checked\":[1-9][0-9]*"
)

find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(NAME gv2_headless_script_feedback_python
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tools/Content/test_script_feedback.py" "$<TARGET_FILE:gv2-headless>"
    )
endif()
