#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/Testing/BuildResultConformance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreBuildResultTest,
    "GV2.Runtime.ContentCore.BuildResultAPI",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreBuildResultTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2ContentCore::Testing::RunBuildResultConformance();
    TestTrue(
        FString::Printf(TEXT("BuildResult conformance passes: %s"), UTF8_TO_TCHAR(Error.c_str())),
        Error.empty());
    return Error.empty();
}

#endif
