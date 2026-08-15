#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/Testing/DiagnosticModelConformance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreDiagnosticModelTest,
    "GV2.Runtime.ContentCore.DiagnosticModel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreDiagnosticModelTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2ContentCore::Testing::RunDiagnosticModelConformance();
    TestTrue(
        FString::Printf(TEXT("DiagnosticModel conformance passes: %s"), UTF8_TO_TCHAR(Error.c_str())),
        Error.empty());
    return Error.empty();
}

#endif
