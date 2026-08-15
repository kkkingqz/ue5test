#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/Testing/ParseLimitsConformance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreParseLimitsTest,
    "GV2.Runtime.ContentCore.ParseLimits",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreParseLimitsTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2ContentCore::Testing::RunParseLimitsConformance();
    TestTrue(
        FString::Printf(TEXT("ParseLimits conformance passes: %s"), UTF8_TO_TCHAR(Error.c_str())),
        Error.empty());
    return Error.empty();
}

#endif
