#include "Support/Json5AstRewriter.h"
#include "Support/CliOutput.h"

#include <algorithm>
#include <sstream>

namespace GV2ContentCli
{

bool IsValidJson5Identifier(std::string_view Key)
{
    if (Key.empty()) return false;
    char First = Key[0];
    if (!((First >= 'a' && First <= 'z') || (First >= 'A' && First <= 'Z') || First == '_' || First == '$'))
    {
        return false;
    }
    for (std::size_t i = 1; i < Key.size(); ++i)
    {
        char C = Key[i];
        if (!((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9') || C == '_' || C == '$'))
        {
            return false;
        }
    }
    return true;
}

void FormatJson5Value(std::ostream& Out, const GV2ContentCore::FValue& Value, int IndentLevel)
{
    std::string Indent(IndentLevel * 2, ' ');
    std::string ChildIndent((IndentLevel + 1) * 2, ' ');

    switch (Value.GetKind())
    {
    case GV2ContentCore::EValueKind::Null:
        Out << "null";
        break;
    case GV2ContentCore::EValueKind::Boolean:
        Out << (Value.AsBoolean() ? "true" : "false");
        break;
    case GV2ContentCore::EValueKind::Integer:
        Out << Value.AsInteger();
        break;
    case GV2ContentCore::EValueKind::Number:
        Out << FormatDouble(Value.AsNumber());
        break;
    case GV2ContentCore::EValueKind::String:
        WriteJsonEscapedString(Out, Value.AsString());
        break;
    case GV2ContentCore::EValueKind::Array:
    {
        const auto& Arr = Value.AsArray();
        if (Arr.empty())
        {
            Out << "[]";
        }
        else
        {
            Out << "[\n";
            for (const auto& Item : Arr)
            {
                Out << ChildIndent;
                FormatJson5Value(Out, Item, IndentLevel + 1);
                Out << ",\n";
            }
            Out << Indent << "]";
        }
        break;
    }
    case GV2ContentCore::EValueKind::Object:
    {
        const auto& Obj = Value.AsObject();
        if (Obj.empty())
        {
            Out << "{}";
        }
        else
        {
            Out << "{\n";
            for (const auto& [Key, Child] : Obj)
            {
                Out << ChildIndent;
                if (IsValidJson5Identifier(Key))
                {
                    Out << Key;
                }
                else
                {
                    WriteJsonEscapedString(Out, Key);
                }
                Out << ": ";
                FormatJson5Value(Out, Child, IndentLevel + 1);
                Out << ",\n";
            }
            Out << Indent << "}";
        }
        break;
    }
    }
}

GV2ContentCore::FValue GeneratePlaceholderValue(
    const GV2ContentCore::FCompiledFieldSpec& Spec,
    const std::string& PackageId,
    const std::string& DefinitionType,
    const std::string& DefinitionIdPath,
    const std::string& FieldName)
{
    switch (Spec.Kind)
    {
    case GV2ContentCore::EFieldKind::Scalar:
    {
        if (Spec.Scalar.has_value())
        {
            const auto& S = *Spec.Scalar;
            switch (S.Kind)
            {
            case GV2ContentCore::EScalarFieldKind::Boolean:
                return GV2ContentCore::FValue(false);
            case GV2ContentCore::EScalarFieldKind::Integer:
                if (S.MinimumInteger.has_value() && *S.MinimumInteger > 0)
                {
                    return GV2ContentCore::FValue(static_cast<std::int64_t>(*S.MinimumInteger));
                }
                return GV2ContentCore::FValue(static_cast<std::int64_t>(0));
            case GV2ContentCore::EScalarFieldKind::Number:
                if (S.MinimumNumber.has_value() && *S.MinimumNumber > 0.0)
                {
                    return GV2ContentCore::FValue(*S.MinimumNumber);
                }
                return GV2ContentCore::FValue(0.0);
            case GV2ContentCore::EScalarFieldKind::String:
                return GV2ContentCore::FValue(std::string("TODO"));
            case GV2ContentCore::EScalarFieldKind::Enum:
                if (!S.EnumValues.empty())
                {
                    return S.EnumValues.front();
                }
                return GV2ContentCore::FValue(std::string("TODO"));
            }
        }
        return GV2ContentCore::FValue(std::string("TODO"));
    }
    case GV2ContentCore::EFieldKind::Reference:
    {
        std::string TargetKind = Spec.ExpectedStableIdKind.empty() ? "item" : Spec.ExpectedStableIdKind;
        return GV2ContentCore::FValue(PackageId + ":" + TargetKind + ".placeholder");
    }
    case GV2ContentCore::EFieldKind::TextId:
    {
        std::string Suffix = FieldName;
        if (Suffix.ends_with("_text_id"))
        {
            Suffix = Suffix.substr(0, Suffix.size() - 8);
        }
        if (Suffix.empty()) Suffix = "text";
        return GV2ContentCore::FValue(PackageId + ":text." + DefinitionType + "." + DefinitionIdPath + "." + Suffix);
    }
    case GV2ContentCore::EFieldKind::ResourceReference:
    {
        std::string Suffix = FieldName;
        if (Suffix.ends_with("_resource_id"))
        {
            Suffix = Suffix.substr(0, Suffix.size() - 12);
        }
        if (Suffix.empty()) Suffix = "resource";
        return GV2ContentCore::FValue(PackageId + ":resource." + DefinitionType + "." + DefinitionIdPath + "." + Suffix);
    }
    case GV2ContentCore::EFieldKind::Array:
    {
        GV2ContentCore::FValue::FArray Arr;
        if (Spec.MinimumSize.has_value() && *Spec.MinimumSize > 0 && Spec.Items != nullptr)
        {
            for (std::size_t i = 0; i < *Spec.MinimumSize; ++i)
            {
                Arr.push_back(GeneratePlaceholderValue(*Spec.Items, PackageId, DefinitionType, DefinitionIdPath, FieldName));
            }
        }
        return GV2ContentCore::FValue(std::move(Arr));
    }
    case GV2ContentCore::EFieldKind::Map:
    {
        GV2ContentCore::FValue::FObject Map;
        return GV2ContentCore::FValue(std::move(Map));
    }
    case GV2ContentCore::EFieldKind::Object:
    {
        GV2ContentCore::FValue::FObject Obj;
        for (const auto& Field : Spec.Fields)
        {
            if (Field.bRequired && Field.Spec != nullptr)
            {
                Obj.emplace_back(Field.Name, GeneratePlaceholderValue(*Field.Spec, PackageId, DefinitionType, DefinitionIdPath, Field.Name));
            }
        }
        return GV2ContentCore::FValue(std::move(Obj));
    }
    case GV2ContentCore::EFieldKind::Union:
    {
        GV2ContentCore::FValue::FObject Obj;
        if (!Spec.Variants.empty())
        {
            Obj.emplace_back(Spec.Discriminator, GV2ContentCore::FValue(Spec.Variants.front().DiscriminatorValue));
            if (Spec.Variants.front().Spec != nullptr && Spec.Variants.front().Spec->Kind == GV2ContentCore::EFieldKind::Object)
            {
                for (const auto& Field : Spec.Variants.front().Spec->Fields)
                {
                    if (Field.bRequired && Field.Spec != nullptr)
                    {
                        Obj.emplace_back(Field.Name, GeneratePlaceholderValue(*Field.Spec, PackageId, DefinitionType, DefinitionIdPath, Field.Name));
                    }
                }
            }
        }
        return GV2ContentCore::FValue(std::move(Obj));
    }
    }
    return GV2ContentCore::FValue();
}

std::string FormatDefinitionEntry(
    const std::string& DefinitionId,
    const GV2ContentCore::FValue& Data,
    int IndentLevel)
{
    std::ostringstream Out;
    std::string Indent(IndentLevel * 2, ' ');
    std::string ChildIndent((IndentLevel + 1) * 2, ' ');

    Out << Indent << "{\n";
    Out << ChildIndent << "id: \"" << DefinitionId << "\",\n";
    Out << ChildIndent << "data: ";
    FormatJson5Value(Out, Data, IndentLevel + 1);
    Out << ",\n";
    Out << ChildIndent << "tags: [],\n";
    Out << ChildIndent << "deprecated: false,\n";
    Out << ChildIndent << "extensions: {},\n";
    Out << Indent << "},\n";
    return Out.str();
}

std::string CreateNewDefinitionFileContent(
    std::int64_t SchemaVersion,
    const std::string& DefinitionType,
    const std::string& FormattedEntry)
{
    std::ostringstream Out;
    Out << "{\n";
    Out << "  schema_version: " << SchemaVersion << ",\n";
    Out << "  type: \"" << DefinitionType << "\",\n";
    Out << "  definitions: [\n";
    Out << FormattedEntry;
    Out << "  ],\n";
    Out << "  extensions: {},\n";
    Out << "}\n";
    return Out.str();
}

bool InsertDefinitionEntryIntoJson5(
    const std::string& OriginalContent,
    const std::string& FormattedEntry,
    std::string& OutNewContent,
    std::string& OutErrorMessage)
{
    GV2ContentCore::FParseLimits Limits;
    std::vector<GV2ContentCore::FJson5Token> Tokens;
    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    if (!GV2ContentCore::LexJson5(OriginalContent, Limits, Tokens, Diagnostics))
    {
        OutErrorMessage = "failed to lex JSON5 document";
        return false;
    }

    std::size_t DefinitionsIdx = std::string::npos;
    for (std::size_t i = 0; i < Tokens.size(); ++i)
    {
        if ((Tokens[i].Kind == GV2ContentCore::EJson5TokenKind::Identifier
             || Tokens[i].Kind == GV2ContentCore::EJson5TokenKind::StringLiteral)
            && Tokens[i].StringValue == "definitions")
        {
            DefinitionsIdx = i;
            break;
        }
    }

    if (DefinitionsIdx == std::string::npos)
    {
        OutErrorMessage = "definitions array not found in document";
        return false;
    }

    std::size_t SquareOpenIdx = std::string::npos;
    for (std::size_t i = DefinitionsIdx + 1; i < Tokens.size(); ++i)
    {
        if (Tokens[i].Kind == GV2ContentCore::EJson5TokenKind::Comment) continue;
        if (Tokens[i].Kind == GV2ContentCore::EJson5TokenKind::Colon) continue;
        if (Tokens[i].Kind == GV2ContentCore::EJson5TokenKind::SquareOpen)
        {
            SquareOpenIdx = i;
            break;
        }
        OutErrorMessage = "expected ':' and '[' after definitions key";
        return false;
    }

    if (SquareOpenIdx == std::string::npos)
    {
        OutErrorMessage = "definitions array '[' not found";
        return false;
    }

    int Depth = 0;
    std::size_t SquareCloseIdx = std::string::npos;
    for (std::size_t i = SquareOpenIdx; i < Tokens.size(); ++i)
    {
        if (Tokens[i].Kind == GV2ContentCore::EJson5TokenKind::SquareOpen)
        {
            ++Depth;
        }
        else if (Tokens[i].Kind == GV2ContentCore::EJson5TokenKind::SquareClose)
        {
            --Depth;
            if (Depth == 0)
            {
                SquareCloseIdx = i;
                break;
            }
        }
    }

    if (SquareCloseIdx == std::string::npos)
    {
        OutErrorMessage = "closing ']' for definitions array not found";
        return false;
    }

    const std::size_t OpenOffset = Tokens[SquareOpenIdx].ByteOffset;
    const std::size_t CloseOffset = Tokens[SquareCloseIdx].ByteOffset;

    bool bEmptyArray = true;
    for (std::size_t i = SquareOpenIdx + 1; i < SquareCloseIdx; ++i)
    {
        if (Tokens[i].Kind != GV2ContentCore::EJson5TokenKind::Comment)
        {
            bEmptyArray = false;
            break;
        }
    }

    if (bEmptyArray)
    {
        OutNewContent = OriginalContent.substr(0, OpenOffset + 1) + "\n"
            + FormattedEntry + "  "
            + OriginalContent.substr(CloseOffset);
    }
    else
    {
        std::size_t LastNewline = OriginalContent.rfind('\n', CloseOffset);
        if (LastNewline != std::string::npos)
        {
            OutNewContent = OriginalContent.substr(0, LastNewline + 1)
                + FormattedEntry
                + OriginalContent.substr(LastNewline + 1);
        }
        else
        {
            OutNewContent = OriginalContent.substr(0, CloseOffset)
                + "\n" + FormattedEntry
                + OriginalContent.substr(CloseOffset);
        }
    }

    std::vector<GV2ContentCore::FDiagnostic> VerifyDiagnostics;
    auto ParsedVerify = GV2ContentCore::ParseJson5Document(OutNewContent, Limits, VerifyDiagnostics);
    if (!ParsedVerify)
    {
        OutErrorMessage = "generated JSON5 failed to parse";
        return false;
    }

    return true;
}

FRenameStringTokenResult ReplaceStringTokens(
    const std::string& RawContent,
    const std::string& OldValue,
    const std::string& NewValue,
    const std::string& PackageId,
    const std::string& RelativeSource)
{
    FRenameStringTokenResult Result;
    GV2ContentCore::FParseLimits Limits;
    std::vector<GV2ContentCore::FJson5Token> Tokens;
    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    if (!GV2ContentCore::LexJson5(RawContent, Limits, Tokens, Diagnostics, PackageId, 0, RelativeSource))
    {
        Result.ErrorMessage = "failed to lex source file " + RelativeSource;
        return Result;
    }

    std::vector<GV2ContentCore::FJson5Token> MatchingTokens;
    for (const GV2ContentCore::FJson5Token& Token : Tokens)
    {
        if (Token.Kind == GV2ContentCore::EJson5TokenKind::StringLiteral && Token.StringValue == OldValue)
        {
            MatchingTokens.push_back(Token);
        }
    }

    if (MatchingTokens.empty())
    {
        Result.bSuccess = true;
        Result.UpdatedContent = RawContent;
        Result.ReplacementsCount = 0;
        return Result;
    }

    // Sort descending by ByteOffset so that earlier replacements don't shift later byte offsets
    std::sort(MatchingTokens.begin(), MatchingTokens.end(), [](const GV2ContentCore::FJson5Token& A, const GV2ContentCore::FJson5Token& B) {
        return A.ByteOffset > B.ByteOffset;
    });

    std::string UpdatedContent = RawContent;
    for (const GV2ContentCore::FJson5Token& Token : MatchingTokens)
    {
        char QuoteChar = '"';
        if (Token.ByteLength >= 2 && Token.ByteOffset < RawContent.size())
        {
            const char FirstChar = RawContent[Token.ByteOffset];
            if (FirstChar == '\'' || FirstChar == '"')
            {
                QuoteChar = FirstChar;
            }
        }
        const std::string Replacement = std::string(1, QuoteChar) + NewValue + std::string(1, QuoteChar);
        if (Token.ByteOffset + Token.ByteLength <= UpdatedContent.size())
        {
            UpdatedContent.replace(Token.ByteOffset, Token.ByteLength, Replacement);
        }
    }

    // Validate updated content parses cleanly
    std::vector<GV2ContentCore::FDiagnostic> ParseDiagnostics;
    const auto Doc = GV2ContentCore::ParseJson5Document(
        UpdatedContent, Limits, ParseDiagnostics, PackageId, 0, RelativeSource);
    if (!Doc.has_value())
    {
        Result.ErrorMessage = "rewritten content failed to parse for " + RelativeSource;
        return Result;
    }

    Result.bSuccess = true;
    Result.UpdatedContent = std::move(UpdatedContent);
    Result.ReplacementsCount = MatchingTokens.size();
    return Result;
}

} // namespace GV2ContentCli
