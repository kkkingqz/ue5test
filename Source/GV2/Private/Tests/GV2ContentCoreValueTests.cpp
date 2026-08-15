#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/Testing/ValueModelConformance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreValueModelTest,
    "GV2.Runtime.ContentCore.ValueModel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreValueModelTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2ContentCore::Testing::RunValueModelConformance();
    TestTrue(
        FString::Printf(TEXT("ValueModel conformance passes: %s"), UTF8_TO_TCHAR(Error.c_str())),
        Error.empty());
    return Error.empty();
}

#endif
