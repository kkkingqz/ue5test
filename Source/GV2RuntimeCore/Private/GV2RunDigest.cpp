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

GV2ContentCore::FValue RuntimeValueToContentValue(const FValue& InValue)
{
    if (std::holds_alternative<std::monostate>(InValue.Data))
    {
        return GV2ContentCore::FValue::MakeNull();
    }
    if (std::holds_alternative<bool>(InValue.Data))
    {
        return GV2ContentCore::FValue(std::get<bool>(InValue.Data));
    }
    if (std::holds_alternative<std::int64_t>(InValue.Data))
    {
        return GV2ContentCore::FValue(std::get<std::int64_t>(InValue.Data));
    }
    if (std::holds_alternative<double>(InValue.Data))
    {
        return GV2ContentCore::FValue(std::get<double>(InValue.Data));
    }
    if (std::holds_alternative<std::string>(InValue.Data))
    {
        return GV2ContentCore::FValue(std::get<std::string>(InValue.Data));
    }
    if (std::holds_alternative<FValue::FArray>(InValue.Data))
    {
        GV2ContentCore::FValue::FArray Array;
        for (const auto& Item : std::get<FValue::FArray>(InValue.Data))
        {
            Array.push_back(RuntimeValueToContentValue(Item));
        }
        return GV2ContentCore::FValue(std::move(Array));
    }
    if (std::holds_alternative<FValue::FObject>(InValue.Data))
    {
        GV2ContentCore::FValue::FObject Object;
        for (const auto& [Key, Val] : std::get<FValue::FObject>(InValue.Data))
        {
            Object.emplace_back(Key, RuntimeValueToContentValue(Val));
        }
        return GV2ContentCore::FValue(std::move(Object));
    }
    return GV2ContentCore::FValue::MakeNull();
}
} // namespace

FRunDigest ComputeRunDigest(
    const FRunManifest& Manifest,
    const FRunResult& Result)
{
    GV2ContentCore::FValue::FObject HashPayload;
    HashPayload.emplace_back("lua_release_num", GV2ContentCore::FValue(static_cast<std::int64_t>(Manifest.LuaReleaseNumber)));
    HashPayload.emplace_back("repository_content_hash", GV2ContentCore::FValue(Manifest.RepositoryContentHash));
    HashPayload.emplace_back("seed", GV2ContentCore::FValue(static_cast<std::int64_t>(Manifest.Seed)));

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
    Digest.DigestHash = GV2ContentCore::ComputeCanonicalHash(GV2ContentCore::FValue(std::move(HashPayload)));
    Digest.LuaReleaseNumber = Manifest.LuaReleaseNumber;
    Digest.RepositoryContentHash = Manifest.RepositoryContentHash;
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
    std::string Out = "{\n";
    Out += "  \"digest_hash\": ";
    EscapeJsonString(Digest.DigestHash, Out);
    Out += ",\n";
    Out += "  \"lua_release_num\": " + std::to_string(Digest.LuaReleaseNumber) + ",\n";
    Out += "  \"repository_content_hash\": ";
    EscapeJsonString(Digest.RepositoryContentHash, Out);
    Out += ",\n";
    Out += "  \"seed\": " + std::to_string(Digest.Seed) + ",\n";
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

    const auto* DigestHashVal = Root.FindField("digest_hash");
    if (DigestHashVal == nullptr || !DigestHashVal->IsString() || DigestHashVal->AsString().length() != 64)
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
    if (RepoHashVal == nullptr || !RepoHashVal->IsString() || RepoHashVal->AsString().length() != 64)
    {
        OutError = "run_digest.invalid_repository_content_hash";
        return false;
    }

    const auto* SeedVal = Root.FindField("seed");
    if (SeedVal == nullptr || !SeedVal->IsInteger() || SeedVal->AsInteger() < 0)
    {
        OutError = "run_digest.invalid_seed";
        return false;
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
    if (StateHashVal != nullptr && !StateHashVal->IsString())
    {
        OutError = "run_digest.invalid_state_hash";
        return false;
    }

    OutDigest.DigestHash = DigestHashVal->AsString();
    OutDigest.LuaReleaseNumber = static_cast<std::int32_t>(LuaReleaseVal->AsInteger());
    OutDigest.RepositoryContentHash = RepoHashVal->AsString();
    OutDigest.Seed = static_cast<std::uint64_t>(SeedVal->AsInteger());
    OutDigest.ExecutedCommandsCount = static_cast<std::uint64_t>(ExecutedVal->AsInteger());
    OutDigest.bSuccess = SuccessVal->AsBoolean();
    OutDigest.FinalScreenId = ScreenVal->AsString();
    OutDigest.StateHash = StateHashVal != nullptr ? StateHashVal->AsString() : "";
    OutDigest.FaultCode = FaultVal->AsString();
    return true;
}

} // namespace GV2RuntimeCore
