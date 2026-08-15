#if WITH_DEV_AUTOMATION_TESTS

#include "GV2RuntimeCore/Testing/GV2RunDigestConformance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2RuntimeCoreRunDigestTest,
    "GV2.Runtime.Session.RunDigest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2RuntimeCoreRunDigestTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2RuntimeCore::Testing::RunRunDigestConformance();
    TestTrue(
        FString::Printf(TEXT("RunDigest conformance passes: %s"), UTF8_TO_TCHAR(Error.c_str())),
        Error.empty());
    return Error.empty();
}

#endif
