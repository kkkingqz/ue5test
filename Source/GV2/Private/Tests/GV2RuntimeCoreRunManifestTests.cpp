#if WITH_DEV_AUTOMATION_TESTS

#include "GV2RuntimeCore/Testing/GV2RunManifestConformance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2RuntimeCoreRunManifestTest,
    "GV2.Runtime.Session.RunManifest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2RuntimeCoreRunManifestTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2RuntimeCore::Testing::RunRunManifestConformance();
    TestTrue(
        FString::Printf(TEXT("RunManifest conformance passes: %s"), UTF8_TO_TCHAR(Error.c_str())),
        Error.empty());
    return Error.empty();
}

#endif
