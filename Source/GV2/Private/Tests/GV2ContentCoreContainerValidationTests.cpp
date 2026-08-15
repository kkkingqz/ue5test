#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/Testing/ContainerValidationConformance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreContainerValidationTest,
    "GV2.Runtime.ContentCore.ContainerValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreContainerValidationTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2ContentCore::Testing::RunContainerValidationConformance();
    TestTrue(
        FString::Printf(TEXT("ContainerValidation conformance passes: %s"), UTF8_TO_TCHAR(Error.c_str())),
        Error.empty());
    return Error.empty();
}

#endif
