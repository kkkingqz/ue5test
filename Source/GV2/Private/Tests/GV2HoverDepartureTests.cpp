#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"

#include "GV2PresentationApply/GV2PresentationInteractionSink.h"
#include "GV2PresentationTestFixtures.h"
#include "Runtime/GV2RuntimeSubsystem.h"
#include "UI/GV2GameShellWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2ScreenWidgetBase.h"

// PEP-09 (ADR-0048): the two kinds of exit and the input rule each one carries. Reuses
// SimulateHoverTickForAutomationTest (PEP-08's own test seam) to drive the real production
// transition-handling code without a controllable OS cursor.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2HoverDepartureInputGatingContract,
    "GV2.Runtime.Presentation.HoverDepartureInputGating",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2HoverDepartureInputGatingContract::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();
    UWorld* TestWorld = WorldContext.GetWorld();
    UGV2RuntimeSubsystem* Runtime = GameInstance != nullptr ? GameInstance->GetSubsystem<UGV2RuntimeSubsystem>() : nullptr;
    TestNotNull(TEXT("Runtime subsystem exists"), Runtime);
    if (Runtime == nullptr)
    {
        return false;
    }
    Runtime->StartSession();
    TestTrue(TEXT("Session is ready"), Runtime->GetSessionState().bIsReady);

    UGV2RichTextWidgetBase* RichText = CreateWidget<UGV2RichTextWidgetBase>(TestWorld, UGV2RichTextWidgetBase::StaticClass());
    UGV2ScreenWidgetBase* HoverScreenA = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
    TestNotNull(TEXT("RichText widget instantiates"), RichText);
    TestNotNull(TEXT("Hover screen A instantiates"), HoverScreenA);
    if (RichText == nullptr || HoverScreenA == nullptr)
    {
        return false;
    }

    const FName SpanIdA(TEXT("span_a"));
    FGV2RichTextSpanViewModel SpanA;
    SpanA.SpanId = SpanIdA;
    SpanA.Key = SpanIdA;
    SpanA.Hover.ScreenId = TEXT("core:screen.test_a");
    SpanA.Hover.ScreenWidget = HoverScreenA;
    SpanA.Hover.Duration = 2.0f;
    RichText->ApplySpans({SpanA});

    // ==========================================================================
    // 1. Self-dismissal: interactive to the end, cancellable at an intermediate opacity.
    // ==========================================================================
    RichText->SimulateHoverTickForAutomationTest(EGV2SpanHoverTransition::Began, SpanIdA, 1.0f); // halfway through a 2s appear
    TestTrue(TEXT("A freshly opened window accepts input"), HoverScreenA->GetIsEnabled());

    RichText->SimulateHoverTickForAutomationTest(EGV2SpanHoverTransition::Ended, SpanIdA, 0.0f); // start leaving, no time elapsed yet
    TestTrue(
        TEXT("Done: interaction with a fading (self-dismissing) window is accepted at an intermediate opacity, not only at the start"),
        HoverScreenA->GetIsEnabled());
    TestFalse(TEXT("Self-dismissal is not marked Stale"), RichText->GetActiveHoverIsStaleForAutomationTest());
    const float OpacityAtCancel = HoverScreenA->GetRenderOpacity();
    TestTrue(TEXT("Opacity is genuinely intermediate at the moment of cancel, not at the very start"), OpacityAtCancel > 0.0f && OpacityAtCancel < 1.0f);

    // Cursor returns -- this is the "click/interact with the fading window" moment. Cancel
    // resumes appearing; input was never gated to begin with.
    RichText->SimulateHoverTickForAutomationTest(EGV2SpanHoverTransition::Began, SpanIdA, 0.0f);
    TestTrue(TEXT("Cancelling the leave keeps the window interactive"), HoverScreenA->GetIsEnabled());
    TestEqual(TEXT("Done: cancel resumed from the exact intermediate opacity"), HoverScreenA->GetRenderOpacity(), OpacityAtCancel);

    // Let it finish appearing and then leave all the way, cleanly, for the next section.
    RichText->SimulateHoverTickForAutomationTest(EGV2SpanHoverTransition::None, NAME_None, 2.0f);
    RichText->SimulateHoverTickForAutomationTest(EGV2SpanHoverTransition::Ended, SpanIdA, 3.0f);
    TestTrue(TEXT("The self-dismissed window fully closed"), RichText->GetActiveHoverInstanceKeyForAutomationTest().IsNone());

    // ==========================================================================
    // 2. Stale: reconciliation removes the span while the window is open (still steady,
    // never even started leaving) -- input gates IMMEDIATELY, at the logical-removal
    // moment, strictly before the window is physically detached (it is still fading/
    // present in overlay_stack for however long the leave takes).
    // ==========================================================================
    RichText->SimulateHoverTickForAutomationTest(EGV2SpanHoverTransition::Began, SpanIdA, 0.0f);
    RichText->SimulateHoverTickForAutomationTest(EGV2SpanHoverTransition::None, NAME_None, 2.0f); // fully appeared, steady
    TestTrue(TEXT("Window is fully visible and enabled before reconciliation removes its span"), HoverScreenA->GetIsEnabled());
    TestEqual(TEXT("Fully appeared"), HoverScreenA->GetRenderOpacity(), 1.0f);

    const FName InstanceKeyBeforeStale = RichText->GetActiveHoverInstanceKeyForAutomationTest();
    TestFalse(TEXT("A window is open before the span is removed"), InstanceKeyBeforeStale.IsNone());

    // Reconciliation removes the span: a fresh ApplySpans with SpanA no longer present.
    RichText->ApplySpans({});

    TestTrue(TEXT("Done: input gates in the SAME call that removed the span -- not deferred to any later tick"), RichText->GetActiveHoverIsStaleForAutomationTest());
    TestFalse(
        TEXT("Done: the window no longer accepts input the moment reconciliation removed its span, before any physical detach"),
        HoverScreenA->GetIsEnabled());
    TestEqual(
        TEXT("Done: the gap between logical removal and physical detach is real -- the window is STILL a live overlay_stack participant right after going stale"),
        RichText->GetActiveHoverInstanceKeyForAutomationTest(),
        InstanceKeyBeforeStale);
    TestTrue(
        TEXT("The window is still a real overlay_stack child immediately after going stale (physical detach has not happened yet)"),
        Runtime->GetActiveGameShell() != nullptr
            && Runtime->GetActiveGameShell()->GetScreensInLayer(UGV2GameShellWidgetBase::LayerOverlayStack)
                .Contains(Cast<UUserWidget>(HoverScreenA)));

    // Done: stale plays out its fade (not a jump-cut) and does not accept input at any point
    // during it, then closes once opacity reaches zero -- never reopened by re-hovering.
    RichText->SimulateHoverTickForAutomationTest(EGV2SpanHoverTransition::None, NAME_None, 1.0f); // halfway
    TestTrue(TEXT("Still fading, not a jump-cut disappearance"), HoverScreenA->GetRenderOpacity() > 0.0f);
    TestFalse(TEXT("Still disabled mid-fade"), HoverScreenA->GetIsEnabled());

    // A same-span Began during a Stale leave must NOT cancel it (unlike self-dismissal).
    RichText->SimulateHoverTickForAutomationTest(EGV2SpanHoverTransition::Began, SpanIdA, 0.0f);
    TestFalse(TEXT("Re-hovering a Stale departure's own vanished span does not resurrect input"), HoverScreenA->GetIsEnabled());

    RichText->SimulateHoverTickForAutomationTest(EGV2SpanHoverTransition::None, NAME_None, 3.0f); // overshoot to zero
    TestTrue(TEXT("The stale window physically closes once opacity reaches zero"), RichText->GetActiveHoverInstanceKeyForAutomationTest().IsNone());

    return true;
}

// PEP-09: "интерактивный stale невыразим структурно" is proven by the TYPE (no field on
// FGV2StaleHostLocalDeparture could ever say "but accept input") -- this test proves there
// is ALSO a real behavioral safety net: if the ONE dispatch function that reads the type
// were mutated to ignore it, a concrete behavioral assertion (not a synthetic unit check)
// goes red. Mutated and reverted by hand while authoring this task (see the commit message);
// this test documents and re-exercises the same claim on every run via the type's own
// contract, not by re-performing the source mutation each time.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2HostLocalDepartureAcceptsInputContract,
    "GV2.Runtime.Presentation.HostLocalDepartureAcceptsInputIsExhaustive",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2HostLocalDepartureAcceptsInputContract::RunTest(const FString& Parameters)
{
    const FGV2HostLocalDepartureState Stale{TInPlaceType<FGV2StaleHostLocalDeparture>()};
    const FGV2HostLocalDepartureState SelfDismissing{TInPlaceType<FGV2SelfDismissingHostLocalDeparture>()};

    TestFalse(TEXT("A Stale departure never accepts input"), GV2HostLocalDepartureAcceptsInput(Stale));
    TestTrue(TEXT("A SelfDismissing departure always accepts input"), GV2HostLocalDepartureAcceptsInput(SelfDismissing));

    // The production header uses std::is_empty_v for the no-policy-data claim and an
    // overload visitor for compiler exhaustiveness. These runtime assertions prove the
    // two current alternatives reach their respective production overloads.
    static_assert(std::is_empty_v<FGV2StaleHostLocalDeparture>);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
