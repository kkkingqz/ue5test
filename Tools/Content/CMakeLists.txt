add_executable(gv2-content
    Source/main.cpp
    Source/Support/CliOutput.cpp
    Source/Support/DirectoryWatcher.cpp
    Source/Support/Json5AstRewriter.cpp
    Source/Support/Json5AstRewriterConformance.cpp
    Source/Support/PackageLoader.cpp
    Source/Commands/CoverageCommand.cpp
    Source/Commands/DeleteCommand.cpp
    Source/Commands/DescribeCommand.cpp
    Source/Commands/HashCommand.cpp
    Source/Commands/IndexCommand.cpp
    Source/Commands/InspectCommand.cpp
    Source/Commands/NewCommand.cpp
    Source/Commands/RefsCommand.cpp
    Source/Commands/RenameCommand.cpp
    Source/Commands/SetCommand.cpp
    Source/Commands/ValidateCommand.cpp
)
target_compile_features(gv2-content PRIVATE cxx_std_20)
target_include_directories(gv2-content PRIVATE Source)
target_link_libraries(gv2-content PRIVATE gv2_content_core gv2_content_host_support)

if(BUILD_TESTING)
    # TAS-07: GameData/core (live gameplay content) and
    # Tests/Fixtures/PortableContentCore/valid/core (TAS-06: frozen test
    # corpus, independent of GameData/core) each get their own pinned
    # value. GameData/core does not pin a content hash at all — only a
    # smoke validate check, so adding gameplay content never requires
    # touching a pinned value here. The fixture corpus's pinned hash lives
    # as a sibling of the corpus, not inside it: PCC-01 forbids any
    # "expected*" file within Tests/Fixtures/PortableContentCore itself
    # (validate_pcc_fixtures.py, CTest pcc_shared_fixture_contract).
    file(READ "${PROJECT_SOURCE_DIR}/Tests/Fixtures/expected_core_content_hash.txt" GV2_PCC_EXPECTED_HASH)
    string(STRIP "${GV2_PCC_EXPECTED_HASH}" GV2_PCC_EXPECTED_HASH)

    add_test(NAME gv2_content_validate_gamedata_core
        COMMAND gv2-content validate "${PROJECT_SOURCE_DIR}/GameData/core"
    )
    # CBM-follow-up: every gv2-content case except the live-content smoke
    # validate above runs against the frozen corpus, not GameData/core.
    # Two reasons. Игровой контент перестал быть фикстурой: он принадлежит
    # пакету игры и меняется свободно. И негативные кейсы `new … <id>`,
    # проверяющие отказ по дубликату, при отсутствии определения не
    # отказывают, а записывают его в целевой пакет — на живом дереве это
    # тихая порча, на корпусе её ловит pcc_shared_fixture_contract по
    # неиндексированному файлу.
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
    add_test(NAME gv2_content_validate_rejects_duplicate_key_exit_code
        COMMAND gv2-content validate "${GV2_PCC_FIXTURE_ROOT}/invalid/duplicate_key/core"
    )
    add_test(NAME gv2_content_validate_rejects_missing_root
        COMMAND gv2-content validate "${GV2_PCC_FIXTURE_ROOT}/does_not_exist"
    )
    add_test(NAME gv2_content_validate_rejects_missing_root_exit_code
        COMMAND gv2-content validate "${GV2_PCC_FIXTURE_ROOT}/does_not_exist"
    )

    add_test(NAME gv2_content_validate_rejects_provenance
        COMMAND gv2-content validate "${GV2_PCC_FIXTURE_ROOT}/valid/core" --provenance
    )
    add_test(NAME gv2_content_validate_rejects_provenance_exit_code
        COMMAND gv2-content validate "${GV2_PCC_FIXTURE_ROOT}/valid/core" --provenance
    )

    add_test(NAME gv2_content_describe_item_text
        COMMAND gv2-content describe "${PROJECT_SOURCE_DIR}/GameData/rh" item
    )
    add_test(NAME gv2_content_describe_item_json
        COMMAND gv2-content describe "${PROJECT_SOURCE_DIR}/GameData/rh" item --format=json
    )
    add_test(NAME gv2_content_describe_rejects_unknown_type
        COMMAND gv2-content describe "${GV2_PCC_FIXTURE_ROOT}/valid/core" nonexistent_type
    )
    add_test(NAME gv2_content_describe_rejects_unknown_type_exit_code
        COMMAND gv2-content describe "${GV2_PCC_FIXTURE_ROOT}/valid/core" nonexistent_type
    )
    add_test(NAME gv2_content_describe_rejects_unknown_type_json
        COMMAND gv2-content describe "${GV2_PCC_FIXTURE_ROOT}/valid/core" nonexistent_type --format=json
    )
    add_test(NAME gv2_content_describe_rejects_unknown_type_json_exit_code
        COMMAND gv2-content describe "${GV2_PCC_FIXTURE_ROOT}/valid/core" nonexistent_type --format=json
    )

    add_test(NAME gv2_content_new_rejects_duplicate_id
        COMMAND gv2-content new "${GV2_PCC_FIXTURE_ROOT}/valid/core" screen "core:screen.main"
    )
    add_test(NAME gv2_content_new_rejects_duplicate_id_exit_code
        COMMAND gv2-content new "${GV2_PCC_FIXTURE_ROOT}/valid/core" screen "core:screen.main"
    )
    add_test(NAME gv2_content_new_rejects_duplicate_id_json
        COMMAND gv2-content new "${GV2_PCC_FIXTURE_ROOT}/valid/core" screen "core:screen.main" --format=json
    )
    add_test(NAME gv2_content_new_rejects_duplicate_id_json_exit_code
        COMMAND gv2-content new "${GV2_PCC_FIXTURE_ROOT}/valid/core" screen "core:screen.main" --format=json
    )
    add_test(NAME gv2_content_new_rejects_invalid_id
        COMMAND gv2-content new "${GV2_PCC_FIXTURE_ROOT}/valid/core" item "not-valid-id"
    )
    add_test(NAME gv2_content_new_rejects_invalid_id_exit_code
        COMMAND gv2-content new "${GV2_PCC_FIXTURE_ROOT}/valid/core" item "not-valid-id"
    )
    add_test(NAME gv2_content_new_rejects_id_kind_mismatch
        COMMAND gv2-content new "${GV2_PCC_FIXTURE_ROOT}/valid/core" item "core:actor.hero"
    )
    add_test(NAME gv2_content_new_rejects_id_kind_mismatch_exit_code
        COMMAND gv2-content new "${GV2_PCC_FIXTURE_ROOT}/valid/core" item "core:actor.hero"
    )

    add_test(NAME gv2_content_refs_text
        COMMAND gv2-content refs "${GV2_PCC_FIXTURE_ROOT}/valid/core" core:text.screen.main.title
    )
    add_test(NAME gv2_content_refs_json
        COMMAND gv2-content refs "${GV2_PCC_FIXTURE_ROOT}/valid/core" core:text.screen.main.title --format=json
    )
    add_test(NAME gv2_content_refs_unreferenced
        COMMAND gv2-content refs "${GV2_PCC_FIXTURE_ROOT}/valid/core" core:text.unreferenced.sample
    )
    add_test(NAME gv2_content_refs_rejects_invalid_id
        COMMAND gv2-content refs "${GV2_PCC_FIXTURE_ROOT}/valid/core" not-valid-id
    )
    add_test(NAME gv2_content_refs_rejects_invalid_id_exit_code
        COMMAND gv2-content refs "${GV2_PCC_FIXTURE_ROOT}/valid/core" not-valid-id
    )

    add_test(NAME gv2_content_rename_rejects_nonexistent_id
        COMMAND gv2-content rename "${GV2_PCC_FIXTURE_ROOT}/valid/core" core:text.nonexistent.id core:text.new.id
    )
    add_test(NAME gv2_content_rename_rejects_nonexistent_id_exit_code
        COMMAND gv2-content rename "${GV2_PCC_FIXTURE_ROOT}/valid/core" core:text.nonexistent.id core:text.new.id
    )
    add_test(NAME gv2_content_rename_rejects_duplicate_id
        COMMAND gv2-content rename "${GV2_PCC_FIXTURE_ROOT}/valid/core" core:screen.inventory core:screen.main
    )
    add_test(NAME gv2_content_rename_rejects_duplicate_id_exit_code
        COMMAND gv2-content rename "${GV2_PCC_FIXTURE_ROOT}/valid/core" core:screen.inventory core:screen.main
    )
    add_test(NAME gv2_content_rename_rejects_invalid_id
        COMMAND gv2-content rename "${GV2_PCC_FIXTURE_ROOT}/valid/core" core:screen.inventory not-valid-id
    )
    add_test(NAME gv2_content_rename_rejects_invalid_id_exit_code
        COMMAND gv2-content rename "${GV2_PCC_FIXTURE_ROOT}/valid/core" core:screen.inventory not-valid-id
    )
    add_test(NAME gv2_content_rename_rejects_id_kind_mismatch
        COMMAND gv2-content rename "${GV2_PCC_FIXTURE_ROOT}/valid/core" core:screen.inventory core:item.inventory
    )
    add_test(NAME gv2_content_rename_rejects_id_kind_mismatch_exit_code
        COMMAND gv2-content rename "${GV2_PCC_FIXTURE_ROOT}/valid/core" core:screen.inventory core:item.inventory
    )

    add_test(NAME gv2_content_validate_watch_single_iteration
        COMMAND gv2-content validate "${GV2_PCC_FIXTURE_ROOT}/valid/core" --watch --max-iterations=1 --format=json
    )
    add_test(NAME gv2_content_validate_watch_rejects_other_commands
        COMMAND gv2-content inspect "${GV2_PCC_FIXTURE_ROOT}/valid/core" core:screen.main --watch
    )
    add_test(NAME gv2_content_validate_watch_rejects_other_commands_exit_code
        COMMAND gv2-content inspect "${GV2_PCC_FIXTURE_ROOT}/valid/core" core:screen.main --watch
    )

    add_test(NAME gv2_content_index_text
        COMMAND gv2-content index "${GV2_PCC_FIXTURE_ROOT}/valid/core"
    )
    add_test(NAME gv2_content_index_json
        COMMAND gv2-content index "${GV2_PCC_FIXTURE_ROOT}/valid/core" --format=json
    )
    add_test(NAME gv2_content_index_rejects_missing_root
        COMMAND gv2-content index
    )
    add_test(NAME gv2_content_index_rejects_missing_root_exit_code
        COMMAND gv2-content index
    )

    add_test(NAME gv2_content_self_test
        COMMAND gv2-content --self-test
    )

    add_test(NAME gv2_content_set_rejects_nonexistent_id
        COMMAND gv2-content set "${GV2_PCC_FIXTURE_ROOT}/valid/core" core:item.nonexistent price 10 --format=json
    )
    add_test(NAME gv2_content_set_rejects_nonexistent_id_exit_code
        COMMAND gv2-content set "${GV2_PCC_FIXTURE_ROOT}/valid/core" core:item.nonexistent price 10 --format=json
    )
    add_test(NAME gv2_content_set_rejects_nonexistent_pointer
        COMMAND gv2-content set "${GV2_PCC_FIXTURE_ROOT}/valid/core" core:item.weapon.iron_sword nonexistent_field 10 --format=json
    )
    add_test(NAME gv2_content_set_rejects_nonexistent_pointer_exit_code
        COMMAND gv2-content set "${GV2_PCC_FIXTURE_ROOT}/valid/core" core:item.weapon.iron_sword nonexistent_field 10 --format=json
    )
    add_test(NAME gv2_content_set_rejects_container
        COMMAND gv2-content set "${GV2_PCC_FIXTURE_ROOT}/valid/core" core:item.weapon.iron_sword /data 10 --format=json
    )
    add_test(NAME gv2_content_set_rejects_container_exit_code
        COMMAND gv2-content set "${GV2_PCC_FIXTURE_ROOT}/valid/core" core:item.weapon.iron_sword /data 10 --format=json
    )

    add_test(NAME gv2_content_delete_rejects_nonexistent_id
        COMMAND gv2-content delete "${GV2_PCC_FIXTURE_ROOT}/valid/core" core:item.nonexistent --format=json
    )
    add_test(NAME gv2_content_delete_rejects_nonexistent_id_exit_code
        COMMAND gv2-content delete "${GV2_PCC_FIXTURE_ROOT}/valid/core" core:item.nonexistent --format=json
    )
    add_test(NAME gv2_content_delete_rejects_referenced_id
        COMMAND gv2-content delete "${GV2_PCC_FIXTURE_ROOT}/valid/core" core:text.screen.main.title --format=json
    )
    add_test(NAME gv2_content_delete_rejects_referenced_id_exit_code
        COMMAND gv2-content delete "${GV2_PCC_FIXTURE_ROOT}/valid/core" core:text.screen.main.title --format=json
    )

    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_FOUND)
        add_test(NAME gv2_content_authoring_tools_python
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_authoring_tools.py" "$<TARGET_FILE:gv2-content>" "${GV2_PCC_FIXTURE_ROOT}/valid/core"
        )
    endif()

    set_tests_properties(
        gv2_content_validate_core_fixture
        gv2_content_inspect_core_fixture
        gv2_content_describe_item_text
        gv2_content_describe_item_json
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
        gv2_content_describe_item_text
        PROPERTIES
            PASS_REGULAR_EXPRESSION "definition_type: item.*schema_id: rh:schema.definition.item.v1.*price: int64 \\(required, min=0.*\\)"
    )
    set_tests_properties(
        gv2_content_describe_item_json
        PROPERTIES
            PASS_REGULAR_EXPRESSION "\"status\":\"ok\".*\"definition_type\":\"item\".*\"schema_id\":\"rh:schema.definition.item.v1\""
    )
    set_tests_properties(
        gv2_content_describe_rejects_unknown_type
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "unknown definition type 'nonexistent_type'"
    )
    set_tests_properties(
        gv2_content_describe_rejects_unknown_type_json
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "\"code\":\"unknown_definition_type\""
    )
    set_tests_properties(
        gv2_content_new_rejects_duplicate_id
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "already exists in package"
    )
    set_tests_properties(
        gv2_content_new_rejects_duplicate_id_json
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "\"code\":\"duplicate_definition_id\""
    )
    set_tests_properties(
        gv2_content_new_rejects_invalid_id
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "not a valid definition id"
    )
    set_tests_properties(
        gv2_content_new_rejects_id_kind_mismatch
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "does not match definition type 'item'"
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
    set_tests_properties(
        gv2_content_refs_text
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "definition_id: core:text.screen.main.title.*references: 1.*definitions/screens.json5:8:24"
    )
    set_tests_properties(
        gv2_content_refs_json
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "\"status\":\"ok\".*\"definition_id\":\"core:text.screen.main.title\".*\"references_count\":1"
    )
    set_tests_properties(
        gv2_content_refs_unreferenced
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "references: 0"
    )
    set_tests_properties(
        gv2_content_refs_rejects_invalid_id
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "not a valid definition id 'not-valid-id'"
    )
    set_tests_properties(
        gv2_content_rename_rejects_nonexistent_id
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "not found in package"
    )
    set_tests_properties(
        gv2_content_rename_rejects_duplicate_id
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "already exists in package"
    )
    set_tests_properties(
        gv2_content_rename_rejects_invalid_id
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "not a valid new definition id 'not-valid-id'"
    )
    set_tests_properties(
        gv2_content_rename_rejects_id_kind_mismatch
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "does not match old definition ID kind"
    )
    set_tests_properties(
        gv2_content_validate_watch_single_iteration
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "\"status\":\"ok\".*\"iteration\":1"
    )
    set_tests_properties(
        gv2_content_validate_watch_rejects_other_commands
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "--watch, --poll-interval, and --max-iterations are only supported for 'validate'"
    )
    set_tests_properties(
        gv2_content_index_text
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "package_id: core.*active_ids: [1-9][0-9]*.*\\[screen\\]"
    )
    set_tests_properties(
        gv2_content_index_json
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "\"status\":\"ok\".*\"package_id\":\"core\".*\"total_active_ids\":[1-9][0-9]*.*\"kinds\":\\{.*\"screen\":\\["
    )
    set_tests_properties(
        gv2_content_index_rejects_missing_root
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "index requires <package-root>"
    )
    set_tests_properties(
        gv2_content_self_test
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "gv2-content self-test passed."
    )
    set_tests_properties(
        gv2_content_set_rejects_nonexistent_id
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "\"code\":\"definition_not_found\""
    )
    set_tests_properties(
        gv2_content_set_rejects_nonexistent_pointer
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "\"code\":\"pointer_not_found\""
    )
    set_tests_properties(
        gv2_content_set_rejects_container
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "\"code\":\"target_is_container\""
    )
    set_tests_properties(
        gv2_content_delete_rejects_nonexistent_id
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "\"code\":\"definition_not_found\""
    )
    set_tests_properties(
        gv2_content_delete_rejects_referenced_id
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            PASS_REGULAR_EXPRESSION "\"code\":\"referenced_by_definitions\""
    )
    set_tests_properties(
        gv2_content_validate_rejects_duplicate_key_exit_code
        gv2_content_validate_rejects_missing_root_exit_code
        gv2_content_validate_rejects_provenance_exit_code
        gv2_content_describe_rejects_unknown_type_exit_code
        gv2_content_describe_rejects_unknown_type_json_exit_code
        gv2_content_new_rejects_duplicate_id_exit_code
        gv2_content_new_rejects_duplicate_id_json_exit_code
        gv2_content_new_rejects_invalid_id_exit_code
        gv2_content_new_rejects_id_kind_mismatch_exit_code
        gv2_content_refs_rejects_invalid_id_exit_code
        gv2_content_rename_rejects_nonexistent_id_exit_code
        gv2_content_rename_rejects_duplicate_id_exit_code
        gv2_content_rename_rejects_invalid_id_exit_code
        gv2_content_rename_rejects_id_kind_mismatch_exit_code
        gv2_content_set_rejects_nonexistent_id_exit_code
        gv2_content_set_rejects_nonexistent_pointer_exit_code
        gv2_content_set_rejects_container_exit_code
        gv2_content_delete_rejects_nonexistent_id_exit_code
        gv2_content_delete_rejects_referenced_id_exit_code
        gv2_content_validate_watch_rejects_other_commands_exit_code
        gv2_content_index_rejects_missing_root_exit_code
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            WILL_FAIL TRUE
    )
endif()
