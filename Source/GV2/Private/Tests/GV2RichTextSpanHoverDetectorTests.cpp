#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/GV2PresentationTestFixtures.h"

#include "Tests/GV2ForgeryTestWidgets.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2UiTheme.h"
#include "Widgets/SVirtualWindow.h"

// PEP-06A/06B: FSlateHyperlinkRun::Create takes OnClick/OnGenerateTooltip but no hover
// callback -- IToolTip::OnOpening/OnClosed was the original hover signal. This suite proves
// the replacement: UGV2RichTextWidgetBase::CaptureHoverableSpanAnchors/HitTestSpanAnchors/
// AdvanceSpanHoverState, fed synthetic points against a REAL rendered interactive span (no
// synthesized cursor -- render for real, then query geometry, then do point math by hand,
// the same idiom GV2ContentSmokeTests.cpp already uses). PEP-06B deleted the Slate-tooltip
// popover entirely; FGV2RichTextSpanToolTip::IsEmpty() is now unconditionally true (it no
// longer reflects hover content, only whether Slate should ever show ITS OWN popup, which
// is never), so this test no longer compares against it -- coverage-set equivalence is
// proven directly against CaptureHoverableSpanAnchors's own output below (section 2).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2RichTextSpanHoverDetectorTest,
    "GV2.Runtime.Presentation.RichTextSpanHoverDetector",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2RichTextSpanHoverDetectorTest::RunTest(const FString& Parameters)
{
    using namespace GV2PresentationTestFixtures;

    UGV2UiTheme* Theme = LoadConfiguredThemeForTest();
    TestNotNull(TEXT("Theme is available"), Theme);
    if (Theme == nullptr)
    {
        return false;
    }
    const GV2PresentationApply::FPreparedRichTextStyle PreparedStyle = MakePreparedRichTextStyleForTest(*Theme);
    TestTrue(TEXT("Prepared rich text style resolves"), PreparedStyle.bIsResolved);

    // ADR-0046/TSR-10: a contract test cannot reference a non-core content path (the only
    // interactive RichText WBP in the project lives under /Game/TextSystem, a sample
    // package) -- this forgery populates the same BindWidget sub-widgets a real WBP would,
    // matching UGV2RichTextPopoverBoundTestWidget's own precedent for the same constraint.
    UGV2RichTextBoundTestWidget* RichText = NewObject<UGV2RichTextBoundTestWidget>();
    TestNotNull(TEXT("RichText instantiates"), RichText);
    if (RichText == nullptr)
    {
        return false;
    }
    RichText->BuildBoundSubWidgets();
    RichText->ApplyRichTextStyleValues(PreparedStyle);

    // A span carrying hover content (a resolved nested-screen id, PEP-05's own shape) and a
    // span carrying only a binding -- interactive markup either way, but only the first is
    // one FGV2RichTextSpanToolTip::IsEmpty() ever opens a tooltip for. Both are tagged
    // "interactive" in the markup regardless: NormalizeMarkupCommon/Emit() writes the tag
    // from source markup alone, before FGV2RichTextSpansPropertyConsumer ever validates
    // hover-or-binding.
    FGV2TextViewModel Text = MakeResolvedLiteralTextForTest(*Theme, TEXT(""));
    Text.NormalizedMarkup = TEXT("Hover <gv2 style=\"default\" interactive=\"hover_span\">this</> or click <gv2 style=\"default\" interactive=\"click_span\">that</>.");

    FGV2RichTextSpanViewModel HoverSpan;
    HoverSpan.SpanId = FName(TEXT("hover_span"));
    HoverSpan.Key = HoverSpan.SpanId;
    HoverSpan.Hover.ScreenId = TEXT("core:screen.test_embedded");

    FGV2RichTextSpanViewModel ClickOnlySpan;
    ClickOnlySpan.SpanId = FName(TEXT("click_span"));
    ClickOnlySpan.Key = ClickOnlySpan.SpanId;
    ClickOnlySpan.Binding = FGV2UiBindingHandle::Create(TEXT("cmd_inspect@1:1"));

    RichText->ApplyInteractiveRichText(Text, {HoverSpan, ClickOnlySpan});

    // 1. PEP-06B: the correlation vessel itself is inert now -- both spans' tooltip is
    // unconditionally empty, regardless of hover content. Asserting that directly is what
    // proves Slate's own tooltip popup can never open for either span any more.
    TestTrue(TEXT("Tooltip is empty for the hover-bearing span (Slate popup never opens)"),
        RichText->CreateSpanToolTip(HoverSpan.SpanId)->IsEmpty());
    TestTrue(TEXT("Tooltip is empty for the binding-only span"),
        RichText->CreateSpanToolTip(ClickOnlySpan.SpanId)->IsEmpty());

    // 2. Render for real -- geometry below comes from live Slate arrangement, not a manual
    // layout walk (GV2ContentSmokeTests.cpp's own established idiom).
    TSharedPtr<SWidget> SlateWidget = RichText->TakeWidget();
    TestTrue(TEXT("RichText produces a valid Slate widget"), SlateWidget.IsValid());
    if (!SlateWidget.IsValid())
    {
        return false;
    }
    TSharedRef<SVirtualWindow> VirtualWindow = SNew(SVirtualWindow).Size(FVector2D(800, 200));
    VirtualWindow->SetContent(SlateWidget.ToSharedRef());
    GV2SimulateResponsiveFrame(VirtualWindow, FVector2D(800, 200));

    const TArray<FGV2RichTextSpanAnchor> Anchors = RichText->CaptureHoverableSpanAnchors();
    TestEqual(TEXT("Exactly one anchor is captured (the hover-bearing span only)"), Anchors.Num(), 1);
    if (Anchors.Num() != 1)
    {
        return false;
    }
    TestEqual(TEXT("The captured anchor is the hover-bearing span"), Anchors[0].SpanId, HoverSpan.SpanId);
    TestTrue(TEXT("The captured anchor has a non-degenerate rect"),
        Anchors[0].Rect.GetSize().X > 0.0f && Anchors[0].Rect.GetSize().Y > 0.0f);

    // 3. Hit-test by feeding synthetic points -- no real cursor anywhere in this test.
    const FVector2D InsidePoint = Anchors[0].Rect.GetCenter();
    const FVector2D OutsidePoint = FVector2D(Anchors[0].Rect.GetBottomLeft()) + FVector2D(0.0f, 500.0f);
    TestEqual(TEXT("A point inside the anchor hits it"),
        UGV2RichTextWidgetBase::HitTestSpanAnchors(Anchors, InsidePoint), &Anchors[0]);
    TestNull(TEXT("A point outside every anchor hits nothing"),
        UGV2RichTextWidgetBase::HitTestSpanAnchors(Anchors, OutsidePoint));

    // 4. Hover begin/end/changed as pure state transitions, fed a manual point sequence.
    FGV2SpanHoverState State;
    FName TransitionSpanId;
    TestEqual(TEXT("First poll outside any anchor reports no transition"),
        UGV2RichTextWidgetBase::AdvanceSpanHoverState(State, Anchors, OutsidePoint, TransitionSpanId),
        EGV2SpanHoverTransition::None);

    TestEqual(TEXT("Moving inside the anchor reports Began"),
        UGV2RichTextWidgetBase::AdvanceSpanHoverState(State, Anchors, InsidePoint, TransitionSpanId),
        EGV2SpanHoverTransition::Began);
    TestEqual(TEXT("Began names the entered span"), TransitionSpanId, HoverSpan.SpanId);

    TestEqual(TEXT("Staying inside the same anchor reports no further transition"),
        UGV2RichTextWidgetBase::AdvanceSpanHoverState(State, Anchors, InsidePoint, TransitionSpanId),
        EGV2SpanHoverTransition::None);

    TestEqual(TEXT("Moving back outside reports Ended"),
        UGV2RichTextWidgetBase::AdvanceSpanHoverState(State, Anchors, OutsidePoint, TransitionSpanId),
        EGV2SpanHoverTransition::Ended);
    TestEqual(TEXT("Ended names the span that was left"), TransitionSpanId, HoverSpan.SpanId);

    // Cursor leaving the widget's own bounds entirely (unset Point) must also end a hover.
    TestEqual(TEXT("Re-entering reports Began again"),
        UGV2RichTextWidgetBase::AdvanceSpanHoverState(State, Anchors, InsidePoint, TransitionSpanId),
        EGV2SpanHoverTransition::Began);
    TestEqual(TEXT("An unset point (cursor left the widget) reports Ended"),
        UGV2RichTextWidgetBase::AdvanceSpanHoverState(State, Anchors, TOptional<FVector2D>(), TransitionSpanId),
        EGV2SpanHoverTransition::Ended);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
