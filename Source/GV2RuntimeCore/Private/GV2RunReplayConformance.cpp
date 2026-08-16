#include "GV2RuntimeCore/Testing/GV2RunReplayConformance.h"

#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentCore/RepositoryBuilder.h"
#include "GV2RuntimeCore/GV2RunReplay.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace GV2RuntimeCore::Testing
{
namespace
{
class FReplayInMemoryContentProvider final : public GV2ContentCore::IContentSourceProvider
{
public:
    std::map<std::string, std::string> Files;

    std::optional<std::string> ReadSource(
        std::string_view PackageId,
        std::string_view RelativeSource) const override
    {
        const std::string Key = std::string(PackageId) + "/" + std::string(RelativeSource);
        const auto Found = Files.find(Key);
        return Found == Files.end() ? std::nullopt : std::optional<std::string>(Found->second);
    }
};

std::vector<FRuntimeSource> CreateReplayRuntimeSources()
{
    std::vector<FRuntimeSource> Sources;
    Sources.push_back({
        "@core/bootstrap/manifest.lua",
        "return {\n"
        "    entry_module_id = 'core:module.bootstrap.main',\n"
        "    modules = {\n"
        "        {\n"
        "            module_id = 'core:module.bootstrap.main',\n"
        "            source = 'bootstrap/main.lua',\n"
        "            dependencies = {}\n"
        "        }\n"
        "    }\n"
        "}\n"
    });
    Sources.push_back({
        "@core/bootstrap/main.lua",
        "game.runtime = {\n"
        "    dispatch_command = function(request)\n"
        "        return request.sequence\n"
        "    end\n"
        "}\n"
        "return {}\n"
    });
    return Sources;
}
} // namespace

std::string RunRunReplayConformance()
{
    // Build minimal in-memory repository
    FReplayInMemoryContentProvider Provider;
    Provider.Files["core/schemas/item.json5"] =
        "{ id: 'core:schema.definition.item.v1', definition_type: 'item', schema_version: 1, "
        "root: { kind: 'object', fields: {} }, semantic_validators: [], extensions: {} }";
    Provider.Files["core/definitions/items.json5"] =
        "{ schema_version: 1, type: 'item', definitions: [] }";

    const GV2ContentCore::FPackageDescriptor Descriptor(
        "core", "core", 0,
        { "definitions/items.json5" },
        { GV2ContentCore::FSchemaBinding("item", 1, "core:schema.definition.item.v1", "schemas/item.json5") });

    GV2ContentCore::FBuildOptions Options;
    Options.SourceProvider = &Provider;
    const auto BuildResult = GV2ContentCore::BuildRepository({ Descriptor }, Options);
    if (!BuildResult.IsSuccess())
    {
        return "run_replay.build_repository_failed";
    }

    const auto ReadHandle = BuildResult.GetCandidate().GetReadHandle();
    const auto Sources = CreateReplayRuntimeSources();

    // 1. Negative: Lua release mismatch
    FRunManifest MismatchReleaseManifest;
    MismatchReleaseManifest.LuaReleaseNumber = 99999;
    MismatchReleaseManifest.RepositoryContentHash = ReadHandle.GetContentHash();
    MismatchReleaseManifest.Seed = 1;

    FRunResult Result;
    FRuntimeFault Fault;
    if (ReplayRunManifest(MismatchReleaseManifest, ReadHandle, Sources, Result, Fault)
        || Fault.Code != "core:fault.run_manifest.lua_release_mismatch")
    {
        return "run_replay.lua_release_mismatch_rejected";
    }

    // 2. Negative: Repository content hash mismatch
    FRunManifest MismatchHashManifest;
    MismatchHashManifest.LuaReleaseNumber = FRuntimeSession::LuaReleaseNumber;
    MismatchHashManifest.RepositoryContentHash = "0000000000000000000000000000000000000000000000000000000000000000";
    MismatchHashManifest.Seed = 1;

    if (ReplayRunManifest(MismatchHashManifest, ReadHandle, Sources, Result, Fault)
        || Fault.Code != "core:fault.run_manifest.repository_hash_mismatch")
    {
        return "run_replay.repository_hash_mismatch_rejected";
    }

    // 3. Positive: Replay executed successfully
    FRunManifest ValidManifest;
    ValidManifest.LuaReleaseNumber = FRuntimeSession::LuaReleaseNumber;
    ValidManifest.RepositoryContentHash = ReadHandle.GetContentHash();
    ValidManifest.Seed = 42;

    FRunAcceptedCommand Cmd1;
    Cmd1.CommandId = "core:command.test.step";
    Cmd1.Sequence = 1;
    Cmd1.Args.emplace("step", FValue(static_cast<std::int64_t>(1)));

    FRunAcceptedCommand Cmd2;
    Cmd2.CommandId = "core:command.test.step";
    Cmd2.Sequence = 2;
    Cmd2.Args.emplace("step", FValue(static_cast<std::int64_t>(2)));

    ValidManifest.AcceptedCommands.push_back(std::move(Cmd1));
    ValidManifest.AcceptedCommands.push_back(std::move(Cmd2));

    if (!ReplayRunManifest(ValidManifest, ReadHandle, Sources, Result, Fault)
        || !Result.bSuccess
        || Result.ExecutedCommandsCount != 2)
    {
        return "run_replay.valid_replay_failed";
    }

    const FRunDigest Digest = ComputeRunDigest(ValidManifest, Result);
    if (Digest.DigestHash.length() != 64 || !Digest.bSuccess || Digest.ExecutedCommandsCount != 2)
    {
        return "run_replay.digest_computation_failed";
    }

    return "";
}
} // namespace GV2RuntimeCore::Testing
