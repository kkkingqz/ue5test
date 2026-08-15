#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/Testing/Json5ParserConformance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreJson5ParserTest,
    "GV2.Runtime.ContentCore.Json5Parser",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreJson5ParserTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2ContentCore::Testing::RunJson5ParserConformance();
    TestTrue(
        FString::Printf(TEXT("Json5Parser conformance passes: %s"), UTF8_TO_TCHAR(Error.c_str())),
        Error.empty());
    return Error.empty();
}

#endif
