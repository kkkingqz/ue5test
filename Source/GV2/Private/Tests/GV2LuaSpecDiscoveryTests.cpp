#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GV2ContentHostSupport/LuaSpecDiscovery.h"

#include <filesystem>
#include <fstream>

namespace
{
// Creates <base>/<Name> and writes Content into it, creating parent
// directories as needed.
void WriteScratchFile(const std::filesystem::path& Base, const char* RelativePath, const char* Content)
{
    const std::filesystem::path FilePath = Base / RelativePath;
    std::filesystem::create_directories(FilePath.parent_path());
    std::ofstream Stream(FilePath, std::ios::binary);
    Stream << Content;
}
} // namespace

// TAS-02: Tests/Lua spec discovery — recursive, deterministic, tolerant of
// a missing root directory.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LuaSpecDiscoveryTest,
    "GV2.Runtime.ContentHostSupport.LuaSpecDiscovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LuaSpecDiscoveryTest::RunTest(const FString& Parameters)
{
    const std::filesystem::path Root =
        std::filesystem::temp_directory_path() / "gv2_lua_spec_discovery_test";
    std::error_code Ec;
    std::filesystem::remove_all(Root, Ec);

    // 1. A missing root is not an error: empty result.
    {
        const std::vector<GV2ContentHostSupport::FLuaSpecFile> Files =
            GV2ContentHostSupport::DiscoverLuaSpecFiles(Root);
        if (!TestTrue(TEXT("missing root yields an empty, non-error result"), Files.empty()))
        {
            return false;
        }
    }

    // 2. Recursive discovery, non-.lua files ignored, deterministic
    // ascending order by relative path regardless of creation order.
    WriteScratchFile(Root, "z_top.lua", "return { a = function() end }");
    WriteScratchFile(Root, "world/current_location.lua", "return { a = function() end }");
    WriteScratchFile(Root, "world/README.md", "not a spec");
    WriteScratchFile(Root, "a_top.lua", "return { a = function() end }");

    const std::vector<GV2ContentHostSupport::FLuaSpecFile> Files =
        GV2ContentHostSupport::DiscoverLuaSpecFiles(Root);

    std::filesystem::remove_all(Root, Ec);

    if (!TestEqual(TEXT("discovers exactly the three .lua files, ignoring README.md"), Files.size(), std::size_t(3)))
    {
        return false;
    }
    TestEqual(TEXT("sorted ascending: a_top.lua first"), FString(Files[0].RelativePath.c_str()), FString("a_top.lua"));
    TestEqual(
        TEXT("sorted ascending: world/current_location.lua second"),
        FString(Files[1].RelativePath.c_str()),
        FString("world/current_location.lua"));
    TestEqual(TEXT("sorted ascending: z_top.lua last"), FString(Files[2].RelativePath.c_str()), FString("z_top.lua"));
    TestTrue(TEXT("file content is read"), Files[0].Source.find("return") != std::string::npos);

    return true;
}

#endif
