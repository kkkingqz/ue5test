add_executable(gv2-content Source/main.cpp)
target_compile_features(gv2-content PRIVATE cxx_std_20)
target_link_libraries(gv2-content PRIVATE gv2_content_core)

if(BUILD_TESTING)
    file(READ "${PROJECT_SOURCE_DIR}/Tests/Fixtures/expected_core_content_hash.txt" GV2_PCC_EXPECTED_HASH)
    string(STRIP "${GV2_PCC_EXPECTED_HASH}" GV2_PCC_EXPECTED_HASH)

    add_test(NAME gv2_content_validate_core_fixture
        COMMAND gv2-content validate "${GV2_PCC_FIXTURE_ROOT}/valid/core"
    )
    add_test(NAME gv2_content_validate_core_fixture_json
        COMMAND gv2-content validate "${GV2_PCC_FIXTURE_ROOT}/valid/core" --format=json
    )
    add_test(NAME gv2_content_hash_core_fixture
        COMMAND gv2-content hash "${GV2_PCC_FIXTURE_ROOT}/valid/core"
    )
    add_test(NAME gv2_content_inspect_core_fixture
        COMMAND gv2-content inspect "${GV2_PCC_FIXTURE_ROOT}/valid/core" "core:item.weapon.iron_sword" --provenance --format=json
    )
    add_test(NAME gv2_content_validate_rejects_duplicate_key
        COMMAND gv2-content validate "${GV2_PCC_FIXTURE_ROOT}/invalid/duplicate_key/core"
    )
    add_test(NAME gv2_content_validate_rejects_missing_root
        COMMAND gv2-content validate "${GV2_PCC_FIXTURE_ROOT}/does_not_exist"
    )

    add_test(NAME gv2_content_validate_rejects_provenance
        COMMAND gv2-content validate "${GV2_PCC_FIXTURE_ROOT}/valid/core" --provenance
    )

    set_tests_properties(
        gv2_content_validate_core_fixture
        gv2_content_inspect_core_fixture
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    set_tests_properties(
        gv2_content_hash_core_fixture
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "${GV2_PCC_EXPECTED_HASH}"
    )
    set_tests_properties(
        gv2_content_validate_core_fixture_json
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "\"content_hash\":[ \t]*\"${GV2_PCC_EXPECTED_HASH}\""
    )
    set_tests_properties(
        gv2_content_validate_rejects_duplicate_key
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "core:diagnostic.json5.duplicate_key"
    )
    set_tests_properties(
        gv2_content_validate_rejects_missing_root
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "package root not found or not a directory"
    )
    set_tests_properties(
        gv2_content_validate_rejects_provenance
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "--provenance is only supported for 'inspect'"
    )
endif()
