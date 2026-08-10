#include "GV2RuntimeCore/GV2HostServices.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <string>

namespace
{
class FMetadataOnlyResourceCatalog final : public GV2RuntimeCore::IResourceCatalog
{
public:
    FMetadataOnlyResourceCatalog()
    {
        GV2RuntimeCore::FResourceMetadata Metadata;
        Metadata.ResourceId = "core:resource.image.headless_fixture";
        Metadata.Kind = GV2RuntimeCore::EResourceKind::Image;
        Metadata.bAvailable = true;
        Resources.emplace(Metadata.ResourceId, std::move(Metadata));
    }

    std::optional<GV2RuntimeCore::FResourceMetadata> FindMetadata(
        const std::string& ResourceId) const override
    {
        const auto Found = Resources.find(ResourceId);
        return Found != Resources.end()
            ? std::optional<GV2RuntimeCore::FResourceMetadata>(Found->second)
            : std::nullopt;
    }

private:
    std::map<std::string, GV2RuntimeCore::FResourceMetadata, std::less<>> Resources;
};

class FUnresolvedLocalizationAdapter final : public GV2RuntimeCore::ILocalizationAdapter
{
public:
    std::optional<std::string> Resolve(
        const GV2RuntimeCore::FTextSpec&,
        const std::string&) const override
    {
        return std::nullopt;
    }
};

bool TryParsePositive(const std::string& Text, std::int64_t& OutValue)
{
    const char* Begin = Text.data();
    const char* End = Begin + Text.size();
    const auto Result = std::from_chars(Begin, End, OutValue);
    return Result.ec == std::errc{} && Result.ptr == End && OutValue > 0;
}

int Run(const std::int64_t CommandCount, const std::int64_t Seed, const bool bSelfTest)
{
    GV2RuntimeCore::FRuntimeSession Runtime;
    GV2RuntimeCore::FRuntimeFault Fault;
    if (!Runtime.StartTestRuntime(1, Fault))
    {
        std::cerr << "runtime_start_failed code=" << Fault.Code << " message=" << Fault.Message << '\n';
        return 2;
    }

    FMetadataOnlyResourceCatalog Resources;
    const auto Resource = Resources.FindMetadata("core:resource.image.headless_fixture");
    if (!Resource || !Resource->bAvailable)
    {
        std::cerr << "metadata_resource_catalog_failed\n";
        return 3;
    }

    FUnresolvedLocalizationAdapter Localization;
    GV2RuntimeCore::FTextSpec Text;
    Text.TextId = "core:text.headless.fixture";
    if (Localization.Resolve(Text, "").has_value())
    {
        std::cerr << "headless_localization_should_remain_unresolved\n";
        return 4;
    }

    const auto Started = std::chrono::steady_clock::now();
    for (std::int64_t Index = 1; Index <= CommandCount; ++Index)
    {
        GV2RuntimeCore::FCommandRequest Request;
        Request.CommandId = "core:command.test.headless_step";
        Request.Sequence = Index;
        Request.Args.emplace("seed", GV2RuntimeCore::FValue(Seed));
        Request.Args.emplace("step", GV2RuntimeCore::FValue(Index));
        if (!Runtime.DispatchCommand(Request, Fault))
        {
            std::cerr << "command_failed sequence=" << Index
                      << " code=" << Fault.Code
                      << " message=" << Fault.Message << '\n';
            return 5;
        }
    }
    const auto Finished = std::chrono::steady_clock::now();
    const double Seconds = std::chrono::duration<double>(Finished - Started).count();
    const double CommandsPerSecond = Seconds > 0.0
        ? static_cast<double>(CommandCount) / Seconds
        : 0.0;

    if (bSelfTest)
    {
        GV2RuntimeCore::FSemanticInput Input;
        Input.SessionGeneration = 1;
        Input.UiInstanceId = "ui@1:1";
        Input.Revision = 1;
        Input.Sequence = CommandCount + 1;
        Input.NodeKeyPath = {"route", "button"};
        Input.CommandId = "core:command.test.semantic_step";
        if (!Runtime.DispatchSemanticInput(Input, Fault))
        {
            std::cerr << "semantic_input_failed code=" << Fault.Code
                      << " message=" << Fault.Message << '\n';
            return 6;
        }
    }

    std::cout << "{\"ok\":true,\"lua_release_num\":"
              << GV2RuntimeCore::FRuntimeSession::LuaReleaseNumber
              << ",\"commands\":" << CommandCount
              << ",\"seed\":" << Seed
              << ",\"commands_per_second\":" << CommandsPerSecond
              << ",\"media_payload_loaded\":false"
              << ",\"localization_resolved\":false}\n";
    return 0;
}
}

int main(int argc, char** argv)
{
    std::int64_t CommandCount = 1000;
    std::int64_t Seed = 1;
    bool bSelfTest = false;

    for (int Index = 1; Index < argc; ++Index)
    {
        const std::string Argument = argv[Index];
        if (Argument == "--self-test")
        {
            bSelfTest = true;
        }
        else if (Argument.rfind("--commands=", 0) == 0)
        {
            if (!TryParsePositive(Argument.substr(11), CommandCount))
            {
                std::cerr << "invalid --commands value\n";
                return 64;
            }
        }
        else if (Argument.rfind("--seed=", 0) == 0)
        {
            if (!TryParsePositive(Argument.substr(7), Seed))
            {
                std::cerr << "invalid --seed value\n";
                return 64;
            }
        }
        else
        {
            std::cerr << "unknown argument: " << Argument << '\n';
            return 64;
        }
    }

    return Run(CommandCount, Seed, bSelfTest);
}
