#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/Testing/SpecialFieldValidationConformance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreSpecialFieldValidationTest,
    "GV2.Runtime.ContentCore.SpecialFieldValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreSpecialFieldValidationTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2ContentCore::Testing::RunSpecialFieldValidationConformance();
    TestTrue(
        FString::Printf(TEXT("SpecialFieldValidation conformance passes: %s"), UTF8_TO_TCHAR(Error.c_str())),
        Error.empty());
    return Error.empty();
}

#endif
