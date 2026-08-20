include_guard(GLOBAL)

function(gv2_add_cli_contract_test)
    cmake_parse_arguments(
        GV2_TEST
        ""
        "NAME;TARGET;EXPECTED_EXIT_CODE;EXPECTED_OUTPUT_REGEX;WORKING_DIRECTORY"
        "ARGS"
        ${ARGN}
    )

    foreach(REQUIRED_ARGUMENT NAME TARGET EXPECTED_EXIT_CODE EXPECTED_OUTPUT_REGEX)
        if(NOT DEFINED GV2_TEST_${REQUIRED_ARGUMENT} OR GV2_TEST_${REQUIRED_ARGUMENT} STREQUAL "")
            message(FATAL_ERROR "gv2_add_cli_contract_test requires ${REQUIRED_ARGUMENT}")
        endif()
    endforeach()

    if(NOT TARGET ${GV2_TEST_TARGET})
        message(FATAL_ERROR "gv2_add_cli_contract_test target does not exist: ${GV2_TEST_TARGET}")
    endif()

    if(NOT DEFINED GV2_TEST_WORKING_DIRECTORY OR GV2_TEST_WORKING_DIRECTORY STREQUAL "")
        set(GV2_TEST_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    endif()

    list(JOIN GV2_TEST_ARGS ";" GV2_TEST_ARGUMENTS)
    add_test(
        NAME ${GV2_TEST_NAME}
        COMMAND
            "${CMAKE_COMMAND}"
            "-DGV2_PROGRAM=$<TARGET_FILE:${GV2_TEST_TARGET}>"
            "-DGV2_ARGUMENTS=${GV2_TEST_ARGUMENTS}"
            "-DGV2_EXPECTED_EXIT_CODE=${GV2_TEST_EXPECTED_EXIT_CODE}"
            "-DGV2_EXPECTED_OUTPUT_REGEX=${GV2_TEST_EXPECTED_OUTPUT_REGEX}"
            "-DGV2_WORKING_DIRECTORY=${GV2_TEST_WORKING_DIRECTORY}"
            -P "${PROJECT_SOURCE_DIR}/Tools/Testing/AssertProcess.cmake"
    )
endfunction()
