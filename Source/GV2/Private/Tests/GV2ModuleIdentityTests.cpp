#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GV2Module.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ModuleIdentityTest,
    "GV2.Runtime.ModuleIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ModuleIdentityTest::RunTest(const FString& Parameters)
{
    const FGV2RuntimeModuleIdentity Identity = FGV2Module::Get().GetRuntimeIdentity();

    TestFalse(TEXT("source_revision is not empty"), Identity.SourceRevision.IsEmpty());
    TestFalse(TEXT("source_diff_hash is not empty"), Identity.SourceDiffHash.IsEmpty());
    TestFalse(TEXT("build_fingerprint is not empty"), Identity.BuildFingerprint.IsEmpty());

    TestFalse(TEXT("source_revision does not start with unknown_"), Identity.SourceRevision.StartsWith(TEXT("unknown_")));
    TestFalse(TEXT("source_diff_hash does not start with unknown_"), Identity.SourceDiffHash.StartsWith(TEXT("unknown_")));
    TestFalse(TEXT("build_fingerprint does not start with missing_"), Identity.BuildFingerprint.StartsWith(TEXT("missing_")));
    TestFalse(TEXT("build_fingerprint does not start with no_"), Identity.BuildFingerprint.StartsWith(TEXT("no_")));
    TestFalse(TEXT("build_fingerprint does not start with error:"), Identity.BuildFingerprint.StartsWith(TEXT("error:")));

    const FString JsonString = Identity.ToJson();
    ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Info, FString::Printf(TEXT("GV2_RUNTIME_IDENTITY:%s"), *JsonString)));

    FGV2Module::Get().WriteRuntimeIdentityArtifacts();
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
