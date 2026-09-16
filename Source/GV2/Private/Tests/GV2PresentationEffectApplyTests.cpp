#if WITH_DEV_AUTOMATION_TESTS

#include "GV2PresentationApply/PresentationEffectApply.h"
#include "Misc/AutomationTest.h"

#include <future>

// PEP-04: a dedicated file, not appended to GV2UiPrepareCommitTests.cpp -- this exercises
// FGV2PresentationEffectApply, a distinct entry point from FGV2PresentationApply, matching
// GV2PresentationEffectConformanceTests.cpp's own precedent from PEP-03.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2PresentationEffectApplyTest,
    "GV2.Runtime.Presentation.PresentationEffectApply",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2PresentationEffectApplyTest::RunTest(const FString& Parameters)
{
    using namespace GV2PresentationApply;

    // 1. PEP-04: completeness gate over EPresentationEffectKind. Count is 0 until PEP-08
    // adds the first kind (opacity fade); the gate trivially passes over an empty range,
    // exactly as it will once real kinds exist to walk.
    TArray<FString> GateDiagnostics;
    const bool bAllHandled = FGV2PresentationEffectApply::ValidateAllEffectKindsHandled(GateDiagnostics);
    TestTrue(TEXT("PEP-04: All EPresentationEffectKind values handled by gate"), bAllHandled);
    TestEqual(TEXT("PEP-04: Zero unhandled effect kind diagnostics"), GateDiagnostics.Num(), 0);

    // 2. PEP-04: with no kind registered yet, applying the sentinel value is rejected as
    // an unsupported kind, never silently accepted. This is also the only concrete input
    // available to exercise the "unsupported kind" path before PEP-08 exists.
    {
        FGV2PresentationEffectApplyResult Result;
        const bool bApplied = FGV2PresentationEffectApply::Apply(EPresentationEffectKind::Count, Result);
        TestFalse(TEXT("PEP-04: Apply with no registered kind fails"), bApplied);
        TestEqual(TEXT("PEP-04: Rejection reason is UnsupportedKind"),
            Result.RejectReason, EGV2PresentationEffectApplyReject::UnsupportedKind);
        TestTrue(TEXT("PEP-04: Unsupported-kind diagnostic names the mechanism"),
            Result.Error.Contains(TEXT("core:diagnostic.presentation_effect_apply.unsupported_kind")));

        // Apply's signature carries no widget, snapshot, or gameplay-state reference --
        // GV2PresentationApply.Build.cs denies every module that could supply one. A
        // rejected effect therefore cannot have touched snapshot or gameplay state: the
        // guarantee is structural (ADR-0043 D2), not something this test has to prove by
        // reasoning about side effects that have no type to be expressed through here.
    }

    // 3. PEP-04 (ADR-0043 D2 parity): off-Game-Thread calls are rejected with a typed
    // reason, matching FGV2PresentationApply::Apply's own guard (CFC-04B in
    // GV2UiPrepareCommitTests.cpp).
    {
        FGV2PresentationEffectApplyResult GtResult;
        const bool bGtApplied = FGV2PresentationEffectApply::Apply(EPresentationEffectKind::Count, GtResult);
        TestFalse(TEXT("PEP-04: Apply on Game Thread still rejects the unsupported sentinel"), bGtApplied);
        TestNotEqual(TEXT("PEP-04: Game Thread call is not rejected as off-thread"),
            GtResult.RejectReason, EGV2PresentationEffectApplyReject::OffGameThread);

        auto OffThreadFuture = std::async(std::launch::async, []()
        {
            FGV2PresentationEffectApplyResult WorkerResult;
            const bool bWorkerApplied = FGV2PresentationEffectApply::Apply(EPresentationEffectKind::Count, WorkerResult);
            return TPair<bool, EGV2PresentationEffectApplyReject>(bWorkerApplied, WorkerResult.RejectReason);
        });

        TPair<bool, EGV2PresentationEffectApplyReject> OffThreadOutcome = OffThreadFuture.get();
        TestFalse(TEXT("PEP-04: Apply from worker thread is rejected"), OffThreadOutcome.Key);
        TestEqual(TEXT("PEP-04: Worker-thread rejection reason is OffGameThread"),
            OffThreadOutcome.Value, EGV2PresentationEffectApplyReject::OffGameThread);
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
