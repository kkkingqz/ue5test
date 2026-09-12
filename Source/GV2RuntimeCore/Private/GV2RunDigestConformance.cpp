#include "GV2RuntimeCore/Testing/GV2RunDigestConformance.h"

#include "GV2RuntimeCore/GV2RunDigest.h"

#include <string>

namespace GV2RuntimeCore::Testing
{
std::string RunRunDigestConformance()
{
    // TAS-09: RepositoryContentHash/StateHash values below are arbitrary
    // opaque hex strings, not pinned to any real repository (this suite
    // tests FRunDigest computation mechanics only, independent of any
    // actual content build) — distinct from the corpus-derived hashes
    // centralized under Tests/Fixtures/.

    // 1. Basic deterministic digest computation
    FRunManifest BaseManifest;
    BaseManifest.LuaReleaseNumber = 50408;
    BaseManifest.RepositoryContentHash = "35ed7d8000170391d46cac29a1d23534affa093312bf5eb9c73e62ccdc0ae5d8";
    BaseManifest.ScriptSetHash = "35ed7d8000170391d46cac29a1d23534affa093312bf5eb9c73e62ccdc0ae5d8";
    BaseManifest.Seed = 100;

    FRunAcceptedCommand Cmd1;
    Cmd1.CommandId = "core:command.location.travel";
    Cmd1.Sequence = 1;
    Cmd1.Args.emplace("target", FValue(std::string("core:location.city.market")));
    BaseManifest.AcceptedCommands.push_back(std::move(Cmd1));

    FRunResult BaseResult;
    BaseResult.bSuccess = true;
    BaseResult.ExecutedCommandsCount = 1;
    BaseResult.FinalScreenId = "core:screen.location";
    BaseResult.FinalScreenFields.emplace("title", FValue(std::string("Marketplace")));

    const FRunDigest Digest1 = ComputeRunDigest(BaseManifest, BaseResult);
    const FRunDigest Digest2 = ComputeRunDigest(BaseManifest, BaseResult);

    if (Digest1.DigestHash.length() != 64 || Digest1.DigestHash != Digest2.DigestHash)
    {
        return "run_digest.digest_hash_not_deterministic";
    }

    if (Digest1 != Digest2)
    {
        return "run_digest.digest_equality";
    }

    // 2. Modifying manifest changes digest hash
    FRunManifest ModifiedManifest = BaseManifest;
    ModifiedManifest.AcceptedCommands[0].Args["target"] = FValue(std::string("core:location.city.tavern"));
    const FRunDigest DigestModifiedCmd = ComputeRunDigest(ModifiedManifest, BaseResult);
    if (DigestModifiedCmd.DigestHash == Digest1.DigestHash)
    {
        return "run_digest.command_args_change_not_reflected";
    }

    FRunManifest ModifiedSeedManifest = BaseManifest;
    ModifiedSeedManifest.Seed = 101;
    const FRunDigest DigestModifiedSeed = ComputeRunDigest(ModifiedSeedManifest, BaseResult);
    if (DigestModifiedSeed.DigestHash == Digest1.DigestHash)
    {
        return "run_digest.seed_change_not_reflected";
    }

    FRunManifest ModifiedScriptSetManifest = BaseManifest;
    ModifiedScriptSetManifest.ScriptSetHash = "45ed7d8000170391d46cac29a1d23534affa093312bf5eb9c73e62ccdc0ae5d8";
    const FRunDigest DigestModifiedScriptSet = ComputeRunDigest(ModifiedScriptSetManifest, BaseResult);
    if (DigestModifiedScriptSet.DigestHash == Digest1.DigestHash)
    {
        return "run_digest.script_set_hash_change_not_reflected";
    }

    // 3. Modifying result changes digest hash
    FRunResult ModifiedResult = BaseResult;
    ModifiedResult.bSuccess = false;
    ModifiedResult.FaultCode = "core:fault.command_rejected";
    const FRunDigest DigestModifiedResult = ComputeRunDigest(BaseManifest, ModifiedResult);
    if (DigestModifiedResult.DigestHash == Digest1.DigestHash)
    {
        return "run_digest.result_failure_not_reflected";
    }

    FRunResult ModifiedScreenResult = BaseResult;
    ModifiedScreenResult.FinalScreenId = "core:screen.inventory";
    const FRunDigest DigestModifiedScreen = ComputeRunDigest(BaseManifest, ModifiedScreenResult);
    if (DigestModifiedScreen.DigestHash == Digest1.DigestHash)
    {
        return "run_digest.screen_change_not_reflected";
    }

    FRunResult ModifiedStateResult = BaseResult;
    ModifiedStateResult.StateHash = "24cdcc27591ff32f962d36c867bf852032daf3e87b93d875d757861d3c55f5c6";
    const FRunDigest DigestModifiedState = ComputeRunDigest(BaseManifest, ModifiedStateResult);
    if (DigestModifiedState.DigestHash == Digest1.DigestHash)
    {
        return "run_digest.state_hash_change_not_reflected";
    }

    // 4. Round-trip serialization & deserialization
    const std::string Serialized = SerializeRunDigest(Digest1);
    if (Serialized.empty())
    {
        return "run_digest.serialize_failed";
    }

    FRunDigest DeserializedDigest;
    std::string Error;
    if (!DeserializeRunDigest(Serialized, DeserializedDigest, Error) || !Error.empty())
    {
        return "run_digest.deserialize_failed";
    }

    if (DeserializedDigest != Digest1)
    {
        return "run_digest.deserialized_equality";
    }

    // 5. Sanitization check
    if (Serialized.find("/home/") != std::string::npos
        || Serialized.find("/Game/") != std::string::npos
        || Serialized.find("C:\\") != std::string::npos)
    {
        return "run_digest.contains_filesystem_path";
    }

    // 6. Validation rejection of invalid serialized digest
    FRunDigest InvalidDigest;
    if (DeserializeRunDigest("malformed json {", InvalidDigest, Error)
        || Error != "run_digest.parse_json_failed")
    {
        return "run_digest.reject_malformed_json";
    }

    const std::string ValidSha = "35ed7d8000170391d46cac29a1d23534affa093312bf5eb9c73e62ccdc0ae5d8";
    const std::vector<std::string> BadHashes = {
        "short",
        std::string(63, 'a'),
        std::string(65, 'a'),
        std::string(64, 'z'),
        "35ED7D8000170391D46CAC29A1D23534AFFA093312BF5EB9C73E62CCDC0AE5D8",
        ""
    };

    // Invalid digest_hash
    for (const auto& BadHash : BadHashes)
    {
        const std::string Json = "{ digest_hash: '" + BadHash + "', lua_release_num: 50408, repository_content_hash: '" + ValidSha + "', script_set_hash: '" + ValidSha + "', seed: 0, executed_commands_count: 0, success: true, final_screen_id: '', fault_code: '' }";
        if (DeserializeRunDigest(Json, InvalidDigest, Error)
            || Error != "run_digest.invalid_digest_hash")
        {
            return "run_digest.reject_invalid_digest_hash";
        }
    }

    // Invalid repository_content_hash
    for (const auto& BadHash : BadHashes)
    {
        const std::string Json = "{ digest_hash: '" + ValidSha + "', lua_release_num: 50408, repository_content_hash: '" + BadHash + "', script_set_hash: '" + ValidSha + "', seed: 0, executed_commands_count: 0, success: true, final_screen_id: '', fault_code: '' }";
        if (DeserializeRunDigest(Json, InvalidDigest, Error)
            || Error != "run_digest.invalid_repository_content_hash")
        {
            return "run_digest.reject_invalid_repository_content_hash";
        }
    }

    // Invalid script_set_hash
    for (const auto& BadHash : BadHashes)
    {
        const std::string Json = "{ digest_hash: '" + ValidSha + "', lua_release_num: 50408, repository_content_hash: '" + ValidSha + "', script_set_hash: '" + BadHash + "', seed: 0, executed_commands_count: 0, success: true, final_screen_id: '', fault_code: '' }";
        if (DeserializeRunDigest(Json, InvalidDigest, Error)
            || Error != "run_digest.invalid_script_set_hash")
        {
            return "run_digest.reject_invalid_script_set_hash";
        }
    }

    // 7. State hash validation: permitted empty/omitted vs rejected invalid
    FRunDigest ValidPermittedDigest;

    // Permitted: omitted state_hash
    const std::string OmittedStateJson = "{ digest_hash: '" + ValidSha + "', lua_release_num: 50408, repository_content_hash: '" + ValidSha + "', script_set_hash: '" + ValidSha + "', seed: 0, executed_commands_count: 0, success: true, final_screen_id: '', fault_code: '' }";
    if (!DeserializeRunDigest(OmittedStateJson, ValidPermittedDigest, Error) || !Error.empty() || !ValidPermittedDigest.StateHash.empty())
    {
        return "run_digest.omitted_state_hash_must_be_permitted";
    }

    // Permitted: empty state_hash string
    const std::string EmptyStateJson = "{ digest_hash: '" + ValidSha + "', lua_release_num: 50408, repository_content_hash: '" + ValidSha + "', script_set_hash: '" + ValidSha + "', seed: 0, executed_commands_count: 0, success: true, final_screen_id: '', fault_code: '', state_hash: '' }";
    if (!DeserializeRunDigest(EmptyStateJson, ValidPermittedDigest, Error) || !Error.empty() || !ValidPermittedDigest.StateHash.empty())
    {
        return "run_digest.empty_state_hash_must_be_permitted";
    }

    // Permitted: valid 64 lowercase hex state_hash
    const std::string ValidStateJson = "{ digest_hash: '" + ValidSha + "', lua_release_num: 50408, repository_content_hash: '" + ValidSha + "', script_set_hash: '" + ValidSha + "', seed: 0, executed_commands_count: 0, success: true, final_screen_id: '', fault_code: '', state_hash: '" + ValidSha + "' }";
    if (!DeserializeRunDigest(ValidStateJson, ValidPermittedDigest, Error) || !Error.empty() || ValidPermittedDigest.StateHash != ValidSha)
    {
        return "run_digest.valid_state_hash_must_be_accepted";
    }

    // Rejected: invalid non-empty state_hash (64 'z', uppercase, length 63, length 65)
    const std::vector<std::string> BadStateHashes = {
        "short",
        std::string(63, 'a'),
        std::string(65, 'a'),
        std::string(64, 'z'),
        "35ED7D8000170391D46CAC29A1D23534AFFA093312BF5EB9C73E62CCDC0AE5D8"
    };
    for (const auto& BadStateHash : BadStateHashes)
    {
        const std::string BadStateJson = "{ digest_hash: '" + ValidSha + "', lua_release_num: 50408, repository_content_hash: '" + ValidSha + "', script_set_hash: '" + ValidSha + "', seed: 0, executed_commands_count: 0, success: true, final_screen_id: '', fault_code: '', state_hash: '" + BadStateHash + "' }";
        if (DeserializeRunDigest(BadStateJson, InvalidDigest, Error)
            || Error != "run_digest.invalid_state_hash")
        {
            return "run_digest.reject_invalid_state_hash";
        }
    }

    // Rejected: non-string state_hash
    const std::string NonStringStateJson = "{ digest_hash: '" + ValidSha + "', lua_release_num: 50408, repository_content_hash: '" + ValidSha + "', script_set_hash: '" + ValidSha + "', seed: 0, executed_commands_count: 0, success: true, final_screen_id: '', fault_code: '', state_hash: 12345 }";
    if (DeserializeRunDigest(NonStringStateJson, InvalidDigest, Error)
        || Error != "run_digest.invalid_state_hash")
    {
        return "run_digest.reject_non_string_state_hash";
    }

    return "";
}
} // namespace GV2RuntimeCore::Testing
