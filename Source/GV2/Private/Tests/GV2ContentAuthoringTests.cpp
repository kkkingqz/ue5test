#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentAuthoring/Testing/AuthoringLibraryConformance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentAuthoringTest,
    "GV2.Runtime.ContentAuthoring.Conformance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentAuthoringTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2ContentAuthoring::Testing::RunAuthoringLibraryConformance();
    TestTrue(
        FString::Printf(TEXT("AuthoringLibrary conformance passes: %s"), UTF8_TO_TCHAR(Error.c_str())),
        Error.empty());
    return Error.empty();
}

#endif
