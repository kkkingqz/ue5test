#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/Testing/Json5LexerConformance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreJson5LexerTest,
    "GV2.Runtime.ContentCore.Json5Lexer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreJson5LexerTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2ContentCore::Testing::RunJson5LexerConformance();
    TestTrue(
        FString::Printf(TEXT("Json5Lexer conformance passes: %s"), UTF8_TO_TCHAR(Error.c_str())),
        Error.empty());
    return Error.empty();
}

#endif
