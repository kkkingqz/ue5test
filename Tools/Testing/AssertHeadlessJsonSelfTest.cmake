foreach(REQUIRED_VARIABLE GV2_HEADLESS_PROGRAM GV2_GOLDEN_DIGEST GV2_FIXTURE_ROOT GV2_WORKING_DIRECTORY)
    if(NOT DEFINED ${REQUIRED_VARIABLE} OR "${${REQUIRED_VARIABLE}}" STREQUAL "")
        message(FATAL_ERROR "AssertHeadlessJsonSelfTest.cmake requires ${REQUIRED_VARIABLE}")
    endif()
endforeach()

set(ASSERT_SCRIPT "${GV2_WORKING_DIRECTORY}/Tools/Testing/AssertHeadlessJson.cmake")
file(READ "${GV2_GOLDEN_DIGEST}" GOLDEN_JSON)
string(JSON ORIGINAL_DIGEST_HASH GET "${GOLDEN_JSON}" digest_hash)
set(WRONG_DIGEST_HASH "0000000000000000000000000000000000000000000000000000000000000000")
string(REPLACE "${ORIGINAL_DIGEST_HASH}" "${WRONG_DIGEST_HASH}" WRONG_GOLDEN_JSON "${GOLDEN_JSON}")

set(WRONG_GOLDEN_PATH "${CMAKE_CURRENT_BINARY_DIR}/assert_headless_json_wrong_golden.json")
file(WRITE "${WRONG_GOLDEN_PATH}" "${WRONG_GOLDEN_JSON}")

execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        "-DGV2_PROGRAM=${GV2_HEADLESS_PROGRAM}"
        "-DGV2_ARGUMENTS=--commands=10;--seed=42;--content-root=${GV2_FIXTURE_ROOT}/valid/core"
        "-DGV2_MODE=GOLDEN_RUN"
        "-DGV2_GOLDEN_DIGEST=${WRONG_GOLDEN_PATH}"
        "-DGV2_WORKING_DIRECTORY=${GV2_WORKING_DIRECTORY}"
        -P "${ASSERT_SCRIPT}"
    RESULT_VARIABLE SELF_TEST_RESULT
    OUTPUT_VARIABLE SELF_TEST_STDOUT
    ERROR_VARIABLE SELF_TEST_STDERR
)
file(REMOVE "${WRONG_GOLDEN_PATH}")

set(SELF_TEST_OUTPUT "${SELF_TEST_STDOUT}\n${SELF_TEST_STDERR}")
if(SELF_TEST_RESULT EQUAL 0)
    message(FATAL_ERROR "AssertHeadlessJson accepted an incorrect golden digest")
endif()
if(NOT SELF_TEST_OUTPUT MATCHES "top-level digest_hash")
    message(FATAL_ERROR
        "AssertHeadlessJson rejected the wrong golden for an unexpected reason.\n"
        "stdout:\n${SELF_TEST_STDOUT}\nstderr:\n${SELF_TEST_STDERR}")
endif()

message(STATUS "Headless JSON contract rejection self-test passed")
