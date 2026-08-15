#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/Testing/SchemaRegistryConformance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreSchemaRegistryTest,
    "GV2.Runtime.ContentCore.SchemaRegistry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreSchemaRegistryTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2ContentCore::Testing::RunSchemaRegistryConformance();
    TestTrue(
        FString::Printf(TEXT("SchemaRegistry conformance passes: %s"), UTF8_TO_TCHAR(Error.c_str())),
        Error.empty());
    return Error.empty();
}

#endif
