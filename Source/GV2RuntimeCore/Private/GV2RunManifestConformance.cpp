#include "GV2RuntimeCore/Testing/GV2RunManifestConformance.h"

#include "GV2RuntimeCore/GV2RunManifest.h"

#include <string>

namespace GV2RuntimeCore::Testing
{
std::string RunRunManifestConformance()
{
    // TAS-09: RepositoryContentHash values below are arbitrary opaque hex
    // strings, not pinned to any real repository (this suite tests
    // FRunManifest serialization/round-trip mechanics only) — distinct from
    // the corpus-derived hashes centralized under Tests/Fixtures/.

    // 1. Empty accepted commands round-trip
    FRunManifest EmptyManifest;
    EmptyManifest.LuaReleaseNumber = 50408;
    EmptyManifest.RepositoryContentHash = "35ed7d8000170391d46cac29a1d23534affa093312bf5eb9c73e62ccdc0ae5d8";
    EmptyManifest.ScriptSetHash = "35ed7d8000170391d46cac29a1d23534affa093312bf5eb9c73e62ccdc0ae5d8";
    EmptyManifest.Seed = 42;

    const std::string EmptyJson = SerializeRunManifest(EmptyManifest);
    if (EmptyJson.empty())
    {
        return "run_manifest.serialize_empty_manifest";
    }

    FRunManifest ParsedEmpty;
    std::string Error;
    if (!DeserializeRunManifest(EmptyJson, ParsedEmpty, Error) || !Error.empty())
    {
        return "run_manifest.deserialize_empty_manifest";
    }

    if (ParsedEmpty != EmptyManifest)
    {
        return "run_manifest.empty_manifest_equality";
    }

    // 2. Rich command payload round-trip
    FRunManifest RichManifest;
    RichManifest.LuaReleaseNumber = 50408;
    RichManifest.RepositoryContentHash = "35ed7d8000170391d46cac29a1d23534affa093312bf5eb9c73e62ccdc0ae5d8";
    RichManifest.ScriptSetHash = "35ed7d8000170391d46cac29a1d23534affa093312bf5eb9c73e62ccdc0ae5d8";
    RichManifest.Seed = 12345;

    FRunAcceptedCommand Cmd1;
    Cmd1.CommandId = "core:command.location.travel";
    Cmd1.Sequence = 1;
    Cmd1.Args.emplace("destination_id", FValue(std::string("core:location.city.market")));
    Cmd1.Args.emplace("fast_travel", FValue(true));
    Cmd1.Args.emplace("cost", FValue(static_cast<std::int64_t>(100)));

    FRunAcceptedCommand Cmd2;
    Cmd2.CommandId = "core:command.debug.ping";
    Cmd2.Sequence = 2;
    FValue::FArray Tags;
    Tags.push_back(FValue(std::string("alpha")));
    Tags.push_back(FValue(std::string("beta")));
    Cmd2.Args.emplace("tags", FValue(std::move(Tags)));

    FValue::FObject SubMeta;
    SubMeta.emplace("ratio", FValue(1.5));
    Cmd2.Args.emplace("meta", FValue(std::move(SubMeta)));

    RichManifest.AcceptedCommands.push_back(std::move(Cmd1));
    RichManifest.AcceptedCommands.push_back(std::move(Cmd2));

    const std::string RichJson1 = SerializeRunManifest(RichManifest);
    const std::string RichJson2 = SerializeRunManifest(RichManifest);
    if (RichJson1 != RichJson2)
    {
        return "run_manifest.serialization_not_deterministic";
    }

    FRunManifest ParsedRich;
    if (!DeserializeRunManifest(RichJson1, ParsedRich, Error) || !Error.empty())
    {
        return "run_manifest.deserialize_rich_manifest";
    }

    if (ParsedRich != RichManifest)
    {
        return "run_manifest.rich_manifest_equality";
    }

    // 3. Validation rejections
    FRunManifest InvalidParsed;

    // Invalid JSON
    if (DeserializeRunManifest("invalid json {{{", InvalidParsed, Error)
        || Error != "run_manifest.parse_json_failed")
    {
        return "run_manifest.reject_invalid_json";
    }

    // Missing lua_release_num
    if (DeserializeRunManifest("{ repository_content_hash: '35ed7d8000170391d46cac29a1d23534affa093312bf5eb9c73e62ccdc0ae5d8', script_set_hash: '35ed7d8000170391d46cac29a1d23534affa093312bf5eb9c73e62ccdc0ae5d8', seed: 0, accepted_commands: [] }", InvalidParsed, Error)
        || Error != "run_manifest.invalid_lua_release_num")
    {
        return "run_manifest.reject_missing_lua_release";
    }

    // Invalid hash (not 64 hex characters)
    if (DeserializeRunManifest("{ lua_release_num: 50408, repository_content_hash: 'tooshort', script_set_hash: '35ed7d8000170391d46cac29a1d23534affa093312bf5eb9c73e62ccdc0ae5d8', seed: 0, accepted_commands: [] }", InvalidParsed, Error)
        || Error != "run_manifest.invalid_repository_content_hash")
    {
        return "run_manifest.reject_invalid_hash";
    }

    // Invalid script_set_hash
    if (DeserializeRunManifest("{ lua_release_num: 50408, repository_content_hash: '35ed7d8000170391d46cac29a1d23534affa093312bf5eb9c73e62ccdc0ae5d8', script_set_hash: 'tooshort', seed: 0, accepted_commands: [] }", InvalidParsed, Error)
        || Error != "run_manifest.invalid_script_set_hash")
    {
        return "run_manifest.reject_invalid_script_set_hash";
    }

    // Invalid seed (negative)
    if (DeserializeRunManifest("{ lua_release_num: 50408, repository_content_hash: '35ed7d8000170391d46cac29a1d23534affa093312bf5eb9c73e62ccdc0ae5d8', script_set_hash: '35ed7d8000170391d46cac29a1d23534affa093312bf5eb9c73e62ccdc0ae5d8', seed: -1, accepted_commands: [] }", InvalidParsed, Error)
        || Error != "run_manifest.invalid_seed")
    {
        return "run_manifest.reject_negative_seed";
    }

    // Invalid command ID (not a valid Stable ID)
    if (DeserializeRunManifest("{ lua_release_num: 50408, repository_content_hash: '35ed7d8000170391d46cac29a1d23534affa093312bf5eb9c73e62ccdc0ae5d8', script_set_hash: '35ed7d8000170391d46cac29a1d23534affa093312bf5eb9c73e62ccdc0ae5d8', seed: 0, accepted_commands: [{ command_id: 'Invalid ID!', args: {}, sequence: 0 }] }", InvalidParsed, Error)
        || Error != "run_manifest.invalid_command_id")
    {
        return "run_manifest.reject_invalid_command_id";
    }

    // 4. Sanitation check: ensure output does not contain file system roots or absolute paths
    if (RichJson1.find("/home/") != std::string::npos
        || RichJson1.find("/Game/") != std::string::npos
        || RichJson1.find("C:\\") != std::string::npos)
    {
        return "run_manifest.contains_filesystem_path";
    }

    return "";
}
} // namespace GV2RuntimeCore::Testing
