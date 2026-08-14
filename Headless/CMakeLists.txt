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

set(GV2_CONTENT_CORE_DIR "${PROJECT_SOURCE_DIR}/Source/GV2ContentCore")

add_library(gv2_content_core STATIC
    "${GV2_CONTENT_CORE_DIR}/Private/BuildResult.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/CanonicalHash.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/DefinitionEnvelope.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/Diagnostic.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/ExtensionSchema.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/FieldValidation.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/GV2ContentCore.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/Json5Lexer.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/Json5Conformance.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/Json5Parser.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/PackageDescriptor.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/PackageDiscovery.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/ParseLimits.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/RepositoryBuilder.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/RepositorySnapshot.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/RepresentativeCore.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/SchemaRegistry.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/SemanticValidation.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/ScalarValidation.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/StableId.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/Value.cpp"
)
target_compile_features(gv2_content_core PUBLIC cxx_std_20)
target_include_directories(gv2_content_core
    PUBLIC "${GV2_CONTENT_CORE_DIR}/Public"
)

target_link_libraries(gv2_runtime_core PUBLIC gv2_content_core)

add_executable(gv2-headless Source/main.cpp)
target_compile_features(gv2-headless PRIVATE cxx_std_20)
target_link_libraries(gv2-headless PRIVATE gv2_runtime_core gv2_content_core)

add_test(NAME gv2_headless_self_test COMMAND gv2-headless --self-test)
set_tests_properties(gv2_headless_self_test PROPERTIES
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

# PCC-35: missing/invalid repository content must block session startup
# before Lua bootstrap even runs.
add_test(NAME gv2_headless_rejects_missing_content_root
    COMMAND gv2-headless --content-root=${PROJECT_SOURCE_DIR}/Tests/Fixtures/PortableContentCore/does_not_exist
)
add_test(NAME gv2_headless_rejects_invalid_content_root
    COMMAND gv2-headless --content-root=${PROJECT_SOURCE_DIR}/Tests/Fixtures/PortableContentCore/invalid/duplicate_key/core
)
set_tests_properties(
    gv2_headless_rejects_missing_content_root
    PROPERTIES
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        PASS_REGULAR_EXPRESSION "core:diagnostic.package.discovery.root_not_found"
)
set_tests_properties(
    gv2_headless_rejects_invalid_content_root
    PROPERTIES
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        PASS_REGULAR_EXPRESSION "core:diagnostic.json5.duplicate_key"
)
