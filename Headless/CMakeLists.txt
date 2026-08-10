set(GV2_RUNTIME_CORE_DIR "${PROJECT_SOURCE_DIR}/Source/GV2RuntimeCore")
set(GV2_LUA_DIR "${GV2_RUNTIME_CORE_DIR}/Private/ThirdParty/Lua54")

file(GLOB GV2_LUA_SOURCES CONFIGURE_DEPENDS "${GV2_LUA_DIR}/*.c")

add_library(gv2_runtime_core STATIC
    "${GV2_RUNTIME_CORE_DIR}/Private/GV2RuntimeSession.cpp"
    ${GV2_LUA_SOURCES}
)

target_compile_features(gv2_runtime_core PUBLIC cxx_std_20)
target_include_directories(gv2_runtime_core
    PUBLIC "${GV2_RUNTIME_CORE_DIR}/Public"
    PRIVATE "${GV2_LUA_DIR}"
)

if(WIN32)
    target_compile_definitions(gv2_runtime_core PRIVATE LUA_USE_WINDOWS=1)
elseif(UNIX)
    target_compile_definitions(gv2_runtime_core PRIVATE LUA_USE_LINUX=1)
    target_link_libraries(gv2_runtime_core PRIVATE m dl)
endif()

add_executable(gv2-headless Source/main.cpp)
target_compile_features(gv2-headless PRIVATE cxx_std_20)
target_link_libraries(gv2-headless PRIVATE gv2_runtime_core)

add_test(NAME gv2_headless_self_test COMMAND gv2-headless --self-test)
