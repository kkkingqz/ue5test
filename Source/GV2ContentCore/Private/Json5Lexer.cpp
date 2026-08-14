#include "GV2ContentCore/Json5Lexer.h"

#include <cctype>

namespace GV2ContentCore
{
    bool FJson5Token::operator==(const FJson5Token& Other) const
    {
        return Kind == Other.Kind &&
               RawSpelling == Other.RawSpelling &&
               StringValue == Other.StringValue &&
               Span == Other.Span &&
               ByteOffset == Other.ByteOffset &&
               ByteLength == Other.ByteLength;
    }

    bool FJson5Token::operator!=(const FJson5Token& Other) const
    {
        return !(*this == Other);
    }

    namespace
    {
        bool IsWhitespaceChar(char C)
        {
            return C == ' ' || C == '\t' || C == '\n' || C == '\r' || C == '\v' || C == '\f';
        }

        bool IsIdentifierStartChar(char C)
        {
            return (C >= 'a' && C <= 'z') ||
                   (C >= 'A' && C <= 'Z') ||
                   C == '_' || C == '$';
        }

        bool IsIdentifierContinueChar(char C)
        {
            return IsIdentifierStartChar(C) || (C >= '0' && C <= '9');
        }

        bool IsHexDigit(char C)
        {
            return (C >= '0' && C <= '9') ||
                   (C >= 'a' && C <= 'f') ||
                   (C >= 'A' && C <= 'F');
        }

        int HexVal(char C)
        {
            if (C >= '0' && C <= '9') return C - '0';
            if (C >= 'a' && C <= 'f') return 10 + (C - 'a');
            if (C >= 'A' && C <= 'F') return 10 + (C - 'A');
            return 0;
        }

        void AppendUtf8CodePoint(std::uint32_t CodePoint, std::string& OutStr)
        {
            if (CodePoint <= 0x7F)
            {
                OutStr.push_back(static_cast<char>(CodePoint));
            }
            else if (CodePoint <= 0x7FF)
            {
                OutStr.push_back(static_cast<char>(0xC0 | ((CodePoint >> 6) & 0x1F)));
                OutStr.push_back(static_cast<char>(0x80 | (CodePoint & 0x3F)));
            }
            else if (CodePoint <= 0xFFFF)
            {
                OutStr.push_back(static_cast<char>(0xE0 | ((CodePoint >> 12) & 0x0F)));
                OutStr.push_back(static_cast<char>(0x80 | ((CodePoint >> 6) & 0x3F)));
                OutStr.push_back(static_cast<char>(0x80 | (CodePoint & 0x3F)));
            }
            else if (CodePoint <= 0x10FFFF)
            {
                OutStr.push_back(static_cast<char>(0xF0 | ((CodePoint >> 18) & 0x07)));
                OutStr.push_back(static_cast<char>(0x80 | ((CodePoint >> 12) & 0x3F)));
                OutStr.push_back(static_cast<char>(0x80 | ((CodePoint >> 6) & 0x3F)));
                OutStr.push_back(static_cast<char>(0x80 | (CodePoint & 0x3F)));
            }
        }
    }

    bool LexJson5(
        std::string_view RawInput,
        const FParseLimits& Limits,
        std::vector<FJson5Token>& OutTokens,
        std::vector<FDiagnostic>& OutDiagnostics,
        std::optional<std::string> PackageId,
        std::optional<std::uint32_t> PackageLoadIndex,
        std::optional<std::string> RelativeSource)
    {
        auto Utf8Res = ValidateUtf8AndLimits(
            RawInput, Limits, OutDiagnostics, PackageId, PackageLoadIndex, RelativeSource);
        if (!Utf8Res.has_value())
        {
            return false;
        }

        std::string_view Source = Utf8Res->CleanedView;
        const std::size_t BaseOffset = Utf8Res->ByteOffsetShift;

        std::size_t Index = 0;
        const std::size_t Size = Source.size();

        std::uint32_t Line = 1;
        std::uint32_t Column = 1;

        auto AdvanceChar = [&]() -> char
        {
            char C = Source[Index];
            Index++;
            if (C == '\n')
            {
                Line++;
                Column = 1;
            }
            else if (C == '\r')
            {
                if (Index < Size && Source[Index] == '\n')
                {
                    Index++;
                }
                Line++;
                Column = 1;
            }
            else
            {
                const unsigned char Byte = static_cast<unsigned char>(C);
                if ((Byte & 0xC0) != 0x80)
                {
                    Column++;
                }
            }
            return C;
        };

        auto PeekChar = [&](std::size_t Ahead = 0) -> char
        {
            if (Index + Ahead < Size)
            {
                return Source[Index + Ahead];
            }
            return '\0';
        };

        while (Index < Size)
        {
            char C = PeekChar();

            // Skip whitespace
            if (IsWhitespaceChar(C))
            {
                AdvanceChar();
                continue;
            }

            const std::size_t StartIndex = Index;
            const std::uint32_t StartLine = Line;
            const std::uint32_t StartColumn = Column;

            // Single characters
            if (C == '{' || C == '}' || C == '[' || C == ']' || C == ':' || C == ',')
            {
                AdvanceChar();
                FJson5Token Tok;
                Tok.ByteOffset = BaseOffset + StartIndex;
                Tok.ByteLength = Index - StartIndex;
                Tok.RawSpelling = std::string(Source.substr(StartIndex, Index - StartIndex));
                Tok.Span = FSourceSpan{ StartLine, StartColumn, Line, Column };

                switch (C)
                {
                case '{': Tok.Kind = EJson5TokenKind::CurlyOpen; break;
                case '}': Tok.Kind = EJson5TokenKind::CurlyClose; break;
                case '[': Tok.Kind = EJson5TokenKind::SquareOpen; break;
                case ']': Tok.Kind = EJson5TokenKind::SquareClose; break;
                case ':': Tok.Kind = EJson5TokenKind::Colon; break;
                case ',': Tok.Kind = EJson5TokenKind::Comma; break;
                }

                OutTokens.push_back(std::move(Tok));
                continue;
            }

            // Comments
            if (C == '/' && PeekChar(1) == '/')
            {
                AdvanceChar(); // '/'
                AdvanceChar(); // '/'
                while (Index < Size && PeekChar() != '\n' && PeekChar() != '\r')
                {
                    AdvanceChar();
                }

                FJson5Token Tok;
                Tok.Kind = EJson5TokenKind::Comment;
                Tok.ByteOffset = BaseOffset + StartIndex;
                Tok.ByteLength = Index - StartIndex;
                Tok.RawSpelling = std::string(Source.substr(StartIndex, Index - StartIndex));
                Tok.Span = FSourceSpan{ StartLine, StartColumn, Line, Column };
                OutTokens.push_back(std::move(Tok));
                continue;
            }

            if (C == '/' && PeekChar(1) == '*')
            {
                AdvanceChar(); // '/'
                AdvanceChar(); // '*'
                bool bClosed = false;
                while (Index < Size)
                {
                    if (PeekChar() == '*' && PeekChar(1) == '/')
                    {
                        AdvanceChar(); // '*'
                        AdvanceChar(); // '/'
                        bClosed = true;
                        break;
                    }
                    AdvanceChar();
                }

                if (!bClosed)
                {
                    FDiagnostic Diag;
                    Diag.Code = "core:diagnostic.json5.unclosed_comment";
                    Diag.Severity = EDiagnosticSeverity::Error;
                    Diag.Message = "Unclosed block comment";
                    Diag.PackageId = PackageId;
                    Diag.PackageLoadIndex = PackageLoadIndex;
                    Diag.RelativeSource = RelativeSource;
                    Diag.Span = FSourceSpan{ StartLine, StartColumn, Line, Column };
                    OutDiagnostics.push_back(std::move(Diag));
                    return false;
                }

                FJson5Token Tok;
                Tok.Kind = EJson5TokenKind::Comment;
                Tok.ByteOffset = BaseOffset + StartIndex;
                Tok.ByteLength = Index - StartIndex;
                Tok.RawSpelling = std::string(Source.substr(StartIndex, Index - StartIndex));
                Tok.Span = FSourceSpan{ StartLine, StartColumn, Line, Column };
                OutTokens.push_back(std::move(Tok));
                continue;
            }

            // Strings (single or double quoted)
            if (C == '"' || C == '\'')
            {
                const char QuoteChar = AdvanceChar();
                std::string StringVal;
                bool bClosed = false;

                while (Index < Size)
                {
                    const std::uint32_t CharacterLine = Line;
                    const std::uint32_t CharacterColumn = Column;
                    const char NextC = AdvanceChar();
                    if (NextC == QuoteChar)
                    {
                        bClosed = true;
                        break;
                    }

                    if (NextC == '\n' || NextC == '\r')
                    {
                        FDiagnostic Diag;
                        Diag.Code = "core:diagnostic.json5.syntax_error";
                        Diag.Message = "Raw line terminator is not allowed in a JSON5 string";
                        Diag.PackageId = PackageId;
                        Diag.PackageLoadIndex = PackageLoadIndex;
                        Diag.RelativeSource = RelativeSource;
                        Diag.Span = FSourceSpan{ CharacterLine, CharacterColumn, Line, Column };
                        OutDiagnostics.push_back(std::move(Diag));
                        return false;
                    }

                    if (NextC == '\\')
                    {
                        if (Index >= Size) break;
                        char EscC = AdvanceChar();
                        switch (EscC)
                        {
                        case '\'': StringVal.push_back('\''); break;
                        case '"': StringVal.push_back('"'); break;
                        case '\\': StringVal.push_back('\\'); break;
                        case '/': StringVal.push_back('/'); break;
                        case 'b': StringVal.push_back('\b'); break;
                        case 'f': StringVal.push_back('\f'); break;
                        case 'n': StringVal.push_back('\n'); break;
                        case 'r': StringVal.push_back('\r'); break;
                        case 't': StringVal.push_back('\t'); break;
                        case 'v': StringVal.push_back('\v'); break;
                        case '0':
                            if (PeekChar() >= '0' && PeekChar() <= '9')
                            {
                                FDiagnostic Diag;
                                Diag.Code = "core:diagnostic.json5.syntax_error";
                                Diag.Message = "Legacy octal escapes are not allowed in a JSON5 string";
                                Diag.PackageId = PackageId;
                                Diag.PackageLoadIndex = PackageLoadIndex;
                                Diag.RelativeSource = RelativeSource;
                                Diag.Span = FSourceSpan{ CharacterLine, CharacterColumn, Line, Column };
                                OutDiagnostics.push_back(std::move(Diag));
                                return false;
                            }
                            StringVal.push_back('\0');
                            break;
                        case '\n': break; // Multiline string escape
                        case '\r': break; // Multiline string escape
                        case 'u':
                        {
                            auto ReadHexDigits = [&](const int Count, std::uint32_t& OutValue)
                            {
                                if (Index + static_cast<std::size_t>(Count) > Size)
                                {
                                    return false;
                                }
                                OutValue = 0;
                                for (int DigitIndex = 0; DigitIndex < Count; ++DigitIndex)
                                {
                                    const char HexC = PeekChar(static_cast<std::size_t>(DigitIndex));
                                    if (!IsHexDigit(HexC))
                                    {
                                        return false;
                                    }
                                    OutValue = (OutValue << 4) | HexVal(HexC);
                                }
                                for (int DigitIndex = 0; DigitIndex < Count; ++DigitIndex)
                                {
                                    AdvanceChar();
                                }
                                return true;
                            };

                            std::uint32_t CodePoint = 0;
                            if (!ReadHexDigits(4, CodePoint))
                            {
                                FDiagnostic Diag;
                                Diag.Code = "core:diagnostic.json5.invalid_unicode_escape";
                                Diag.Message = "Invalid \\u escape in JSON5 string";
                                Diag.PackageId = PackageId;
                                Diag.PackageLoadIndex = PackageLoadIndex;
                                Diag.RelativeSource = RelativeSource;
                                Diag.Span = FSourceSpan{ StartLine, StartColumn, Line, Column };
                                OutDiagnostics.push_back(std::move(Diag));
                                return false;
                            }

                            if (CodePoint >= 0xD800 && CodePoint <= 0xDBFF)
                            {
                                if (PeekChar() != '\\' || PeekChar(1) != 'u')
                                {
                                    FDiagnostic Diag;
                                    Diag.Code = "core:diagnostic.json5.invalid_unicode_escape";
                                    Diag.Message = "High surrogate must be followed by a low surrogate";
                                    Diag.PackageId = PackageId;
                                    Diag.PackageLoadIndex = PackageLoadIndex;
                                    Diag.RelativeSource = RelativeSource;
                                    Diag.Span = FSourceSpan{ StartLine, StartColumn, Line, Column };
                                    OutDiagnostics.push_back(std::move(Diag));
                                    return false;
                                }
                                AdvanceChar();
                                AdvanceChar();
                                std::uint32_t LowSurrogate = 0;
                                if (!ReadHexDigits(4, LowSurrogate)
                                    || LowSurrogate < 0xDC00
                                    || LowSurrogate > 0xDFFF)
                                {
                                    FDiagnostic Diag;
                                    Diag.Code = "core:diagnostic.json5.invalid_unicode_escape";
                                    Diag.Message = "Invalid low surrogate in JSON5 string";
                                    Diag.PackageId = PackageId;
                                    Diag.PackageLoadIndex = PackageLoadIndex;
                                    Diag.RelativeSource = RelativeSource;
                                    Diag.Span = FSourceSpan{ StartLine, StartColumn, Line, Column };
                                    OutDiagnostics.push_back(std::move(Diag));
                                    return false;
                                }
                                CodePoint = 0x10000
                                    + ((CodePoint - 0xD800) << 10)
                                    + (LowSurrogate - 0xDC00);
                            }
                            else if (CodePoint >= 0xDC00 && CodePoint <= 0xDFFF)
                            {
                                FDiagnostic Diag;
                                Diag.Code = "core:diagnostic.json5.invalid_unicode_escape";
                                Diag.Message = "Unexpected low surrogate in JSON5 string";
                                Diag.PackageId = PackageId;
                                Diag.PackageLoadIndex = PackageLoadIndex;
                                Diag.RelativeSource = RelativeSource;
                                Diag.Span = FSourceSpan{ StartLine, StartColumn, Line, Column };
                                OutDiagnostics.push_back(std::move(Diag));
                                return false;
                            }
                            AppendUtf8CodePoint(CodePoint, StringVal);
                            break;
                        }
                        case 'x':
                        {
                            if (Index + 2 > Size || !IsHexDigit(PeekChar()) || !IsHexDigit(PeekChar(1)))
                            {
                                FDiagnostic Diag;
                                Diag.Code = "core:diagnostic.json5.invalid_unicode_escape";
                                Diag.Message = "Invalid \\x escape in JSON5 string";
                                Diag.PackageId = PackageId;
                                Diag.PackageLoadIndex = PackageLoadIndex;
                                Diag.RelativeSource = RelativeSource;
                                Diag.Span = FSourceSpan{ StartLine, StartColumn, Line, Column };
                                OutDiagnostics.push_back(std::move(Diag));
                                return false;
                            }
                            const std::uint32_t CodePoint = (HexVal(AdvanceChar()) << 4) | HexVal(AdvanceChar());
                            AppendUtf8CodePoint(CodePoint, StringVal);
                            break;
                        }
                        default:
                            StringVal.push_back(EscC);
                            break;
                        }
                    }
                    else
                    {
                        StringVal.push_back(NextC);
                    }

                    if (StringVal.size() > Limits.MaxStringLengthBytes)
                    {
                        const FSourceSpan CurrentSpan{ StartLine, StartColumn, Line, Column };
                        CheckStringLength(
                            StringVal.size(), Limits, CurrentSpan, OutDiagnostics,
                            PackageId, PackageLoadIndex, RelativeSource);
                        return false;
                    }
                }

                const FSourceSpan StringSpan{ StartLine, StartColumn, Line, Column };

                if (!bClosed)
                {
                    FDiagnostic Diag;
                    Diag.Code = "core:diagnostic.json5.unclosed_string";
                    Diag.Severity = EDiagnosticSeverity::Error;
                    Diag.Message = "Unclosed string literal";
                    Diag.PackageId = PackageId;
                    Diag.PackageLoadIndex = PackageLoadIndex;
                    Diag.RelativeSource = RelativeSource;
                    Diag.Span = StringSpan;
                    OutDiagnostics.push_back(std::move(Diag));
                    return false;
                }

                if (!CheckStringLength(
                        StringVal.size(), Limits, StringSpan, OutDiagnostics,
                        PackageId, PackageLoadIndex, RelativeSource))
                {
                    return false;
                }

                FJson5Token Tok;
                Tok.Kind = EJson5TokenKind::StringLiteral;
                Tok.ByteOffset = BaseOffset + StartIndex;
                Tok.ByteLength = Index - StartIndex;
                Tok.RawSpelling = std::string(Source.substr(StartIndex, Index - StartIndex));
                Tok.StringValue = std::move(StringVal);
                Tok.Span = StringSpan;
                OutTokens.push_back(std::move(Tok));
                continue;
            }

            // Numbers or signed special values (+Infinity, -Infinity, etc.)
            bool bIsNumber = false;
            if (C == '+' || C == '-')
            {
                char Next1 = PeekChar(1);
                if ((Next1 >= '0' && Next1 <= '9') || Next1 == '.' ||
                    Source.substr(Index + 1).starts_with("Infinity") ||
                    Source.substr(Index + 1).starts_with("NaN"))
                {
                    bIsNumber = true;
                }
            }
            else if ((C >= '0' && C <= '9') || C == '.')
            {
                bIsNumber = true;
            }

            if (bIsNumber)
            {
                if (C == '+' || C == '-')
                {
                    AdvanceChar();
                }

                if (Source.substr(Index).starts_with("0x") || Source.substr(Index).starts_with("0X"))
                {
                    AdvanceChar(); // '0'
                    AdvanceChar(); // 'x' / 'X'
                    while (Index < Size && IsHexDigit(PeekChar()))
                    {
                        AdvanceChar();
                    }
                }
                else if (Source.substr(Index).starts_with("Infinity"))
                {
                    for (int i = 0; i < 8; ++i) AdvanceChar();
                }
                else if (Source.substr(Index).starts_with("NaN"))
                {
                    for (int i = 0; i < 3; ++i) AdvanceChar();
                }
                else
                {
                    bool bHasDot = false;
                    bool bHasExp = false;

                    while (Index < Size)
                    {
                        char NC = PeekChar();
                        if (NC >= '0' && NC <= '9')
                        {
                            AdvanceChar();
                        }
                        else if (NC == '.' && !bHasDot && !bHasExp)
                        {
                            bHasDot = true;
                            AdvanceChar();
                        }
                        else if ((NC == 'e' || NC == 'E') && !bHasExp)
                        {
                            bHasExp = true;
                            AdvanceChar();
                            if (PeekChar() == '+' || PeekChar() == '-')
                            {
                                AdvanceChar();
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                }

                FJson5Token Tok;
                Tok.Kind = EJson5TokenKind::NumberLiteral;
                Tok.ByteOffset = BaseOffset + StartIndex;
                Tok.ByteLength = Index - StartIndex;
                Tok.RawSpelling = std::string(Source.substr(StartIndex, Index - StartIndex));
                Tok.StringValue = Tok.RawSpelling;
                Tok.Span = FSourceSpan{ StartLine, StartColumn, Line, Column };
                OutTokens.push_back(std::move(Tok));
                continue;
            }

            // Identifiers and Keywords
            if (IsIdentifierStartChar(C))
            {
                while (Index < Size && IsIdentifierContinueChar(PeekChar()))
                {
                    AdvanceChar();
                }

                std::string IdentName = std::string(Source.substr(StartIndex, Index - StartIndex));
                if (!CheckStringLength(
                        IdentName.size(), Limits,
                        FSourceSpan{ StartLine, StartColumn, Line, Column },
                        OutDiagnostics, PackageId, PackageLoadIndex, RelativeSource))
                {
                    return false;
                }
                FJson5Token Tok;
                Tok.ByteOffset = BaseOffset + StartIndex;
                Tok.ByteLength = Index - StartIndex;
                Tok.RawSpelling = IdentName;
                Tok.StringValue = IdentName;
                Tok.Span = FSourceSpan{ StartLine, StartColumn, Line, Column };

                if (IdentName == "null")
                {
                    Tok.Kind = EJson5TokenKind::NullLiteral;
                }
                else if (IdentName == "true" || IdentName == "false")
                {
                    Tok.Kind = EJson5TokenKind::BooleanLiteral;
                }
                else if (IdentName == "NaN" || IdentName == "Infinity")
                {
                    Tok.Kind = EJson5TokenKind::NumberLiteral;
                }
                else
                {
                    Tok.Kind = EJson5TokenKind::Identifier;
                }

                OutTokens.push_back(std::move(Tok));
                continue;
            }

            // Unknown / Invalid Token
            AdvanceChar();
            FDiagnostic Diag;
            Diag.Code = "core:diagnostic.json5.syntax_error";
            Diag.Severity = EDiagnosticSeverity::Error;
            Diag.Message = "Unexpected character in JSON5 input: '" + std::string(1, C) + "'";
            Diag.PackageId = PackageId;
            Diag.PackageLoadIndex = PackageLoadIndex;
            Diag.RelativeSource = RelativeSource;
            Diag.Span = FSourceSpan{ StartLine, StartColumn, Line, Column };
            OutDiagnostics.push_back(std::move(Diag));
            return false;
        }

        FJson5Token EofTok;
        EofTok.Kind = EJson5TokenKind::EndOfFile;
        EofTok.ByteOffset = BaseOffset + Size;
        EofTok.ByteLength = 0;
        EofTok.Span = FSourceSpan{ Line, Column, Line, Column };
        OutTokens.push_back(std::move(EofTok));

        return true;
    }
}
