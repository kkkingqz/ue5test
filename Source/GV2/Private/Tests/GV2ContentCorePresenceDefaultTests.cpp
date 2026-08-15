#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/Testing/PresenceDefaultConformance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCorePresenceDefaultTest,
    "GV2.Runtime.ContentCore.PresenceDefault",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCorePresenceDefaultTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2ContentCore::Testing::RunPresenceDefaultConformance();
    TestTrue(
        FString::Printf(TEXT("PresenceDefault conformance passes: %s"), UTF8_TO_TCHAR(Error.c_str())),
        Error.empty());
    return Error.empty();
}

#endif
