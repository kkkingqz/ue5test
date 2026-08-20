file(GLOB_RECURSE GV2_CMAKE_LISTS
    LIST_DIRECTORIES FALSE
    "${PROJECT_SOURCE_DIR}/CMakeLists.txt"
    "${PROJECT_SOURCE_DIR}/*/CMakeLists.txt"
)

foreach(CMAKE_LIST IN LISTS GV2_CMAKE_LISTS)
    file(READ "${CMAKE_LIST}" CONTENT)
    if(CONTENT MATCHES "WILL_FAIL[ \t\r\n]+TRUE")
        message(FATAL_ERROR
            "Weak expected-failure contract is forbidden: ${CMAKE_LIST}. "
            "Use gv2_add_cli_contract_test with an exact exit code and output marker.")
    endif()
endforeach()

message(STATUS "No CTest WILL_FAIL TRUE properties found")
