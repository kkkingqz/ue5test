#include "GV2RuntimeCore/GV2RunDigest.h"

#include "GV2ContentCore/CanonicalHash.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"

#include <iomanip>
#include <sstream>

namespace GV2RuntimeCore
{
namespace
{
void EscapeJsonString(std::string_view Input, std::string& Out)
{
    Out.push_back('"');
    for (const char Ch : Input)
    {
        switch (Ch)
        {
        case '"':  Out += "\\\""; break;
        case '\\': Out += "\\\\"; break;
        case '\b': Out += "\\b";  break;
        case '\f': Out += "\\f";  break;
        case '\n': Out += "\\n";  break;
        case '\r': Out += "\\r";  break;
        case '\t': Out += "\\t";  break;
        default:
            if (static_cast<unsigned char>(Ch) < 0x20)
            {
                std::ostringstream HexStream;
                HexStream << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                          << static_cast<int>(static_cast<unsigned char>(Ch));
                Out += HexStream.str();
            }
            else
            {
                Out.push_back(Ch);
            }
            break;
        }
    }
    Out.push_back('"');
}
} // namespace

FRunDigest ComputeRunDigest(
    const FRunManifest& Manifest,
    const FRunResult& Result)
{
    char SeedBuffer[32];
    std::snprintf(SeedBuffer, sizeof(SeedBuffer), "%016llx", static_cast<unsigned long long>(Manifest.Seed));

    GV2ContentCore::FValue::FObject HashPayload;
    HashPayload.emplace_back("digest_format_version", GV2ContentCore::FValue(static_cast<std::int64_t>(2)));
    HashPayload.emplace_back("lua_release_num", GV2ContentCore::FValue(static_cast<std::int64_t>(Manifest.LuaReleaseNumber)));
    HashPayload.emplace_back("repository_content_hash", GV2ContentCore::FValue(Manifest.RepositoryContentHash));
    HashPayload.emplace_back("script_set_hash", GV2ContentCore::FValue(Manifest.ScriptSetHash));
    HashPayload.emplace_back("seed", GV2ContentCore::FValue(std::string(SeedBuffer)));

    GV2ContentCore::FValue::FArray CommandsArray;
    CommandsArray.reserve(Manifest.AcceptedCommands.size());
    for (const auto& Cmd : Manifest.AcceptedCommands)
    {
        GV2ContentCore::FValue::FObject CmdObj;
        CmdObj.emplace_back("command_id", GV2ContentCore::FValue(Cmd.CommandId));
        CmdObj.emplace_back("sequence", GV2ContentCore::FValue(Cmd.Sequence));
        CmdObj.emplace_back("args", RuntimeValueToContentValue(FValue(Cmd.Args)));
        CommandsArray.push_back(GV2ContentCore::FValue(std::move(CmdObj)));
    }
    HashPayload.emplace_back("manifest_commands", GV2ContentCore::FValue(std::move(CommandsArray)));

    HashPayload.emplace_back("success", GV2ContentCore::FValue(Result.bSuccess));
    HashPayload.emplace_back("executed_commands_count", GV2ContentCore::FValue(static_cast<std::int64_t>(Result.ExecutedCommandsCount)));
    HashPayload.emplace_back("final_screen_id", GV2ContentCore::FValue(Result.FinalScreenId));
    HashPayload.emplace_back("final_screen_fields", RuntimeValueToContentValue(FValue(Result.FinalScreenFields)));
    HashPayload.emplace_back("state_hash", GV2ContentCore::FValue(Result.StateHash));
    HashPayload.emplace_back("fault_code", GV2ContentCore::FValue(Result.FaultCode));

    FRunDigest Digest;
    Digest.DigestFormatVersion = 2;
    Digest.DigestHash = GV2ContentCore::ComputeCanonicalHash(GV2ContentCore::FValue(std::move(HashPayload)));
    Digest.LuaReleaseNumber = Manifest.LuaReleaseNumber;
    Digest.RepositoryContentHash = Manifest.RepositoryContentHash;
    Digest.ScriptSetHash = Manifest.ScriptSetHash;
    Digest.Seed = Manifest.Seed;
    Digest.ExecutedCommandsCount = Result.ExecutedCommandsCount;
    Digest.bSuccess = Result.bSuccess;
    Digest.FinalScreenId = Result.FinalScreenId;
    Digest.StateHash = Result.StateHash;
    Digest.FaultCode = Result.FaultCode;
    return Digest;
}

std::string SerializeRunDigest(const FRunDigest& Digest)
{
    char SeedBuffer[32];
    std::snprintf(SeedBuffer, sizeof(SeedBuffer), "%016llx", static_cast<unsigned long long>(Digest.Seed));

    std::string Out = "{\n";
    Out += "  \"digest_format_version\": " + std::to_string(Digest.DigestFormatVersion) + ",\n";
    Out += "  \"digest_hash\": ";
    EscapeJsonString(Digest.DigestHash, Out);
    Out += ",\n";
    Out += "  \"lua_release_num\": " + std::to_string(Digest.LuaReleaseNumber) + ",\n";
    Out += "  \"repository_content_hash\": ";
    EscapeJsonString(Digest.RepositoryContentHash, Out);
    Out += ",\n";
    Out += "  \"script_set_hash\": ";
    EscapeJsonString(Digest.ScriptSetHash, Out);
    Out += ",\n";
    Out += "  \"seed\": \"";
    Out += SeedBuffer;
    Out += "\",\n";
    Out += "  \"executed_commands_count\": " + std::to_string(Digest.ExecutedCommandsCount) + ",\n";
    Out += "  \"success\": ";
    Out += Digest.bSuccess ? "true,\n" : "false,\n";
    Out += "  \"final_screen_id\": ";
    EscapeJsonString(Digest.FinalScreenId, Out);
    Out += ",\n";
    Out += "  \"state_hash\": ";
    EscapeJsonString(Digest.StateHash, Out);
    Out += ",\n";
    Out += "  \"fault_code\": ";
    EscapeJsonString(Digest.FaultCode, Out);
    Out += "\n}\n";
    return Out;
}

bool DeserializeRunDigest(
    const std::string_view Json,
    FRunDigest& OutDigest,
    std::string& OutError)
{
    OutError.clear();
    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    auto Document = GV2ContentCore::ParseJson5Document(Json, GV2ContentCore::FParseLimits{}, Diagnostics);
    if (!Document.has_value() || !Diagnostics.empty())
    {
        OutError = "run_digest.parse_json_failed";
        return false;
    }

    const auto& Root = Document->GetRootValue();
    if (!Root.IsObject())
    {
        OutError = "run_digest.root_must_be_object";
        return false;
    }

    const auto* FormatVersionVal = Root.FindField("digest_format_version");
    std::int32_t FormatVersion = 1;
    if (FormatVersionVal != nullptr)
    {
        if (!FormatVersionVal->IsInteger() || FormatVersionVal->AsInteger() != 2)
        {
            OutError = "run_digest.unsupported_format_version";
            return false;
        }
        FormatVersion = static_cast<std::int32_t>(FormatVersionVal->AsInteger());
    }

    const auto* DigestHashVal = Root.FindField("digest_hash");
    if (DigestHashVal == nullptr || !DigestHashVal->IsString() || !GV2ContentCore::IsCanonicalSha256(DigestHashVal->AsString()))
    {
        OutError = "run_digest.invalid_digest_hash";
        return false;
    }

    const auto* LuaReleaseVal = Root.FindField("lua_release_num");
    if (LuaReleaseVal == nullptr || !LuaReleaseVal->IsInteger())
    {
        OutError = "run_digest.invalid_lua_release_num";
        return false;
    }

    const auto* RepoHashVal = Root.FindField("repository_content_hash");
    if (RepoHashVal == nullptr || !RepoHashVal->IsString() || !GV2ContentCore::IsCanonicalSha256(RepoHashVal->AsString()))
    {
        OutError = "run_digest.invalid_repository_content_hash";
        return false;
    }

    const auto* ScriptSetHashVal = Root.FindField("script_set_hash");
    if (ScriptSetHashVal == nullptr || !ScriptSetHashVal->IsString() || !GV2ContentCore::IsCanonicalSha256(ScriptSetHashVal->AsString()))
    {
        OutError = "run_digest.invalid_script_set_hash";
        return false;
    }

    const auto* SeedVal = Root.FindField("seed");
    if (SeedVal == nullptr)
    {
        OutError = "run_digest.invalid_seed";
        return false;
    }

    std::uint64_t ParsedSeed = 0;
    if (FormatVersion == 2)
    {
        if (!SeedVal->IsString())
        {
            OutError = "run_digest.invalid_seed";
            return false;
        }
        const std::string& SeedStr = SeedVal->AsString();
        if (SeedStr.size() != 16)
        {
            OutError = "run_digest.invalid_seed";
            return false;
        }
        for (char C : SeedStr)
        {
            if (!((C >= '0' && C <= '9') || (C >= 'a' && C <= 'f')))
            {
                OutError = "run_digest.invalid_seed";
                return false;
            }
        }
        char* EndPtr = nullptr;
        ParsedSeed = std::strtoull(SeedStr.c_str(), &EndPtr, 16);
        if (EndPtr == nullptr || *EndPtr != '\0')
        {
            OutError = "run_digest.invalid_seed";
            return false;
        }
    }
    else // FormatVersion == 1 (legacy migration)
    {
        if (!SeedVal->IsInteger() || SeedVal->AsInteger() < 0)
        {
            OutError = "run_digest.invalid_seed";
            return false;
        }
        ParsedSeed = static_cast<std::uint64_t>(SeedVal->AsInteger());
    }

    const auto* ExecutedVal = Root.FindField("executed_commands_count");
    if (ExecutedVal == nullptr || !ExecutedVal->IsInteger() || ExecutedVal->AsInteger() < 0)
    {
        OutError = "run_digest.invalid_executed_commands_count";
        return false;
    }

    const auto* SuccessVal = Root.FindField("success");
    if (SuccessVal == nullptr || !SuccessVal->IsBoolean())
    {
        OutError = "run_digest.invalid_success";
        return false;
    }

    const auto* ScreenVal = Root.FindField("final_screen_id");
    if (ScreenVal == nullptr || !ScreenVal->IsString())
    {
        OutError = "run_digest.invalid_final_screen_id";
        return false;
    }

    const auto* FaultVal = Root.FindField("fault_code");
    if (FaultVal == nullptr || !FaultVal->IsString())
    {
        OutError = "run_digest.invalid_fault_code";
        return false;
    }

    const auto* StateHashVal = Root.FindField("state_hash");
    if (StateHashVal != nullptr && (!StateHashVal->IsString() || (!StateHashVal->AsString().empty() && !GV2ContentCore::IsCanonicalSha256(StateHashVal->AsString()))))
    {
        OutError = "run_digest.invalid_state_hash";
        return false;
    }

    OutDigest.DigestFormatVersion = FormatVersion;
    OutDigest.DigestHash = DigestHashVal->AsString();
    OutDigest.LuaReleaseNumber = static_cast<std::int32_t>(LuaReleaseVal->AsInteger());
    OutDigest.RepositoryContentHash = RepoHashVal->AsString();
    OutDigest.ScriptSetHash = ScriptSetHashVal->AsString();
    OutDigest.Seed = ParsedSeed;
    OutDigest.ExecutedCommandsCount = static_cast<std::uint64_t>(ExecutedVal->AsInteger());
    OutDigest.bSuccess = SuccessVal->AsBoolean();
    OutDigest.FinalScreenId = ScreenVal->AsString();
    OutDigest.StateHash = StateHashVal != nullptr ? StateHashVal->AsString() : "";
    OutDigest.FaultCode = FaultVal->AsString();
    return true;
}

} // namespace GV2RuntimeCore
