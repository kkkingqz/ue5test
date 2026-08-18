#include "GV2TestSupport/CommandValidatorFixture.h"

#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentCore/RepositoryBuilder.h"
#include "GV2ContentCore/RepositorySnapshot.h"

#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace GV2TestSupport
{
namespace
{
std::optional<std::string> ReadFileToString(const std::filesystem::path& FilePath)
{
    std::ifstream Stream(FilePath, std::ios::binary);
    if (!Stream)
    {
        return std::nullopt;
    }
    return std::string(
        (std::istreambuf_iterator<char>(Stream)),
        std::istreambuf_iterator<char>());
}

// GameDataRepositoryContract.md "Empty core descriptor создаёт допустимый
// пустой snapshot". This fixture exercises dispatch/validator plumbing,
// not content.
GV2ContentCore::FRepositoryReadHandle MakeEmptyRepository()
{
    const GV2ContentCore::FPackageDescriptor EmptyCore("core", "core", 0u, {}, {});
    GV2ContentCore::FBuildOptions Options;
    GV2ContentCore::FBuildResult Result = GV2ContentCore::BuildRepository({EmptyCore}, Options);
    if (Result.IsFailure())
    {
        return {};
    }
    return Result.GetCandidate().GetReadHandle();
}
} // namespace

bool StartCommandValidatorFixtureSession(
    const std::filesystem::path& ScriptsRoot,
    const std::filesystem::path& FixtureRoot,
    GV2RuntimeCore::FRuntimeSession& OutSession,
    GV2RuntimeCore::FRuntimeFault& OutFault)
{
    const GV2ContentCore::FRepositoryReadHandle RepoHandle = MakeEmptyRepository();
    if (!RepoHandle.IsValid())
    {
        OutFault = {"CommandValidatorFixtureRepositoryFailed", "Failed to build the empty fixture repository."};
        return false;
    }

    struct FRealModule
    {
        const char* RelativePath;
        const char* ChunkName;
    };
    const std::vector<FRealModule> RealModules = {
        {"runtime/mutation_window.lua", "@core/runtime/mutation_window.lua"},
        {"runtime/portable_value.lua", "@core/runtime/portable_value.lua"},
        {"runtime/stable_id.lua", "@core/runtime/stable_id.lua"},
        {"runtime/validator_registry.lua", "@core/runtime/validator_registry.lua"},
        {"runtime/handler_registry.lua", "@core/runtime/handler_registry.lua"},
        {"runtime/event_envelope.lua", "@core/runtime/event_envelope.lua"},
        {"runtime/subscriber_registry.lua", "@core/runtime/subscriber_registry.lua"},
        {"runtime/event_bus.lua", "@core/runtime/event_bus.lua"},
        {"runtime/command_dispatcher.lua", "@core/runtime/command_dispatcher.lua"},
    };

    std::vector<GV2RuntimeCore::FRuntimeSource> Sources;

    const std::optional<std::string> ManifestSource = ReadFileToString(FixtureRoot / "manifest.lua");
    if (!ManifestSource.has_value())
    {
        OutFault = {
            "CommandValidatorFixtureSourceMissing",
            "Failed to read fixture manifest: " + (FixtureRoot / "manifest.lua").string()};
        return false;
    }
    Sources.push_back({"@core/bootstrap/manifest.lua", *ManifestSource});

    for (const FRealModule& Module : RealModules)
    {
        const std::optional<std::string> Source = ReadFileToString(ScriptsRoot / Module.RelativePath);
        if (!Source.has_value())
        {
            OutFault = {
                "CommandValidatorFixtureSourceMissing",
                "Failed to read real module: " + (ScriptsRoot / Module.RelativePath).string()};
            return false;
        }
        Sources.push_back({Module.ChunkName, *Source});
    }

    const std::optional<std::string> DriverSource = ReadFileToString(FixtureRoot / "driver.lua");
    if (!DriverSource.has_value())
    {
        OutFault = {
            "CommandValidatorFixtureSourceMissing",
            "Failed to read fixture driver: " + (FixtureRoot / "driver.lua").string()};
        return false;
    }
    Sources.push_back({"@core/test/command_validator_specs_driver.lua", *DriverSource});

    return OutSession.Start(1, RepoHandle, Sources, OutFault);
}
} // namespace GV2TestSupport
