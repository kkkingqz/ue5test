#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/Testing/PoParserConformance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCorePoParserTest,
    "GV2.Runtime.ContentCore.PoParser",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCorePoParserTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2ContentCore::Testing::RunPoParserConformance();
    TestTrue(
        FString::Printf(TEXT("PoParser conformance passes: %s"), UTF8_TO_TCHAR(Error.c_str())),
        Error.empty());
    return Error.empty();
}

#endif
