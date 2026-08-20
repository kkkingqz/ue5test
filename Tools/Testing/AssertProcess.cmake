foreach(REQUIRED_VARIABLE
    GV2_PROGRAM
    GV2_EXPECTED_EXIT_CODE
    GV2_EXPECTED_OUTPUT_REGEX
    GV2_WORKING_DIRECTORY)
    if(NOT DEFINED ${REQUIRED_VARIABLE} OR "${${REQUIRED_VARIABLE}}" STREQUAL "")
        message(FATAL_ERROR "AssertProcess.cmake requires ${REQUIRED_VARIABLE}")
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

set(PROCESS_OUTPUT "${PROCESS_STDOUT}\n${PROCESS_STDERR}")

# A normal child exit is numeric. CMake reports launch failures, signals and
# timeouts as descriptive strings; none of them may satisfy an expected CLI
# failure contract.
if(NOT "${ACTUAL_RESULT}" MATCHES "^-?[0-9]+$")
    message(FATAL_ERROR
        "Process did not exit normally (result=${ACTUAL_RESULT}).\n"
        "stdout:\n${PROCESS_STDOUT}\n"
        "stderr:\n${PROCESS_STDERR}")
endif()

if(NOT ACTUAL_RESULT EQUAL GV2_EXPECTED_EXIT_CODE)
    message(FATAL_ERROR
        "Expected exit code ${GV2_EXPECTED_EXIT_CODE}, got ${ACTUAL_RESULT}.\n"
        "stdout:\n${PROCESS_STDOUT}\n"
        "stderr:\n${PROCESS_STDERR}")
endif()

if(NOT PROCESS_OUTPUT MATCHES "${GV2_EXPECTED_OUTPUT_REGEX}")
    message(FATAL_ERROR
        "Expected output regex did not match: ${GV2_EXPECTED_OUTPUT_REGEX}\n"
        "stdout:\n${PROCESS_STDOUT}\n"
        "stderr:\n${PROCESS_STDERR}")
endif()

message(STATUS
    "CLI contract satisfied: exit=${ACTUAL_RESULT}, output=${GV2_EXPECTED_OUTPUT_REGEX}")
