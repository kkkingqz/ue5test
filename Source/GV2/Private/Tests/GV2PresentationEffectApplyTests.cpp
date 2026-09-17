#if WITH_DEV_AUTOMATION_TESTS

#include "GV2PresentationApply/PresentationEffectApply.h"
#include "Components/Image.h"
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

    // 1. PEP-04/08: completeness gate over EPresentationEffectKind. PEP-08 adds the first
    // real kind (Transparency); the gate must still find every value either Supported (with
    // a working Apply branch) or independently confirmed Inapplicable -- Transparency is
    // Supported, so it needs no IsInapplicableKind entry, and the gate still passes.
    TArray<FString> GateDiagnostics;
    const bool bAllHandled = FGV2PresentationEffectApply::ValidateAllEffectKindsHandled(GateDiagnostics);
    TestTrue(TEXT("PEP-04/08: All EPresentationEffectKind values handled by gate"), bAllHandled);
    TestEqual(TEXT("PEP-04/08: Zero unhandled effect kind diagnostics"), GateDiagnostics.Num(), 0);
    TestEqual(
        TEXT("PEP-08: Transparency is reported Supported, not falling back to Inapplicable"),
        FGV2PresentationEffectApply::GetKindHandlingStatus(EPresentationEffectKind::Transparency),
        EGV2EffectKindStatus::Supported);

    // 2. With the sentinel Count value, applying is rejected as an unsupported kind, never
    // silently accepted.
    {
        FGV2PresentationEffectApplyResult Result;
        const bool bApplied = FGV2PresentationEffectApply::Apply(EPresentationEffectKind::Count, nullptr, 0.0f, Result);
        TestFalse(TEXT("PEP-04: Apply with the sentinel kind fails"), bApplied);
        TestEqual(TEXT("PEP-04: Rejection reason is UnsupportedKind"),
            Result.RejectReason, EGV2PresentationEffectApplyReject::UnsupportedKind);
        TestTrue(TEXT("PEP-04: Unsupported-kind diagnostic names the mechanism"),
            Result.Error.Contains(TEXT("core:diagnostic.presentation_effect_apply.unsupported_kind")));
    }

    // 3. PEP-08: Transparency actually sets UWidget::RenderOpacity, clamped to [0,1]; a null
    // widget is rejected as InvalidWidget, a distinct reason from UnsupportedKind.
    {
        UImage* ProbeWidget = NewObject<UImage>();
        FGV2PresentationEffectApplyResult Result;
        const bool bApplied = FGV2PresentationEffectApply::Apply(EPresentationEffectKind::Transparency, ProbeWidget, 0.5f, Result);
        TestTrue(TEXT("PEP-08: Transparency applies to a real widget"), bApplied);
        TestEqual(TEXT("PEP-08: RenderOpacity is set to the given alpha"), ProbeWidget->GetRenderOpacity(), 0.5f);

        const bool bClampedApplied = FGV2PresentationEffectApply::Apply(EPresentationEffectKind::Transparency, ProbeWidget, 5.0f, Result);
        TestTrue(TEXT("PEP-08: Transparency applies with an out-of-range alpha"), bClampedApplied);
        TestEqual(TEXT("PEP-08: RenderOpacity is clamped to 1.0"), ProbeWidget->GetRenderOpacity(), 1.0f);

        FGV2PresentationEffectApplyResult NullResult;
        const bool bNullApplied = FGV2PresentationEffectApply::Apply(EPresentationEffectKind::Transparency, nullptr, 0.5f, NullResult);
        TestFalse(TEXT("PEP-08: Transparency on a null widget fails"), bNullApplied);
        TestEqual(TEXT("PEP-08: Rejection reason is InvalidWidget, not UnsupportedKind"),
            NullResult.RejectReason, EGV2PresentationEffectApplyReject::InvalidWidget);
    }

    // 4. PEP-04 (ADR-0043 D2 parity): off-Game-Thread calls are rejected with a typed
    // reason, matching FGV2PresentationApply::Apply's own guard (CFC-04B in
    // GV2UiPrepareCommitTests.cpp).
    {
        FGV2PresentationEffectApplyResult GtResult;
        const bool bGtApplied = FGV2PresentationEffectApply::Apply(EPresentationEffectKind::Count, nullptr, 0.0f, GtResult);
        TestFalse(TEXT("PEP-04: Apply on Game Thread still rejects the unsupported sentinel"), bGtApplied);
        TestNotEqual(TEXT("PEP-04: Game Thread call is not rejected as off-thread"),
            GtResult.RejectReason, EGV2PresentationEffectApplyReject::OffGameThread);

        auto OffThreadFuture = std::async(std::launch::async, []()
        {
            FGV2PresentationEffectApplyResult WorkerResult;
            const bool bWorkerApplied = FGV2PresentationEffectApply::Apply(EPresentationEffectKind::Count, nullptr, 0.0f, WorkerResult);
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
