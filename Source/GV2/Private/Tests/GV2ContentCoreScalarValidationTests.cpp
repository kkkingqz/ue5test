#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/Testing/ScalarValidationConformance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreScalarValidationTest,
    "GV2.Runtime.ContentCore.ScalarValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreScalarValidationTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2ContentCore::Testing::RunScalarValidationConformance();
    TestTrue(
        FString::Printf(TEXT("ScalarValidation conformance passes: %s"), UTF8_TO_TCHAR(Error.c_str())),
        Error.empty());
    return Error.empty();
}

#endif
