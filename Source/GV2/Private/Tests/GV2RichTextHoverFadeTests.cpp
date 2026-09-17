#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#include "GV2PresentationTestFixtures.h"
#include "Runtime/GV2RuntimeSubsystem.h"
#include "UI/GV2GameShellWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2ScreenWidgetBase.h"

// PEP-08: appear/leave-with-cancel, driven through UGV2RichTextWidgetBase's own production
// transition-handling and fade-ticking code (SimulateHoverTickForAutomationTest calls the
// SAME HandleHoverTransition/TickHoverFade NativeTick does) -- only the cursor-to-transition
// mapping is bypassed, and that mapping is already covered by GV2RichTextSpanHoverDetectorTests.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2HoverFadeAppearLeaveCancelContract,
    "GV2.Runtime.Presentation.HoverFadeAppearLeaveCancel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2HoverFadeAppearLeaveCancelContract::RunTest(const FString& Parameters)
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
    TestNotNull(TEXT("RichText widget instantiates"), RichText);
    UGV2ScreenWidgetBase* HoverScreenA = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
    UGV2ScreenWidgetBase* HoverScreenB = CreateWidget<UGV2ScreenWidgetBase>(TestWorld, UGV2ScreenWidgetBase::StaticClass());
    TestNotNull(TEXT("Hover screen A instantiates"), HoverScreenA);
    TestNotNull(TEXT("Hover screen B instantiates"), HoverScreenB);
    if (RichText == nullptr || HoverScreenA == nullptr || HoverScreenB == nullptr)
    {
        return false;
    }

    const FName SpanIdA(TEXT("span_a"));
    const FName SpanIdB(TEXT("span_b"));
    FGV2RichTextSpanViewModel SpanA;
    SpanA.SpanId = SpanIdA;
    SpanA.Key = SpanIdA;
    SpanA.Hover.ScreenId = TEXT("core:screen.test_a");
    SpanA.Hover.ScreenWidget = HoverScreenA;
    SpanA.Hover.Duration = 2.0f;

    FGV2RichTextSpanViewModel SpanB;
    SpanB.SpanId = SpanIdB;
    SpanB.Key = SpanIdB;
    SpanB.Hover.ScreenId = TEXT("core:screen.test_b");
    SpanB.Hover.ScreenWidget = HoverScreenB;
    SpanB.Hover.Duration = 4.0f; // double A's, to prove duration is content, not a constant.

    RichText->ApplySpans({SpanA, SpanB});

    // 1. Appear: opacity ramps from 0 toward 1 over the span's OWN declared duration (2s).
    RichText->SimulateHoverTickForAutomationTest(EGV2SpanHoverTransition::Began, SpanIdA, 0.5f);
    TestEqual(TEXT("Fade stage is still Appearing after only 0.5s of a 2s appear"),
        RichText->GetHoverFadeStageForAutomationTest(), EGV2HoverFadeStage::Appearing);
    const float OpacityAfterQuarterSecond = HoverScreenA->GetRenderOpacity();
    TestTrue(TEXT("Opacity moved off zero after 0.5s of a 2s appear"), OpacityAfterQuarterSecond > 0.0f && OpacityAfterQuarterSecond < 1.0f);
    TestEqual(TEXT("Opacity after 0.5s/2s matches the linear rate"), OpacityAfterQuarterSecond, 0.25f, KINDA_SMALL_NUMBER);

    RichText->SimulateHoverTickForAutomationTest(EGV2SpanHoverTransition::None, NAME_None, 1.5f);
    TestEqual(TEXT("Opacity reaches 1.0 once the full 2s has elapsed"), HoverScreenA->GetRenderOpacity(), 1.0f);
    TestEqual(TEXT("Fade stage returns to None once fully appeared (steady, not still ticking)"),
        RichText->GetHoverFadeStageForAutomationTest(), EGV2HoverFadeStage::None);

    // 2. PEP-08 Done: the window does NOT disappear merely because "duration" worth of time
    // has passed while still hovered -- only reaching zero opacity during a Leaving fade
    // closes it, never a timer running out on its own.
    RichText->SimulateHoverTickForAutomationTest(EGV2SpanHoverTransition::None, NAME_None, 10.0f);
    TestFalse(TEXT("The window is still open long after its duration, as long as it is still hovered"),
        RichText->GetActiveHoverInstanceKeyForAutomationTest().IsNone());
    TestEqual(TEXT("Opacity remains 1.0 while steadily hovered"), HoverScreenA->GetRenderOpacity(), 1.0f);

    // 3. Leave: opacity ramps back down.
    RichText->SimulateHoverTickForAutomationTest(EGV2SpanHoverTransition::Ended, SpanIdA, 0.5f);
    const float OpacityAfterHalfSecondLeaving = HoverScreenA->GetRenderOpacity();
    TestEqual(TEXT("Opacity after 0.5s of a 2s leave matches the linear rate"), OpacityAfterHalfSecondLeaving, 0.75f, KINDA_SMALL_NUMBER);

    // 4. Cancel: cursor returns to the SAME span while leaving -- resumes appearing from
    // the CURRENT opacity (0.75), not from zero and not with a snapped jump to 1.
    RichText->SimulateHoverTickForAutomationTest(EGV2SpanHoverTransition::Began, SpanIdA, 0.0f);
    TestEqual(TEXT("Cancel resumes from the exact opacity the leave had reached, not from zero"),
        HoverScreenA->GetRenderOpacity(), OpacityAfterHalfSecondLeaving);
    TestFalse(TEXT("Cancel does not close the window"), RichText->GetActiveHoverInstanceKeyForAutomationTest().IsNone());

    RichText->SimulateHoverTickForAutomationTest(EGV2SpanHoverTransition::None, NAME_None, 0.25f);
    // Remaining distance to 1.0 was 0.25 (1.0 - 0.75); at the same rate (1/2s), covering it
    // takes 0.5s -- proportional to the remainder, not a fresh 2s appear from zero.
    TestEqual(TEXT("Resumed appear covers the remaining distance at the same rate"),
        HoverScreenA->GetRenderOpacity(), 0.875f, KINDA_SMALL_NUMBER);

    const FName InstanceKeyA = RichText->GetActiveHoverInstanceKeyForAutomationTest();

    // 5. A real leave-to-close: this time let it run all the way to zero.
    RichText->SimulateHoverTickForAutomationTest(EGV2SpanHoverTransition::Ended, SpanIdA, 3.0f); // overshoots on purpose
    TestTrue(TEXT("The window physically closes once opacity reaches zero"),
        RichText->GetActiveHoverInstanceKeyForAutomationTest().IsNone());
    TestFalse(
        TEXT("The closed window is no longer a real overlay_stack child"),
        Runtime->GetActiveGameShell() != nullptr
            && Runtime->GetActiveGameShell()->GetScreensInLayer(UGV2GameShellWidgetBase::LayerOverlayStack)
                .Contains(Cast<UUserWidget>(HoverScreenA)));

    // 6. Multiple hover/unhover cycles on DIFFERENT spans in sequence do not leave more than
    // one window open at a time (no second instance accumulating).
    RichText->SimulateHoverTickForAutomationTest(EGV2SpanHoverTransition::Began, SpanIdB, 0.0f);
    const FName InstanceKeyB = RichText->GetActiveHoverInstanceKeyForAutomationTest();
    TestFalse(TEXT("Hovering span B opens its own window"), InstanceKeyB.IsNone());
    TestNotEqual(TEXT("Span B's window is a distinct instance from span A's (already closed)"), InstanceKeyB, InstanceKeyA);
    RichText->SimulateHoverTickForAutomationTest(EGV2SpanHoverTransition::Began, SpanIdA, 0.0f);
    // Began on a DIFFERENT span while B is open (Appearing, not Leaving) closes B and opens
    // a fresh A -- exactly one window open at any moment, never two live at once here.
    const FName InstanceKeyA2 = RichText->GetActiveHoverInstanceKeyForAutomationTest();
    TestFalse(TEXT("Switching to span A opens a window for it"), InstanceKeyA2.IsNone());
    TestNotEqual(TEXT("Switching spans closed B's window first, not left both open"), InstanceKeyA2, InstanceKeyB);

    // 7. Duration is read from data, not a C++ constant: span B's window (4s) has NOT yet
    // reached full opacity after the SAME elapsed time A's own 2s window did.
    RichText->SimulateHoverTickForAutomationTest(EGV2SpanHoverTransition::Ended, SpanIdA, 0.0f);
    RichText->SimulateHoverTickForAutomationTest(EGV2SpanHoverTransition::Began, SpanIdB, 0.0f);
    RichText->SimulateHoverTickForAutomationTest(EGV2SpanHoverTransition::None, NAME_None, 2.0f);
    TestTrue(
        TEXT("A 4s-duration window is not yet fully opaque after only 2s -- duration is per-span content, not one C++ literal"),
        HoverScreenB->GetRenderOpacity() < 1.0f);
    TestEqual(TEXT("Span B's own 4s duration produces the expected halfway opacity"), HoverScreenB->GetRenderOpacity(), 0.5f, KINDA_SMALL_NUMBER);

    return true;
}

// PEP-08: "Длительность берётся из данных; перечислитель -- отсутствие числового литерала
// длительности в C++." A text scan, not a reading: HoverFadeDurationSeconds may be assigned
// only from Span->Hover.Duration (the content-declared value already threaded through
// PEP-06C's own Prepare path), never a numeric literal.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2HoverFadeDurationHasNoCppLiteralContract,
    "GV2.Runtime.Presentation.HoverFadeDurationHasNoCppLiteral",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2HoverFadeDurationHasNoCppLiteralContract::RunTest(const FString& Parameters)
{
    const FString FilePath = FPaths::Combine(
        FPaths::ProjectDir(),
        TEXT("Source/GV2PresentationApply/Private/UI/GV2RichTextWidgetBase.cpp"));
    FString Source;
    TestTrue(TEXT("GV2RichTextWidgetBase.cpp is readable"), FFileHelper::LoadFileToString(Source, *FilePath));
    if (Source.IsEmpty())
    {
        return false;
    }

    TArray<FString> Lines;
    Source.ParseIntoArrayLines(Lines);

    // The one place a value actually traceable to content is computed: a local variable
    // read from Span->Hover.Duration (PEP-06C's own field, already threaded through Prepare).
    const bool bHasContentDerivedLocal = Source.Contains(TEXT("DurationSeconds = FMath::Max(Span->Hover.Duration"));
    TestTrue(TEXT("A local duration variable is derived from Span->Hover.Duration somewhere in this file"), bHasContentDerivedLocal);

    int32 NonResetAssignmentCount = 0;
    for (const FString& Line : Lines)
    {
        if (!Line.Contains(TEXT("HoverFadeDurationSeconds =")) && !Line.Contains(TEXT("HoverFadeDurationSeconds=")))
        {
            continue;
        }
        const FString Trimmed = Line.TrimStartAndEnd();
        if (Trimmed.Contains(TEXT("= 0.0f;")))
        {
            // A teardown reset (CloseActiveHoverOverlay), not a duration value -- not what
            // this scan is checking for.
            continue;
        }
        ++NonResetAssignmentCount;
        TestTrue(
            *FString::Printf(TEXT("HoverFadeDurationSeconds's real assignment reads from the content-derived local, not a literal: '%s'"), *Trimmed),
            Trimmed.Contains(TEXT("= DurationSeconds;")));
    }
    TestEqual(TEXT("Exactly one non-reset HoverFadeDurationSeconds assignment exists in this file"), NonResetAssignmentCount, 1);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
