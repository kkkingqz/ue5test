#include "GV2RuntimeCore/GV2RunManifest.h"

#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"
#include "GV2ContentCore/StableId.h"

#include <cmath>
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

void SerializeValue(const FValue& Value, std::string& Out)
{
    if (std::holds_alternative<std::monostate>(Value.Data))
    {
        Out += "null";
    }
    else if (std::holds_alternative<bool>(Value.Data))
    {
        Out += std::get<bool>(Value.Data) ? "true" : "false";
    }
    else if (std::holds_alternative<std::int64_t>(Value.Data))
    {
        Out += std::to_string(std::get<std::int64_t>(Value.Data));
    }
    else if (std::holds_alternative<double>(Value.Data))
    {
        const double D = std::get<double>(Value.Data);
        if (!std::isfinite(D))
        {
            Out += "null";
        }
        else
        {
            std::ostringstream NumberStream;
            NumberStream << std::setprecision(15) << D;
            std::string NumStr = NumberStream.str();
            if (NumStr.find('.') == std::string::npos && NumStr.find('e') == std::string::npos && NumStr.find('E') == std::string::npos)
            {
                NumStr += ".0";
            }
            Out += NumStr;
        }
    }
    else if (std::holds_alternative<std::string>(Value.Data))
    {
        EscapeJsonString(std::get<std::string>(Value.Data), Out);
    }
    else if (std::holds_alternative<FValue::FArray>(Value.Data))
    {
        Out.push_back('[');
        const auto& Array = std::get<FValue::FArray>(Value.Data);
        for (std::size_t Index = 0; Index < Array.size(); ++Index)
        {
            if (Index > 0)
            {
                Out += ", ";
            }
            SerializeValue(Array[Index], Out);
        }
        Out.push_back(']');
    }
    else if (std::holds_alternative<FValue::FObject>(Value.Data))
    {
        Out.push_back('{');
        const auto& Object = std::get<FValue::FObject>(Value.Data);
        bool bFirst = true;
        for (const auto& [Key, Val] : Object)
        {
            if (!bFirst)
            {
                Out += ", ";
            }
            bFirst = false;
            EscapeJsonString(Key, Out);
            Out += ": ";
            SerializeValue(Val, Out);
        }
        Out.push_back('}');
    }
}

FValue ContentValueToRuntimeValue(const GV2ContentCore::FValue& InValue)
{
    if (InValue.IsNull())
    {
        return FValue();
    }
    if (InValue.IsBoolean())
    {
        return FValue(InValue.AsBoolean());
    }
    if (InValue.IsInteger())
    {
        return FValue(InValue.AsInteger());
    }
    if (InValue.IsNumber())
    {
        return FValue(InValue.AsNumber());
    }
    if (InValue.IsString())
    {
        return FValue(InValue.AsString());
    }
    if (InValue.IsArray())
    {
        FValue::FArray Array;
        Array.reserve(InValue.AsArray().size());
        for (const auto& Item : InValue.AsArray())
        {
            Array.push_back(ContentValueToRuntimeValue(Item));
        }
        return FValue(std::move(Array));
    }
    if (InValue.IsObject())
    {
        FValue::FObject Object;
        for (const auto& [Key, Val] : InValue.AsObject())
        {
            Object.emplace(Key, ContentValueToRuntimeValue(Val));
        }
        return FValue(std::move(Object));
    }
    return FValue();
}
} // namespace

std::string SerializeRunManifest(const FRunManifest& Manifest)
{
    std::string Out = "{\n";
    Out += "  \"lua_release_num\": " + std::to_string(Manifest.LuaReleaseNumber) + ",\n";
    Out += "  \"repository_content_hash\": ";
    EscapeJsonString(Manifest.RepositoryContentHash, Out);
    Out += ",\n";
    Out += "  \"seed\": " + std::to_string(Manifest.Seed) + ",\n";
    Out += "  \"accepted_commands\": [";

    if (Manifest.AcceptedCommands.empty())
    {
        Out += "]\n}\n";
        return Out;
    }

    Out += "\n";
    for (std::size_t Index = 0; Index < Manifest.AcceptedCommands.size(); ++Index)
    {
        const auto& Cmd = Manifest.AcceptedCommands[Index];
        Out += "    {\n";
        Out += "      \"command_id\": ";
        EscapeJsonString(Cmd.CommandId, Out);
        Out += ",\n";
        Out += "      \"args\": ";
        SerializeValue(FValue(Cmd.Args), Out);
        Out += ",\n";
        Out += "      \"sequence\": " + std::to_string(Cmd.Sequence) + "\n";
        Out += "    }";
        if (Index + 1 < Manifest.AcceptedCommands.size())
        {
            Out += ",";
        }
        Out += "\n";
    }
    Out += "  ]\n}\n";
    return Out;
}

bool DeserializeRunManifest(
    const std::string_view Json,
    FRunManifest& OutManifest,
    std::string& OutError)
{
    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    auto Document = GV2ContentCore::ParseJson5Document(Json, GV2ContentCore::FParseLimits{}, Diagnostics);
    if (!Document.has_value() || !Diagnostics.empty())
    {
        OutError = "run_manifest.parse_json_failed";
        return false;
    }

    const auto& Root = Document->GetRootValue();
    if (!Root.IsObject())
    {
        OutError = "run_manifest.root_must_be_object";
        return false;
    }

    const auto* LuaReleaseVal = Root.FindField("lua_release_num");
    if (LuaReleaseVal == nullptr || !LuaReleaseVal->IsInteger())
    {
        OutError = "run_manifest.invalid_lua_release_num";
        return false;
    }

    const auto* HashVal = Root.FindField("repository_content_hash");
    if (HashVal == nullptr || !HashVal->IsString() || HashVal->AsString().length() != 64)
    {
        OutError = "run_manifest.invalid_repository_content_hash";
        return false;
    }
    for (const char Ch : HashVal->AsString())
    {
        if (!((Ch >= '0' && Ch <= '9') || (Ch >= 'a' && Ch <= 'f')))
        {
            OutError = "run_manifest.invalid_repository_content_hash";
            return false;
        }
    }

    const auto* SeedVal = Root.FindField("seed");
    if (SeedVal == nullptr || !SeedVal->IsInteger() || SeedVal->AsInteger() < 0)
    {
        OutError = "run_manifest.invalid_seed";
        return false;
    }

    const auto* CommandsVal = Root.FindField("accepted_commands");
    if (CommandsVal == nullptr || !CommandsVal->IsArray())
    {
        OutError = "run_manifest.invalid_accepted_commands";
        return false;
    }

    std::vector<FRunAcceptedCommand> Commands;
    Commands.reserve(CommandsVal->AsArray().size());

    for (const auto& CmdItem : CommandsVal->AsArray())
    {
        if (!CmdItem.IsObject())
        {
            OutError = "run_manifest.command_must_be_object";
            return false;
        }

        const auto* IdVal = CmdItem.FindField("command_id");
        if (IdVal == nullptr || !IdVal->IsString() || !GV2ContentCore::FStableId::IsValid(IdVal->AsString()))
        {
            OutError = "run_manifest.invalid_command_id";
            return false;
        }

        const auto* ArgsVal = CmdItem.FindField("args");
        if (ArgsVal == nullptr || !ArgsVal->IsObject())
        {
            OutError = "run_manifest.invalid_command_args";
            return false;
        }

        const auto* SeqVal = CmdItem.FindField("sequence");
        if (SeqVal == nullptr || !SeqVal->IsInteger() || SeqVal->AsInteger() < 0)
        {
            OutError = "run_manifest.invalid_command_sequence";
            return false;
        }

        FRunAcceptedCommand Cmd;
        Cmd.CommandId = IdVal->AsString();
        Cmd.Sequence = SeqVal->AsInteger();
        for (const auto& [Key, Val] : ArgsVal->AsObject())
        {
            Cmd.Args.emplace(Key, ContentValueToRuntimeValue(Val));
        }

        Commands.push_back(std::move(Cmd));
    }

    OutManifest.LuaReleaseNumber = static_cast<std::int32_t>(LuaReleaseVal->AsInteger());
    OutManifest.RepositoryContentHash = HashVal->AsString();
    OutManifest.Seed = static_cast<std::uint64_t>(SeedVal->AsInteger());
    OutManifest.AcceptedCommands = std::move(Commands);
    return true;
}

} // namespace GV2RuntimeCore
