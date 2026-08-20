foreach(REQUIRED_VARIABLE GV2_PROGRAM GV2_MODE GV2_WORKING_DIRECTORY)
    if(NOT DEFINED ${REQUIRED_VARIABLE} OR "${${REQUIRED_VARIABLE}}" STREQUAL "")
        message(FATAL_ERROR "AssertHeadlessJson.cmake requires ${REQUIRED_VARIABLE}")
    endif()
endforeach()

execute_process(
    COMMAND "${GV2_PROGRAM}" ${GV2_ARGUMENTS}
    WORKING_DIRECTORY "${GV2_WORKING_DIRECTORY}"
    RESULT_VARIABLE ACTUAL_RESULT
    OUTPUT_VARIABLE PROCESS_STDOUT
    ERROR_VARIABLE PROCESS_STDERR
    TIMEOUT 60
)

if(NOT "${ACTUAL_RESULT}" MATCHES "^-?[0-9]+$")
    message(FATAL_ERROR
        "Headless process did not exit normally (result=${ACTUAL_RESULT}).\n"
        "stdout:\n${PROCESS_STDOUT}\nstderr:\n${PROCESS_STDERR}")
endif()
if(NOT ACTUAL_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Headless process returned ${ACTUAL_RESULT}, expected 0.\n"
        "stdout:\n${PROCESS_STDOUT}\nstderr:\n${PROCESS_STDERR}")
endif()
if(NOT PROCESS_STDERR STREQUAL "")
    message(FATAL_ERROR "Successful headless process wrote to stderr:\n${PROCESS_STDERR}")
endif()

string(STRIP "${PROCESS_STDOUT}" ACTUAL_JSON)

function(json_value OUTPUT JSON)
    string(JSON VALUE ERROR_VARIABLE JSON_ERROR GET "${JSON}" ${ARGN})
    if(NOT JSON_ERROR STREQUAL "NOTFOUND")
        message(FATAL_ERROR "Invalid or incomplete JSON at '${ARGN}': ${JSON_ERROR}\n${JSON}")
    endif()
    set(${OUTPUT} "${VALUE}" PARENT_SCOPE)
endfunction()

function(assert_json_equal LABEL EXPECTED JSON)
    json_value(ACTUAL "${JSON}" ${ARGN})
    if(NOT ACTUAL STREQUAL "${EXPECTED}")
        message(FATAL_ERROR "${LABEL}: expected '${EXPECTED}', got '${ACTUAL}'")
    endif()
endfunction()

function(assert_sha256 LABEL VALUE)
    string(LENGTH "${VALUE}" VALUE_LENGTH)
    if(NOT VALUE_LENGTH EQUAL 64 OR NOT "${VALUE}" MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR "${LABEL}: expected lowercase SHA-256, got '${VALUE}'")
    endif()
endfunction()

assert_json_equal("ok" ON "${ACTUAL_JSON}" ok)

if(GV2_MODE STREQUAL "GOLDEN_RUN")
    if(NOT DEFINED GV2_GOLDEN_DIGEST OR GV2_GOLDEN_DIGEST STREQUAL "")
        message(FATAL_ERROR "GOLDEN_RUN requires GV2_GOLDEN_DIGEST")
    endif()
    file(READ "${GV2_GOLDEN_DIGEST}" GOLDEN_JSON)

    foreach(FIELD lua_release_num repository_content_hash script_set_hash seed digest_hash)
        json_value(EXPECTED "${GOLDEN_JSON}" ${FIELD})
        assert_json_equal("top-level ${FIELD}" "${EXPECTED}" "${ACTUAL_JSON}" ${FIELD})
    endforeach()
    json_value(EXPECTED_COMMANDS "${GOLDEN_JSON}" executed_commands_count)
    assert_json_equal("top-level commands" "${EXPECTED_COMMANDS}" "${ACTUAL_JSON}" commands)
    assert_json_equal("media payload policy" OFF "${ACTUAL_JSON}" media_payload_loaded)
    assert_json_equal("localization policy" OFF "${ACTUAL_JSON}" localization_resolved)

    foreach(FIELD
        digest_hash
        lua_release_num
        repository_content_hash
        script_set_hash
        seed
        executed_commands_count
        success
        final_screen_id
        state_hash
        fault_code)
        json_value(EXPECTED "${GOLDEN_JSON}" ${FIELD})
        assert_json_equal("digest.${FIELD}" "${EXPECTED}" "${ACTUAL_JSON}" digest ${FIELD})
    endforeach()
elseif(GV2_MODE STREQUAL "GOLDEN_CHECK_SCRIPTS")
    if(NOT DEFINED GV2_GOLDEN_DIGEST OR GV2_GOLDEN_DIGEST STREQUAL "")
        message(FATAL_ERROR "GOLDEN_CHECK_SCRIPTS requires GV2_GOLDEN_DIGEST")
    endif()
    file(READ "${GV2_GOLDEN_DIGEST}" GOLDEN_JSON)
    assert_json_equal("status" ok "${ACTUAL_JSON}" status)
    foreach(FIELD repository_content_hash script_set_hash)
        json_value(EXPECTED "${GOLDEN_JSON}" ${FIELD})
        assert_json_equal("${FIELD}" "${EXPECTED}" "${ACTUAL_JSON}" ${FIELD})
    endforeach()
    json_value(MODULES_CHECKED "${ACTUAL_JSON}" modules_checked)
    if(NOT MODULES_CHECKED MATCHES "^[0-9]+$" OR NOT MODULES_CHECKED GREATER 0)
        message(FATAL_ERROR "modules_checked must be a positive integer, got '${MODULES_CHECKED}'")
    endif()
elseif(GV2_MODE STREQUAL "LIVE_RUN_SMOKE")
    assert_json_equal("commands" 1 "${ACTUAL_JSON}" commands)
    assert_json_equal("seed" 42 "${ACTUAL_JSON}" seed)
    assert_json_equal("digest executed commands" 1 "${ACTUAL_JSON}" digest executed_commands_count)
    assert_json_equal("digest success" ON "${ACTUAL_JSON}" digest success)
    assert_json_equal("digest fault" "" "${ACTUAL_JSON}" digest fault_code)
    foreach(FIELD repository_content_hash script_set_hash digest_hash)
        json_value(TOP_VALUE "${ACTUAL_JSON}" ${FIELD})
        assert_sha256("top-level ${FIELD}" "${TOP_VALUE}")
        assert_json_equal("digest.${FIELD} consistency" "${TOP_VALUE}" "${ACTUAL_JSON}" digest ${FIELD})
    endforeach()
    json_value(STATE_HASH "${ACTUAL_JSON}" digest state_hash)
    assert_sha256("digest.state_hash" "${STATE_HASH}")
elseif(GV2_MODE STREQUAL "LIVE_CHECK_SCRIPTS_SMOKE")
    assert_json_equal("status" ok "${ACTUAL_JSON}" status)
    foreach(FIELD repository_content_hash script_set_hash)
        json_value(VALUE "${ACTUAL_JSON}" ${FIELD})
        assert_sha256("${FIELD}" "${VALUE}")
    endforeach()
    json_value(MODULES_CHECKED "${ACTUAL_JSON}" modules_checked)
    if(NOT MODULES_CHECKED MATCHES "^[0-9]+$" OR NOT MODULES_CHECKED GREATER 0)
        message(FATAL_ERROR "modules_checked must be a positive integer, got '${MODULES_CHECKED}'")
    endif()
else()
    message(FATAL_ERROR "Unknown GV2_MODE '${GV2_MODE}'")
endif()

message(STATUS "Headless JSON contract satisfied: ${GV2_MODE}")
