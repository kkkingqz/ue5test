set(GV2_RUNTIME_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/GV2RuntimeCore")
set(GV2_LUA_DIR "${GV2_RUNTIME_CORE_DIR}/Private/ThirdParty/Lua54")

file(GLOB GV2_LUA_SOURCES CONFIGURE_DEPENDS "${GV2_LUA_DIR}/*.c")

add_library(gv2_runtime_core STATIC
    "${GV2_RUNTIME_CORE_DIR}/Private/GV2LuaMarshaller.cpp"
    "${GV2_RUNTIME_CORE_DIR}/Private/GV2LuaRepositoryConformance.cpp"
    "${GV2_RUNTIME_CORE_DIR}/Private/GV2RunDigest.cpp"
    "${GV2_RUNTIME_CORE_DIR}/Private/GV2RunDigestConformance.cpp"
    "${GV2_RUNTIME_CORE_DIR}/Private/GV2RunManifest.cpp"
    "${GV2_RUNTIME_CORE_DIR}/Private/GV2RunManifestConformance.cpp"
    "${GV2_RUNTIME_CORE_DIR}/Private/GV2RunReplay.cpp"
    "${GV2_RUNTIME_CORE_DIR}/Private/GV2RunReplayConformance.cpp"
    "${GV2_RUNTIME_CORE_DIR}/Private/GV2RuntimeSession.cpp"
    "${GV2_RUNTIME_CORE_DIR}/Private/GV2SaveSlotStorage.cpp"
    "${GV2_RUNTIME_CORE_DIR}/Private/GV2SaveSlotStorageConformance.cpp"
    "${GV2_RUNTIME_CORE_DIR}/Private/GV2ColdStartLoadConformance.cpp"
    "${GV2_RUNTIME_CORE_DIR}/Private/GV2ValidatorRegistryConformance.cpp"
    "${GV2_RUNTIME_CORE_DIR}/Private/GV2LuaSpecRunnerConformance.cpp"
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

set(GV2_CONTENT_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/GV2ContentCore")

add_library(gv2_content_core STATIC
    "${GV2_CONTENT_CORE_DIR}/Private/BuildResult.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/BuildResultConformance.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/CanonicalHash.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/ContainerValidationConformance.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/DefinitionEnvelope.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/DefinitionEnvelopeConformance.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/Diagnostic.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/DiagnosticModelConformance.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/ExtensionSchema.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/ExtensionSchemaConformance.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/FieldValidation.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/GV2ContentCore.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/Json5Lexer.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/Json5LexerConformance.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/Json5Conformance.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/Json5Parser.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/Json5ParserConformance.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/PackageDescriptor.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/PackageDescriptorConformance.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/ParseLimits.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/ParseLimitsConformance.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/PoParser.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/PoParserConformance.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/PresenceDefaultConformance.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/RepositoryBuilder.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/RepositorySnapshot.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/RepresentativeCore.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/SchemaRegistry.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/SchemaRegistryConformance.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/SemanticValidation.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/ScalarValidation.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/ScalarValidationConformance.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/SpecialFieldValidationConformance.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/StableId.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/Value.cpp"
    "${GV2_CONTENT_CORE_DIR}/Private/ValueModelConformance.cpp"
)
target_compile_features(gv2_content_core PUBLIC cxx_std_20)
target_include_directories(gv2_content_core
    PUBLIC "${GV2_CONTENT_CORE_DIR}/Public"
)

target_link_libraries(gv2_runtime_core PUBLIC gv2_content_core)

# ADR-0019: host-side package discovery (std::filesystem-based directory
# scan) is deliberately NOT part of gv2_content_core/GV2ContentCore, whose
# public API and shared sources must stay filesystem-free (ADR-0018). This
# library is the one shared implementation used by gv2-content, gv2-headless
# and the UE host adapter.
set(GV2_CONTENT_HOST_SUPPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/GV2ContentHostSupport")

add_library(gv2_content_host_support STATIC
    "${GV2_CONTENT_HOST_SUPPORT_DIR}/Private/PackageDiscovery.cpp"
    "${GV2_CONTENT_HOST_SUPPORT_DIR}/Private/ModsLock.cpp"
    "${GV2_CONTENT_HOST_SUPPORT_DIR}/Private/PackageManifestConformance.cpp"
    "${GV2_CONTENT_HOST_SUPPORT_DIR}/Private/PackageDiscoveryAndOrderConformance.cpp"
    "${GV2_CONTENT_HOST_SUPPORT_DIR}/Private/LocalizationDiscovery.cpp"
    "${GV2_CONTENT_HOST_SUPPORT_DIR}/Private/LuaSpecDiscovery.cpp"
    "${GV2_CONTENT_HOST_SUPPORT_DIR}/Private/LuaSpecIdentity.cpp"
)
target_compile_features(gv2_content_host_support PUBLIC cxx_std_20)
target_include_directories(gv2_content_host_support
    PUBLIC "${GV2_CONTENT_HOST_SUPPORT_DIR}/Public"
)
target_link_libraries(gv2_content_host_support PUBLIC gv2_content_core)

# TAS-04: the one module both hosts call to run Tests/Lua specs. It needs
# BOTH filesystem discovery (gv2_content_host_support) and the Lua VM
# (gv2_runtime_core); no existing target already depends on both, and
# ADR-0024 wants exactly one shared implementation, not host-local
# duplication. Test-only — no gameplay executable links this library.
set(GV2_TEST_SUPPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/GV2TestSupport")

add_library(gv2_test_support STATIC
    "${GV2_TEST_SUPPORT_DIR}/Private/LuaSpecRunner.cpp"
    "${GV2_TEST_SUPPORT_DIR}/Private/CommandValidatorFixture.cpp"
)
target_compile_features(gv2_test_support PUBLIC cxx_std_20)
target_include_directories(gv2_test_support
    PUBLIC "${GV2_TEST_SUPPORT_DIR}/Public"
)
target_link_libraries(gv2_test_support PUBLIC gv2_content_host_support gv2_runtime_core)
