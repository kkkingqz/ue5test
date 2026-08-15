#if WITH_DEV_AUTOMATION_TESTS

#include "GV2RuntimeCore/Testing/GV2RunReplayConformance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2RuntimeCoreRunReplayTest,
    "GV2.Runtime.Session.RunReplay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2RuntimeCoreRunReplayTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2RuntimeCore::Testing::RunRunReplayConformance();
    TestTrue(
        FString::Printf(TEXT("RunReplay conformance passes: %s"), UTF8_TO_TCHAR(Error.c_str())),
        Error.empty());
    return Error.empty();
}

#endif
