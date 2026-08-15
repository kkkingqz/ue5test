#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/Testing/DefinitionEnvelopeConformance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreDefinitionEnvelopeTest,
    "GV2.Runtime.ContentCore.DefinitionEnvelope",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreDefinitionEnvelopeTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2ContentCore::Testing::RunDefinitionEnvelopeConformance();
    TestTrue(
        FString::Printf(TEXT("DefinitionEnvelope conformance passes: %s"), UTF8_TO_TCHAR(Error.c_str())),
        Error.empty());
    return Error.empty();
}

#endif
