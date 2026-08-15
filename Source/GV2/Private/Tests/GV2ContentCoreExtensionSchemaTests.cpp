#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/Testing/ExtensionSchemaConformance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreExtensionSchemaTest,
    "GV2.Runtime.ContentCore.ExtensionSchema",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreExtensionSchemaTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2ContentCore::Testing::RunExtensionSchemaConformance();
    TestTrue(
        FString::Printf(TEXT("ExtensionSchema conformance passes: %s"), UTF8_TO_TCHAR(Error.c_str())),
        Error.empty());
    return Error.empty();
}

#endif
