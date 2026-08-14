#pragma once

#include "GV2ContentCore/GV2ContentCore.h"
#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/ParseLimits.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace GV2ContentCore
{
    enum class EJson5TokenKind : std::uint8_t
    {
        CurlyOpen,      // {
        CurlyClose,     // }
        SquareOpen,     // [
        SquareClose,    // ]
        Colon,          // :
        Comma,          // ,
        NullLiteral,    // null
        BooleanLiteral, // true / false
        StringLiteral,  // "..." or '...'
        Identifier,     // unquoted key
        NumberLiteral,  // 123, 0x1A, 12.34, 1e10, NaN, Infinity
        Comment,        // //... or /*...*/
        EndOfFile
    };

    struct GV2_CONTENT_CORE_API FJson5Token final
    {
        EJson5TokenKind Kind = EJson5TokenKind::EndOfFile;
        std::string RawSpelling;
        std::string StringValue; // Parsed unescaped string or identifier name
        FSourceSpan Span;
        std::size_t ByteOffset = 0;
        std::size_t ByteLength = 0;

        bool operator==(const FJson5Token& Other) const;
        bool operator!=(const FJson5Token& Other) const;
    };

    /**
     * Lexes a UTF-8 JSON5 source string into a sequence of tokens.
     * Returns true on success, or false if lexer error occurs (populating OutDiagnostics).
     */
    GV2_CONTENT_CORE_API bool LexJson5(
        std::string_view RawInput,
        const FParseLimits& Limits,
        std::vector<FJson5Token>& OutTokens,
        std::vector<FDiagnostic>& OutDiagnostics,
        std::optional<std::string> PackageId = std::nullopt,
        std::optional<std::uint32_t> PackageLoadIndex = std::nullopt,
        std::optional<std::string> RelativeSource = std::nullopt);
}
