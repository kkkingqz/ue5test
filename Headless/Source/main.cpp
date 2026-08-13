#include "GV2RuntimeCore/GV2HostServices.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

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

bool LoadRuntimeSources(
    const char* ExecutableArgument,
    std::vector<GV2RuntimeCore::FRuntimeSource>& OutSources)
{
    std::vector<std::filesystem::path> ScriptDirectories{
        std::filesystem::current_path() / "Scripts",
        std::filesystem::current_path() / ".." / "Scripts",
    };
    std::error_code PathError;
    const std::filesystem::path ExecutablePath = std::filesystem::absolute(
        ExecutableArgument,
        PathError);
    if (!PathError)
    {
        ScriptDirectories.push_back(
            ExecutablePath.parent_path().parent_path().parent_path() / "Scripts");
    }

    for (const std::filesystem::path& Directory : ScriptDirectories)
    {
        std::error_code IterationError;
        if (!std::filesystem::is_directory(Directory, IterationError) || IterationError)
        {
            continue;
        }

        std::vector<std::filesystem::path> SourcePaths;
        for (std::filesystem::recursive_directory_iterator Iterator(Directory, IterationError), End;
             !IterationError && Iterator != End;
             Iterator.increment(IterationError))
        {
            if (Iterator->is_regular_file() && Iterator->path().extension() == ".lua")
            {
                SourcePaths.emplace_back(Iterator->path());
            }
        }
        if (IterationError || SourcePaths.empty())
        {
            continue;
        }
        std::sort(SourcePaths.begin(), SourcePaths.end());

        std::vector<GV2RuntimeCore::FRuntimeSource> Candidate;
        Candidate.reserve(SourcePaths.size());
        for (const std::filesystem::path& SourcePath : SourcePaths)
        {
            std::ifstream Stream(SourcePath, std::ios::binary);
            if (!Stream)
            {
                Candidate.clear();
                break;
            }
            std::string Text{
                std::istreambuf_iterator<char>(Stream),
                std::istreambuf_iterator<char>()};
            if (Text.starts_with("\xef\xbb\xbf"))
            {
                Text.erase(0, 3);
            }
            const std::string RelativePath = std::filesystem::relative(SourcePath, Directory).generic_string();
            Candidate.push_back({std::string("@Scripts/") + RelativePath, std::move(Text)});
        }
        if (!Candidate.empty())
        {
            OutSources = std::move(Candidate);
            return true;
        }
    }
    return false;
}

int Run(
    const std::int64_t CommandCount,
    const std::int64_t Seed,
    const bool bSelfTest,
    const std::vector<GV2RuntimeCore::FRuntimeSource>& RuntimeSources)
{
    GV2RuntimeCore::FRuntimeSession Runtime;
    GV2RuntimeCore::FRuntimeFault Fault;
    if (!Runtime.Start(1, RuntimeSources, Fault))
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

        GV2RuntimeCore::FCommandRequest StartRequest;
        StartRequest.CommandId = "core:command.debug.start";
        StartRequest.Sequence = CommandCount + 2;
        std::optional<GV2RuntimeCore::FScreenRequest> PendingScreen;
        if (!Runtime.DispatchCommand(StartRequest, Fault)
            || !Runtime.TakePendingScreen(PendingScreen, Fault)
            || !PendingScreen
            || PendingScreen->ScreenId != "core:screen.test"
            || PendingScreen->Fields.size() != 5
            || PendingScreen->Fields[0].FieldId != "buttons"
            || PendingScreen->Fields[1].FieldId != "checkbox"
            || PendingScreen->Fields[2].FieldId != "class_select"
            || PendingScreen->Fields[3].FieldId != "description"
            || PendingScreen->Fields[4].FieldId != "player_name")
        {
            std::cerr << "debug_start_flow_failed code=" << Fault.Code
                      << " message=" << Fault.Message << '\n';
            return 7;
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

    std::vector<GV2RuntimeCore::FRuntimeSource> RuntimeSources;
    if (!LoadRuntimeSources(argv[0], RuntimeSources))
    {
        std::cerr << "unable to locate a non-empty Scripts module tree\n";
        return 66;
    }
    return Run(CommandCount, Seed, bSelfTest, RuntimeSources);
}
