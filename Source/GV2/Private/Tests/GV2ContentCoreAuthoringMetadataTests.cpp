#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/Testing/AuthoringMetadataConformance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreAuthoringMetadataTest,
    "GV2.Runtime.ContentCore.AuthoringMetadata",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreAuthoringMetadataTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2ContentCore::Testing::RunAuthoringMetadataConformance();
    TestTrue(
        FString::Printf(TEXT("AuthoringMetadata conformance passes: %s"), UTF8_TO_TCHAR(Error.c_str())),
        Error.empty());
    return Error.empty();
}

#endif
