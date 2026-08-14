#include "GV2ContentCore/Json5Parser.h"

#include <algorithm>
#include <cmath>
#include <charconv>
#include <limits>
#include <string>
#include <unordered_map>

namespace GV2ContentCore
{
    namespace
    {
        class FJson5ParserImpl
        {
        public:
            FJson5ParserImpl(
                const std::vector<FJson5Token>& InTokens,
                const FParseLimits& InLimits,
                std::vector<FDiagnostic>& InOutDiagnostics,
                std::optional<std::string> InPackageId,
                std::optional<std::uint32_t> InPackageLoadIndex,
                std::optional<std::string> InRelativeSource)
                : Tokens(InTokens)
                , Limits(InLimits)
                , OutDiagnostics(InOutDiagnostics)
                , PackageId(std::move(InPackageId))
                , PackageLoadIndex(InPackageLoadIndex)
                , RelativeSource(std::move(InRelativeSource))
            {
                AdvanceNonComment();
            }

            std::optional<FParsedDocument> Parse()
            {
                if (IsEof())
                {
                    EmitDiagnostic("core:diagnostic.json5.unexpected_eof", "Empty JSON5 document", GetCurrentSpan());
                    return std::nullopt;
                }

                auto RootVal = ParseValue(0, "");
                if (!RootVal.has_value())
                {
                    return std::nullopt;
                }

                if (!IsEof())
                {
                    EmitDiagnostic("core:diagnostic.json5.syntax_error", "Unexpected token after root value", GetCurrentSpan());
                    return std::nullopt;
                }

                return FParsedDocument(std::move(*RootVal), std::move(Locations));
            }

        private:
            const std::vector<FJson5Token>& Tokens;
            const FParseLimits& Limits;
            std::vector<FDiagnostic>& OutDiagnostics;
            std::optional<std::string> PackageId;
            std::optional<std::uint32_t> PackageLoadIndex;
            std::optional<std::string> RelativeSource;

            std::size_t TokenIndex = 0;
            std::vector<FParsedLocation> Locations;

            static std::string EscapeJsonPointerToken(const std::string& Token)
            {
                std::string Escaped;
                for (const char Character : Token)
                {
                    if (Character == '~') Escaped += "~0";
                    else if (Character == '/') Escaped += "~1";
                    else Escaped.push_back(Character);
                }
                return Escaped;
            }

            void RecordValue(const std::string& JsonPointer, const FSourceSpan& Span)
            {
                Locations.push_back(FParsedLocation{ JsonPointer, Span, std::nullopt });
            }

            void RecordKey(const std::string& JsonPointer, const FSourceSpan& Span)
            {
                for (FParsedLocation& Location : Locations)
                {
                    if (Location.JsonPointer == JsonPointer)
                    {
                        Location.KeySpan = Span;
                        return;
                    }
                }
            }

            void AdvanceNonComment()
            {
                while (TokenIndex < Tokens.size() && Tokens[TokenIndex].Kind == EJson5TokenKind::Comment)
                {
                    TokenIndex++;
                }
            }

            const FJson5Token& PeekToken() const
            {
                if (TokenIndex < Tokens.size())
                {
                    return Tokens[TokenIndex];
                }
                static FJson5Token EofTok{ EJson5TokenKind::EndOfFile, "", "", FSourceSpan{}, 0, 0 };
                return EofTok;
            }

            FJson5Token ConsumeToken()
            {
                FJson5Token Tok = PeekToken();
                if (TokenIndex < Tokens.size())
                {
                    TokenIndex++;
                }
                AdvanceNonComment();
                return Tok;
            }

            bool IsEof() const
            {
                return PeekToken().Kind == EJson5TokenKind::EndOfFile;
            }

            FSourceSpan GetCurrentSpan() const
            {
                return PeekToken().Span;
            }

            void EmitDiagnostic(const std::string& Code, const std::string& Message, const FSourceSpan& Span)
            {
                FDiagnostic Diag;
                Diag.Code = Code;
                Diag.Severity = EDiagnosticSeverity::Error;
                Diag.Message = Message;
                Diag.PackageId = PackageId;
                Diag.PackageLoadIndex = PackageLoadIndex;
                Diag.RelativeSource = RelativeSource;
                Diag.Span = Span;
                OutDiagnostics.push_back(std::move(Diag));
            }

            std::optional<FValue> ParseNumberToken(const FJson5Token& Tok)
            {
                const std::string& Raw = Tok.RawSpelling;

                const auto IsDecimalDigit = [](const char Character)
                {
                    return Character >= '0' && Character <= '9';
                };

                if (Raw == "NaN" || Raw == "Infinity" || Raw == "+Infinity" || Raw == "-Infinity")
                {
                    EmitDiagnostic("core:diagnostic.json5.invalid_number", "NaN and Infinity numeric literals are prohibited", Tok.Span);
                    return std::nullopt;
                }

                // Hex integer check
                bool bNegative = false;
                std::size_t Offset = 0;
                if (!Raw.empty() && (Raw[0] == '+' || Raw[0] == '-'))
                {
                    bNegative = (Raw[0] == '-');
                    Offset = 1;
                }

                if (Raw.size() >= Offset + 2 && Raw[Offset] == '0' && (Raw[Offset + 1] == 'x' || Raw[Offset + 1] == 'X'))
                {
                    const std::string_view Digits(Raw.data() + Offset + 2, Raw.size() - Offset - 2);
                    std::uint64_t Magnitude = 0;
                    const auto Result = std::from_chars(Digits.data(), Digits.data() + Digits.size(), Magnitude, 16);
                    const std::uint64_t MaximumMagnitude = bNegative
                        ? static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1u
                        : static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
                    if (Digits.empty() || Result.ec != std::errc{} || Result.ptr != Digits.data() + Digits.size() || Magnitude > MaximumMagnitude)
                    {
                        EmitDiagnostic("core:diagnostic.json5.invalid_number", "Hexadecimal integer literal overflow/underflow", Tok.Span);
                        return std::nullopt;
                    }
                    const std::int64_t SignedVal = bNegative
                        ? (Magnitude == MaximumMagnitude ? std::numeric_limits<std::int64_t>::min() : -static_cast<std::int64_t>(Magnitude))
                        : static_cast<std::int64_t>(Magnitude);
                    return FValue::MakeInteger(SignedVal);
                }

                // Floating point check (. or e or E)
                if (Raw.find('.') != std::string::npos || Raw.find('e') != std::string::npos || Raw.find('E') != std::string::npos)
                {
                    std::string NormalizedNumber = Raw;
                    if (!NormalizedNumber.empty() && NormalizedNumber.front() == '+')
                    {
                        NormalizedNumber.erase(NormalizedNumber.begin());
                    }
                    const std::size_t SignOffset = !NormalizedNumber.empty() && NormalizedNumber.front() == '-' ? 1 : 0;
                    const std::size_t ExponentOffset = NormalizedNumber.find_first_of("eE");
                    const std::size_t MantissaEnd = ExponentOffset == std::string::npos
                        ? NormalizedNumber.size()
                        : ExponentOffset;
                    const std::size_t DotOffset = NormalizedNumber.find('.', SignOffset);
                    const bool bHasMantissaDigit = std::any_of(
                        NormalizedNumber.begin() + static_cast<std::ptrdiff_t>(SignOffset),
                        NormalizedNumber.begin() + static_cast<std::ptrdiff_t>(MantissaEnd),
                        IsDecimalDigit);
                    const bool bHasInvalidLeadingZero = MantissaEnd > SignOffset + 1
                        && NormalizedNumber[SignOffset] == '0'
                        && (DotOffset == std::string::npos || DotOffset > SignOffset + 1);
                    bool bHasValidExponent = true;
                    if (ExponentOffset != std::string::npos)
                    {
                        std::size_t ExponentDigits = ExponentOffset + 1;
                        if (ExponentDigits < NormalizedNumber.size()
                            && (NormalizedNumber[ExponentDigits] == '+' || NormalizedNumber[ExponentDigits] == '-'))
                        {
                            ++ExponentDigits;
                        }
                        bHasValidExponent = ExponentDigits < NormalizedNumber.size()
                            && std::all_of(
                                NormalizedNumber.begin() + static_cast<std::ptrdiff_t>(ExponentDigits),
                                NormalizedNumber.end(),
                                IsDecimalDigit);
                    }
                    if (!bHasMantissaDigit || bHasInvalidLeadingZero || !bHasValidExponent)
                    {
                        EmitDiagnostic("core:diagnostic.json5.invalid_number", "Invalid decimal number literal", Tok.Span);
                        return std::nullopt;
                    }
                    if (SignOffset < NormalizedNumber.size() && NormalizedNumber[SignOffset] == '.')
                    {
                        NormalizedNumber.insert(SignOffset, "0");
                    }
                    const std::size_t NormalizedExponentOffset = NormalizedNumber.find_first_of("eE");
                    const std::size_t NormalizedMantissaEnd = NormalizedExponentOffset == std::string::npos
                        ? NormalizedNumber.size()
                        : NormalizedExponentOffset;
                    if (NormalizedMantissaEnd > 0 && NormalizedNumber[NormalizedMantissaEnd - 1] == '.')
                    {
                        NormalizedNumber.insert(NormalizedMantissaEnd, "0");
                    }
                    const std::string_view NumberView(NormalizedNumber);
                    double DblVal = 0.0;
                    const auto Result = std::from_chars(
                        NumberView.data(), NumberView.data() + NumberView.size(), DblVal, std::chars_format::general);
                    if (Result.ec != std::errc{} || Result.ptr != NumberView.data() + NumberView.size() || !std::isfinite(DblVal))
                    {
                        EmitDiagnostic("core:diagnostic.json5.invalid_number", "Double literal overflow or invalid value", Tok.Span);
                        return std::nullopt;
                    }
                    if (DblVal == 0.0)
                    {
                        DblVal = 0.0; // Canonicalize -0.0 to +0.0
                    }
                    return FValue::MakeNumber(DblVal);
                }

                // Decimal integer
                std::string_view IntegerView(Raw);
                if (!IntegerView.empty() && IntegerView.front() == '+') IntegerView.remove_prefix(1);
                const std::size_t IntegerSignOffset = !IntegerView.empty() && IntegerView.front() == '-' ? 1 : 0;
                if (IntegerView.size() > IntegerSignOffset + 1 && IntegerView[IntegerSignOffset] == '0')
                {
                    EmitDiagnostic("core:diagnostic.json5.invalid_number", "Leading zero is not allowed in a decimal integer literal", Tok.Span);
                    return std::nullopt;
                }
                std::int64_t IntVal = 0;
                const auto Result = std::from_chars(IntegerView.data(), IntegerView.data() + IntegerView.size(), IntVal, 10);
                if (Result.ec != std::errc{} || Result.ptr != IntegerView.data() + IntegerView.size())
                {
                    EmitDiagnostic("core:diagnostic.json5.invalid_number", "Integer literal overflow or underflow", Tok.Span);
                    return std::nullopt;
                }
                return FValue::MakeInteger(static_cast<int64_t>(IntVal));
            }

            std::optional<FValue> ParseValue(std::size_t Depth, const std::string& JsonPointer)
            {
                const FSourceSpan StartSpan = GetCurrentSpan();
                if (!CheckNestingDepth(Depth, Limits, StartSpan, OutDiagnostics, PackageId, PackageLoadIndex, RelativeSource))
                {
                    return std::nullopt;
                }

                const FJson5Token Tok = PeekToken();

                switch (Tok.Kind)
                {
                case EJson5TokenKind::NullLiteral:
                    ConsumeToken();
                    RecordValue(JsonPointer, Tok.Span);
                    return FValue::MakeNull();

                case EJson5TokenKind::BooleanLiteral:
                    ConsumeToken();
                    RecordValue(JsonPointer, Tok.Span);
                    return FValue::MakeBoolean(Tok.RawSpelling == "true");

                case EJson5TokenKind::StringLiteral:
                    ConsumeToken();
                    RecordValue(JsonPointer, Tok.Span);
                    return FValue::MakeString(Tok.StringValue);

                case EJson5TokenKind::NumberLiteral:
                    ConsumeToken();
                    if (auto Number = ParseNumberToken(Tok))
                    {
                        RecordValue(JsonPointer, Tok.Span);
                        return Number;
                    }
                    return std::nullopt;

                case EJson5TokenKind::SquareOpen:
                    return ParseArray(Depth, JsonPointer);

                case EJson5TokenKind::CurlyOpen:
                    return ParseObject(Depth, JsonPointer);

                case EJson5TokenKind::EndOfFile:
                    EmitDiagnostic("core:diagnostic.json5.unexpected_eof", "Unexpected EOF while parsing JSON5 value", Tok.Span);
                    return std::nullopt;

                default:
                    EmitDiagnostic("core:diagnostic.json5.syntax_error", "Unexpected token '" + Tok.RawSpelling + "'", Tok.Span);
                    return std::nullopt;
                }
            }

            std::optional<FValue> ParseArray(std::size_t Depth, const std::string& JsonPointer)
            {
                const FSourceSpan ArrayStartSpan = GetCurrentSpan();
                ConsumeToken(); // '['
                FSourceSpan ArrayEndSpan = ArrayStartSpan;

                FValue::FArray Elements;

                while (true)
                {
                    if (PeekToken().Kind == EJson5TokenKind::SquareClose)
                    {
                        ArrayEndSpan = ConsumeToken().Span; // ']'
                        break;
                    }

                    if (IsEof())
                    {
                        EmitDiagnostic("core:diagnostic.json5.unexpected_eof", "Unclosed array literal", ArrayStartSpan);
                        return std::nullopt;
                    }

                    const std::string ElementPointer = JsonPointer + "/" + std::to_string(Elements.size());
                    auto ElemVal = ParseValue(Depth + 1, ElementPointer);
                    if (!ElemVal.has_value())
                    {
                        return std::nullopt;
                    }

                    Elements.push_back(std::move(*ElemVal));

                    if (!CheckContainerEntries(Elements.size(), Limits, ArrayStartSpan, OutDiagnostics, PackageId, PackageLoadIndex, RelativeSource))
                    {
                        return std::nullopt;
                    }

                    if (PeekToken().Kind == EJson5TokenKind::Comma)
                    {
                        ConsumeToken(); // ','
                        if (PeekToken().Kind == EJson5TokenKind::SquareClose)
                        {
                            ArrayEndSpan = ConsumeToken().Span; // Trailing comma followed by ']'
                            break;
                        }
                    }
                    else if (PeekToken().Kind == EJson5TokenKind::SquareClose)
                    {
                        ArrayEndSpan = ConsumeToken().Span; // ']'
                        break;
                    }
                    else if (IsEof())
                    {
                        EmitDiagnostic("core:diagnostic.json5.unexpected_eof", "Unexpected EOF in array literal", ArrayStartSpan);
                        return std::nullopt;
                    }
                    else
                    {
                        EmitDiagnostic("core:diagnostic.json5.syntax_error", "Expected ',' or ']' in array", GetCurrentSpan());
                        return std::nullopt;
                    }
                }

                RecordValue(JsonPointer, FSourceSpan{ ArrayStartSpan.StartLine, ArrayStartSpan.StartColumn, ArrayEndSpan.EndLine, ArrayEndSpan.EndColumn });
                return FValue::MakeArray(std::move(Elements));
            }

            std::optional<FValue> ParseObject(std::size_t Depth, const std::string& JsonPointer)
            {
                const FSourceSpan ObjStartSpan = GetCurrentSpan();
                ConsumeToken(); // '{'
                FSourceSpan ObjEndSpan = ObjStartSpan;

                FValue::FObject Properties;
                std::unordered_map<std::string, FSourceSpan> SeenKeys;

                while (true)
                {
                    if (PeekToken().Kind == EJson5TokenKind::CurlyClose)
                    {
                        ObjEndSpan = ConsumeToken().Span; // '}'
                        break;
                    }

                    if (IsEof())
                    {
                        EmitDiagnostic("core:diagnostic.json5.unexpected_eof", "Unclosed object literal", ObjStartSpan);
                        return std::nullopt;
                    }

                    const FJson5Token KeyTok = PeekToken();
                    if (KeyTok.Kind != EJson5TokenKind::Identifier && KeyTok.Kind != EJson5TokenKind::StringLiteral)
                    {
                        EmitDiagnostic("core:diagnostic.json5.syntax_error", "Expected object property key (identifier or string)", KeyTok.Span);
                        return std::nullopt;
                    }

                    ConsumeToken();
                    std::string KeyName = (KeyTok.Kind == EJson5TokenKind::StringLiteral) ? KeyTok.StringValue : KeyTok.RawSpelling;

                    auto [It, bInserted] = SeenKeys.emplace(KeyName, KeyTok.Span);
                    if (!bInserted)
                    {
                        std::string Msg = "Duplicate object key '" + KeyName + "'";
                        EmitDiagnostic("core:diagnostic.json5.duplicate_key", Msg, KeyTok.Span);
                        OutDiagnostics.back().RelatedSpan = It->second;
                        OutDiagnostics.back().RelatedMessage = "First declaration of duplicate key";
                        return std::nullopt;
                    }

                    if (PeekToken().Kind != EJson5TokenKind::Colon)
                    {
                        EmitDiagnostic("core:diagnostic.json5.syntax_error", "Expected ':' after object property key", GetCurrentSpan());
                        return std::nullopt;
                    }
                    ConsumeToken(); // ':'

                    const std::string PropertyPointer = JsonPointer + "/" + EscapeJsonPointerToken(KeyName);
                    auto PropVal = ParseValue(Depth + 1, PropertyPointer);
                    if (!PropVal.has_value())
                    {
                        return std::nullopt;
                    }

                    Properties.push_back({ std::move(KeyName), std::move(*PropVal) });
                    RecordKey(PropertyPointer, KeyTok.Span);

                    if (!CheckContainerEntries(Properties.size(), Limits, ObjStartSpan, OutDiagnostics, PackageId, PackageLoadIndex, RelativeSource))
                    {
                        return std::nullopt;
                    }

                    if (PeekToken().Kind == EJson5TokenKind::Comma)
                    {
                        ConsumeToken(); // ','
                        if (PeekToken().Kind == EJson5TokenKind::CurlyClose)
                        {
                            ObjEndSpan = ConsumeToken().Span; // Trailing comma followed by '}'
                            break;
                        }
                    }
                    else if (PeekToken().Kind == EJson5TokenKind::CurlyClose)
                    {
                        ObjEndSpan = ConsumeToken().Span; // '}'
                        break;
                    }
                    else if (IsEof())
                    {
                        EmitDiagnostic("core:diagnostic.json5.unexpected_eof", "Unexpected EOF in object literal", ObjStartSpan);
                        return std::nullopt;
                    }
                    else
                    {
                        EmitDiagnostic("core:diagnostic.json5.syntax_error", "Expected ',' or '}' in object", GetCurrentSpan());
                        return std::nullopt;
                    }
                }

                RecordValue(JsonPointer, FSourceSpan{ ObjStartSpan.StartLine, ObjStartSpan.StartColumn, ObjEndSpan.EndLine, ObjEndSpan.EndColumn });
                return FValue::MakeObject(std::move(Properties));
            }
        };
    }

    FParsedDocument::FParsedDocument(
        FValue InRootValue,
        std::vector<FParsedLocation> InLocations)
        : RootValue(std::move(InRootValue))
        , Locations(std::move(InLocations))
    {
    }

    const FParsedLocation* FParsedDocument::FindLocation(const std::string_view JsonPointer) const
    {
        for (const FParsedLocation& Location : Locations)
        {
            if (Location.JsonPointer == JsonPointer)
            {
                return &Location;
            }
        }
        return nullptr;
    }

    std::optional<FParsedDocument> ParseJson5Document(
        std::string_view RawInput,
        const FParseLimits& Limits,
        std::vector<FDiagnostic>& OutDiagnostics,
        std::optional<std::string> PackageId,
        std::optional<std::uint32_t> PackageLoadIndex,
        std::optional<std::string> RelativeSource)
    {
        std::vector<FJson5Token> Tokens;
        if (!LexJson5(RawInput, Limits, Tokens, OutDiagnostics, PackageId, PackageLoadIndex, RelativeSource))
        {
            return std::nullopt;
        }

        FJson5ParserImpl Parser(Tokens, Limits, OutDiagnostics, std::move(PackageId), PackageLoadIndex, std::move(RelativeSource));
        return Parser.Parse();
    }

    std::optional<FValue> ParseJson5(
        std::string_view RawInput,
        const FParseLimits& Limits,
        std::vector<FDiagnostic>& OutDiagnostics,
        std::optional<std::string> PackageId,
        std::optional<std::uint32_t> PackageLoadIndex,
        std::optional<std::string> RelativeSource)
    {
        auto Document = ParseJson5Document(
            RawInput,
            Limits,
            OutDiagnostics,
            std::move(PackageId),
            PackageLoadIndex,
            std::move(RelativeSource));
        if (!Document.has_value())
        {
            return std::nullopt;
        }
        return Document->GetRootValue();
    }
}
