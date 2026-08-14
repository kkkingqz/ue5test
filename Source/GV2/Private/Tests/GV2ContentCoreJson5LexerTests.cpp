#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/Json5Lexer.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreJson5LexerTest,
    "GV2.Runtime.ContentCore.Json5Lexer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreJson5LexerTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;

    FParseLimits Limits;

    // 1. Basic JSON5 token lexing
    std::string_view Input1 = "{\n  // Single comment\n  unquoted_key: \"hello \\n world\",\n  num: 0x1A,\n  array: [1, 2, /* block */ 3,]\n}";
    std::vector<FJson5Token> Tokens1;
    std::vector<FDiagnostic> Diags1;

    bool bLexOk = LexJson5(Input1, Limits, Tokens1, Diags1);
    TestTrue(TEXT("Lexing basic JSON5 succeeds"), bLexOk);
    TestTrue(TEXT("No diagnostics on valid JSON5"), Diags1.empty());

    // Check token stream
    TestTrue(TEXT("Tokens generated"), Tokens1.size() > 10);
    if (Tokens1.size() > 5)
    {
        TestEqual(TEXT("Token 0 is CurlyOpen"), static_cast<int>(Tokens1[0].Kind), static_cast<int>(EJson5TokenKind::CurlyOpen));
        TestEqual(TEXT("Token 1 is Comment"), static_cast<int>(Tokens1[1].Kind), static_cast<int>(EJson5TokenKind::Comment));
        TestEqual(TEXT("Token 2 is Identifier"), static_cast<int>(Tokens1[2].Kind), static_cast<int>(EJson5TokenKind::Identifier));
        TestEqual(TEXT("Identifier value is unquoted_key"), Tokens1[2].StringValue, std::string("unquoted_key"));
        TestEqual(TEXT("Token 3 is Colon"), static_cast<int>(Tokens1[3].Kind), static_cast<int>(EJson5TokenKind::Colon));
        TestEqual(TEXT("Token 4 is StringLiteral"), static_cast<int>(Tokens1[4].Kind), static_cast<int>(EJson5TokenKind::StringLiteral));
        TestEqual(TEXT("Unescaped string value"), Tokens1[4].StringValue, std::string("hello \n world"));
    }

    // 2. Unclosed string error
    std::string_view UnclosedStr = "{\n  key: \"unclosed string...\n}";
    std::vector<FJson5Token> Tokens2;
    std::vector<FDiagnostic> Diags2;
    bool bLexStrErr = LexJson5(UnclosedStr, Limits, Tokens2, Diags2);
    TestFalse(TEXT("Unclosed string fails lexer"), bLexStrErr);
    TestEqual(TEXT("Produces 1 diagnostic"), Diags2.size(), static_cast<size_t>(1));
    if (!Diags2.empty())
    {
        TestEqual(TEXT("Raw newline is a syntax error"), Diags2[0].Code, std::string("core:diagnostic.json5.syntax_error"));
    }

    // 3. Unclosed block comment error
    std::string_view UnclosedComment = "{\n  /* never closed comment...\n}";
    std::vector<FJson5Token> Tokens3;
    std::vector<FDiagnostic> Diags3;
    bool bLexCommentErr = LexJson5(UnclosedComment, Limits, Tokens3, Diags3);
    TestFalse(TEXT("Unclosed comment fails lexer"), bLexCommentErr);
    TestEqual(TEXT("Produces 1 diagnostic"), Diags3.size(), static_cast<size_t>(1));
    if (!Diags3.empty())
    {
        TestEqual(TEXT("Code is unclosed_comment"), Diags3[0].Code, std::string("core:diagnostic.json5.unclosed_comment"));
    }

    std::vector<FJson5Token> UnicodeTokens;
    std::vector<FDiagnostic> UnicodeDiagnostics;
    TestTrue(TEXT("UTF-16 surrogate pair escape is decoded"), LexJson5("\"\\uD83D\\uDE00\"", Limits, UnicodeTokens, UnicodeDiagnostics));
    if (!UnicodeTokens.empty())
    {
        TestEqual(TEXT("Decoded surrogate pair is UTF-8"), UnicodeTokens[0].StringValue, std::string("\xF0\x9F\x98\x80", 4));
    }

    UnicodeTokens.clear();
    UnicodeDiagnostics.clear();
    TestFalse(TEXT("Lone surrogate escape is rejected"), LexJson5("\"\\uD800\"", Limits, UnicodeTokens, UnicodeDiagnostics));
    TestTrue(TEXT("Lone surrogate has typed diagnostic"), !UnicodeDiagnostics.empty() && UnicodeDiagnostics[0].Code == "core:diagnostic.json5.invalid_unicode_escape");

    UnicodeTokens.clear();
    UnicodeDiagnostics.clear();
    TestFalse(TEXT("Non-ASCII unquoted key is rejected"), LexJson5("{ ключ: 1 }", Limits, UnicodeTokens, UnicodeDiagnostics));

    UnicodeTokens.clear();
    UnicodeDiagnostics.clear();
    TestTrue(TEXT("Quoted Unicode source lexes"), LexJson5("{ name: '\xD0\xAF', next: 1 }", Limits, UnicodeTokens, UnicodeDiagnostics));
    if (UnicodeTokens.size() > 5)
    {
        TestEqual(TEXT("Columns count Unicode code points, not UTF-8 bytes"), UnicodeTokens[5].Span.StartColumn, 14u);
    }

    return true;
}

#endif
