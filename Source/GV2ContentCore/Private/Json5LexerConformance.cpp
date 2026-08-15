#include "GV2ContentCore/Testing/Json5LexerConformance.h"

#include "GV2ContentCore/Json5Lexer.h"
#include "GV2ContentCore/ParseLimits.h"

#include <string>
#include <string_view>
#include <vector>

namespace GV2ContentCore::Testing
{
std::string RunJson5LexerConformance()
{
    FParseLimits Limits;

    // 1. Basic JSON5 token lexing
    std::string_view Input1 = "{\n  // Single comment\n  unquoted_key: \"hello \\n world\",\n  num: 0x1A,\n  array: [1, 2, /* block */ 3,]\n}";
    std::vector<FJson5Token> Tokens1;
    std::vector<FDiagnostic> Diags1;

    bool bLexOk = LexJson5(Input1, Limits, Tokens1, Diags1);
    if (!bLexOk || !Diags1.empty() || Tokens1.size() <= 10)
    {
        return "json5_lexer.basic_lexing";
    }

    if (Tokens1[0].Kind != EJson5TokenKind::CurlyOpen
        || Tokens1[1].Kind != EJson5TokenKind::Comment
        || Tokens1[2].Kind != EJson5TokenKind::Identifier
        || Tokens1[2].StringValue != "unquoted_key"
        || Tokens1[3].Kind != EJson5TokenKind::Colon
        || Tokens1[4].Kind != EJson5TokenKind::StringLiteral
        || Tokens1[4].StringValue != "hello \n world")
    {
        return "json5_lexer.token_stream_content";
    }

    // 2. Unclosed string error
    std::string_view UnclosedStr = "{\n  key: \"unclosed string...\n}";
    std::vector<FJson5Token> Tokens2;
    std::vector<FDiagnostic> Diags2;
    bool bLexStrErr = LexJson5(UnclosedStr, Limits, Tokens2, Diags2);
    if (bLexStrErr || Diags2.size() != 1 || Diags2[0].Code != "core:diagnostic.json5.syntax_error")
    {
        return "json5_lexer.unclosed_string_syntax_error";
    }

    // 3. Unclosed block comment error
    std::string_view UnclosedComment = "{\n  /* never closed comment...\n}";
    std::vector<FJson5Token> Tokens3;
    std::vector<FDiagnostic> Diags3;
    bool bLexCommentErr = LexJson5(UnclosedComment, Limits, Tokens3, Diags3);
    if (bLexCommentErr || Diags3.size() != 1 || Diags3[0].Code != "core:diagnostic.json5.unclosed_comment")
    {
        return "json5_lexer.unclosed_block_comment";
    }

    // 4. UTF-16 surrogate pair escape decoded to UTF-8
    std::vector<FJson5Token> UnicodeTokens;
    std::vector<FDiagnostic> UnicodeDiagnostics;
    if (!LexJson5("\"\\uD83D\\uDE00\"", Limits, UnicodeTokens, UnicodeDiagnostics)
        || UnicodeTokens.empty()
        || UnicodeTokens[0].StringValue != std::string("\xF0\x9F\x98\x80", 4))
    {
        return "json5_lexer.utf16_surrogate_pair_decoded";
    }

    // 5. Lone surrogate escape rejected
    UnicodeTokens.clear();
    UnicodeDiagnostics.clear();
    if (LexJson5("\"\\uD800\"", Limits, UnicodeTokens, UnicodeDiagnostics)
        || UnicodeDiagnostics.empty()
        || UnicodeDiagnostics[0].Code != "core:diagnostic.json5.invalid_unicode_escape")
    {
        return "json5_lexer.lone_surrogate_rejected";
    }

    // 6. Non-ASCII unquoted key rejected
    UnicodeTokens.clear();
    UnicodeDiagnostics.clear();
    if (LexJson5("{ ключ: 1 }", Limits, UnicodeTokens, UnicodeDiagnostics))
    {
        return "json5_lexer.non_ascii_unquoted_key_rejected";
    }

    // 7. Quoted Unicode source lexes and columns count code points
    UnicodeTokens.clear();
    UnicodeDiagnostics.clear();
    if (!LexJson5("{ name: '\xD0\xAF', next: 1 }", Limits, UnicodeTokens, UnicodeDiagnostics)
        || UnicodeTokens.size() <= 5
        || UnicodeTokens[5].Span.StartColumn != 14u)
    {
        return "json5_lexer.unicode_column_code_point_counting";
    }

    return "";
}
} // namespace GV2ContentCore::Testing
