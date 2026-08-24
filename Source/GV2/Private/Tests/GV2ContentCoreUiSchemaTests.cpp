#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/Testing/UiSchemaConformance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreUiSchemaTest,
    "GV2.Runtime.ContentCore.UiSchema",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreUiSchemaTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2ContentCore::Testing::RunUiSchemaConformance();
    TestTrue(
        FString::Printf(TEXT("UiSchema conformance passes: %s"), UTF8_TO_TCHAR(Error.c_str())),
        Error.empty());
    return Error.empty();
}

#endif
