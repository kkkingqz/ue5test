set(ASSERT_PROCESS_SCRIPT "${PROJECT_SOURCE_DIR}/Tools/Testing/AssertProcess.cmake")

function(expect_assert_process_rejection
    CASE_NAME
    EXPECTED_FAILURE_REGEX
    PROGRAM
    ARGUMENTS
    EXPECTED_EXIT_CODE
    EXPECTED_OUTPUT_REGEX)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            "-DGV2_PROGRAM=${PROGRAM}"
            "-DGV2_ARGUMENTS=${ARGUMENTS}"
            "-DGV2_EXPECTED_EXIT_CODE=${EXPECTED_EXIT_CODE}"
            "-DGV2_EXPECTED_OUTPUT_REGEX=${EXPECTED_OUTPUT_REGEX}"
            "-DGV2_WORKING_DIRECTORY=${PROJECT_SOURCE_DIR}"
            -P "${ASSERT_PROCESS_SCRIPT}"
        RESULT_VARIABLE SELF_TEST_RESULT
        OUTPUT_VARIABLE SELF_TEST_STDOUT
        ERROR_VARIABLE SELF_TEST_STDERR
    )
    set(SELF_TEST_OUTPUT "${SELF_TEST_STDOUT}\n${SELF_TEST_STDERR}")
    if(SELF_TEST_RESULT EQUAL 0)
        message(FATAL_ERROR "${CASE_NAME}: AssertProcess unexpectedly accepted an invalid contract")
    endif()
    if(NOT SELF_TEST_OUTPUT MATCHES "${EXPECTED_FAILURE_REGEX}")
        message(FATAL_ERROR
            "${CASE_NAME}: unexpected rejection.\n"
            "stdout:\n${SELF_TEST_STDOUT}\n"
            "stderr:\n${SELF_TEST_STDERR}")
    endif()
endfunction()

expect_assert_process_rejection(
    wrong_exit_code
    "Expected exit code 9, got 0"
    "${CMAKE_COMMAND}"
    "-E;echo;expected_marker"
    9
    expected_marker
)

expect_assert_process_rejection(
    wrong_output_marker
    "Expected output regex did not match"
    "${CMAKE_COMMAND}"
    "-E;echo;actual_marker"
    0
    missing_marker
)

expect_assert_process_rejection(
    launch_failure
    "Process did not exit normally"
    "${PROJECT_SOURCE_DIR}/Tools/Testing/does-not-exist"
    unused
    9
    unused
)

message(STATUS "AssertProcess rejection self-test passed")
