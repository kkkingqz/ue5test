#include "UI/GV2RichTextWidgetBase.h"

#include "CommonRichTextBlock.h"
#include "CommonTextBlock.h"
#include "Blueprint/UserWidget.h"
#include "Components/ScrollBox.h"
#include "Framework/Application/SlateApplication.h"
#include "GV2PresentationApply/GV2PresentationInteractionSink.h"
#include "GV2PresentationApply/PresentationEffectApply.h"
#include "Styling/CoreStyle.h"
#include "UI/GV2RichTextSpanDecorator.h"
#include "UI/GV2ScreenAnchorHost.h"
#include "GV2WidgetTextApply.h"
#include "UI/GV2UiCapability.h"
#include "UI/GV2UiInteractionEmitter.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
// PEP-06B: the hover popover is no longer a Slate tooltip -- NativeTick below drives it as
// an overlay_stack participant instead. This class survives only as the correlation vessel
// CaptureHoverableSpanAnchors needs: FSlateHyperlinkRun::Create's OnGenerateTooltip is the
// sole per-run hook the engine exposes, so a fresh instance closing over this run's span id
// is what lets CaptureHoverableSpanAnchors read a live SRichTextHyperlink child widget's
// GetToolTip() back to a span id. IsEmpty() is unconditionally true so Slate's native
// tooltip popup never opens for these hyperlinks; every other method is an inert stub.
class FGV2RichTextSpanToolTip final : public IToolTip
{
public:
    explicit FGV2RichTextSpanToolTip(const FName InSpanId) : SpanId(InSpanId) {}

    virtual TSharedRef<SWidget> AsWidget() override { return SNullWidget::NullWidget; }
    virtual TSharedRef<SWidget> GetContentWidget() override { return SNullWidget::NullWidget; }
    virtual void SetContentWidget(const TSharedRef<SWidget>&) override {}
    virtual void ResetContentWidget() override {}
    virtual bool IsEmpty() const override { return true; }
    virtual bool IsInteractive() const override { return false; }
    virtual void OnOpening() override {}
    virtual void OnClosed() override {}
    virtual void OnSetInteractiveWindowLocation(FVector2D&) const override {}

    FName GetSpanId() const { return SpanId; }

private:
    FName SpanId;
};
}

void UGV2RichTextWidgetBase::RefreshPreparedViewportPresentation(float ViewportHeight)
{
    if (UCommonRichTextBlock* Renderer = GetRichTextBlock(); PreparedStyle.bIsResolved && Renderer != nullptr)
    {
        const GV2PresentationApply::FPreparedRichTextTokenStyle& TokenStyle =
            FindPreparedTokenStyle(CurrentText.StyleToken);
        if (TokenStyle.bResolved)
        {
            Renderer->SetDefaultTextStyle(ScalePreparedTokenStyleAtHeight(TokenStyle, ViewportHeight));
        }
    }
    // PEP-06B: the hover overlay is now an ordinary overlay_stack participant, discovered
    // and refreshed by RefreshViewportSubtree's own generic WidgetTree->GetAllWidgets
    // recursion (PresentationApplyFacade.cpp) the same way every other nested screen is --
    // no manual forwarding needed here anymore.
}

void UGV2RichTextWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    if (RichTextBlock != nullptr)
    {
        RichTextBlock->SetDecorators({UGV2RichTextSpanDecorator::StaticClass()});
        RichTextBlock->SetAutoWrapText(true);
        RichTextBlock->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
    }
    // PSC-10B: runtime style arrives as FPreparedRichTextStyle; the decorator set and wrap
    // policy above are structural, not style, and stay here. See UGV2SeparatorWidgetBase.
}

void UGV2RichTextWidgetBase::NativeDestruct()
{
    CloseActiveHoverOverlay();
    SpanIndexById.Reset();
    CurrentText = {};
    CurrentSpans.Reset();
    PreparedStyle = {};
    PreparedStyleAnchors.Reset();
    Super::NativeDestruct();
}

void UGV2RichTextWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    const TArray<FGV2RichTextSpanAnchor> Anchors = CaptureHoverableSpanAnchors();
    TOptional<FVector2D> CursorPos;
    if (FSlateApplication::IsInitialized())
    {
        CursorPos = FSlateApplication::Get().GetCursorPos();
    }

    FName TransitionedSpanId;
    const EGV2SpanHoverTransition Transition = AdvanceSpanHoverState(HoverState, Anchors, CursorPos, TransitionedSpanId);
    HandleHoverTransition(Transition, TransitionedSpanId);

    // Reposition every tick, not only on the transition edge: the overlay's own geometry is
    // one frame stale immediately after AttachHostLocalScreen (it has not been arranged in
    // the panel yet this frame), and re-applying every tick self-heals that without needing
    // a special first-frame case. Uses ActiveHoverSpanId, not HoverState.HoveredSpanId --
    // the latter goes back to NAME_None the instant the cursor leaves, exactly when a
    // Leaving fade still needs its anchor tracked.
    if (!ActiveHoverInstanceKey.IsNone() && !ActiveHoverSpanId.IsNone())
    {
        RepositionActiveHoverOverlay(ActiveHoverSpanId, Anchors);
    }

    TickHoverFade(InDeltaTime);
}

// PEP-08: factored out of NativeTick so GV2.Runtime.Presentation.HoverFadeAppearLeaveCancel
// can drive the SAME production transition-handling code a real cursor would, via the
// test-only SimulateHoverTickForAutomationTest seam below -- without needing a controllable
// OS cursor position (AdvanceSpanHoverState's own cursor-to-transition mapping is already
// covered by GV2RichTextSpanHoverDetectorTests).
void UGV2RichTextWidgetBase::HandleHoverTransition(EGV2SpanHoverTransition Transition, FName TransitionedSpanId)
{
    if (Transition == EGV2SpanHoverTransition::Began && HoverFadeStage == EGV2HoverFadeStage::Leaving
        && TransitionedSpanId == ActiveHoverSpanId)
    {
        // PEP-08: cursor returned to the same span -- or to the still-open window itself,
        // since CaptureHoverableSpanAnchors also anchors the open window's own rect under
        // ActiveHoverSpanId -- while it was leaving. Cancel: resume appearing from whatever
        // opacity it already reached, not from zero and not with a snapped jump.
        HoverFadeStage = EGV2HoverFadeStage::Appearing;
    }
    else if (Transition == EGV2SpanHoverTransition::Began || Transition == EGV2SpanHoverTransition::Changed)
    {
        // Changed means a different span took over with no gap reported -- close whatever
        // was open first; Began means nothing was open (or the leave already finished),
        // making the close a no-op.
        CloseActiveHoverOverlay();
        OpenHoverOverlayForSpan(TransitionedSpanId);
    }
    else if (Transition == EGV2SpanHoverTransition::Ended && !ActiveHoverInstanceKey.IsNone())
    {
        // The window itself is not destroyed here -- only the fade direction reverses.
        // TickHoverFade is what actually closes it, once opacity reaches zero.
        HoverFadeStage = EGV2HoverFadeStage::Leaving;
    }
}

#if WITH_DEV_AUTOMATION_TESTS
void UGV2RichTextWidgetBase::SimulateHoverTickForAutomationTest(
    EGV2SpanHoverTransition TransitionForTest,
    FName SpanIdForTest,
    float DeltaTime)
{
    HandleHoverTransition(TransitionForTest, SpanIdForTest);
    TickHoverFade(DeltaTime);
}
#endif

void UGV2RichTextWidgetBase::OpenHoverOverlayForSpan(FName SpanId)
{
    const FGV2RichTextSpanViewModel* Span = FindInteractiveSpan(SpanId);
    UUserWidget* HoverWidget = Span != nullptr ? Span->Hover.ScreenWidget.Get() : nullptr;
    if (HoverWidget == nullptr)
    {
        return;
    }

    UGV2PresentationInteractionSink* Sink = UGV2PresentationInteractionSink::Find(this);
    FName NewInstanceKey;
    FString Error;
    const float DurationSeconds = FMath::Max(Span->Hover.Duration, 0.0f);
    if (Sink == nullptr || !Sink->OpenHoverOverlay(HoverWidget, DurationSeconds, NewInstanceKey, Error))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("RichText hover overlay open failed for span '%s': %s"),
            *SpanId.ToString(),
            Sink != nullptr ? *Error : TEXT("core:diagnostic.ui_consumer.no_interaction_sink"));
        return;
    }

    ActiveHoverInstanceKey = NewInstanceKey;
    ActiveHoverWidget = HoverWidget;
    ActiveHoverSpanId = SpanId;
    HoverFadeDurationSeconds = DurationSeconds;
    HoverFadeStage = EGV2HoverFadeStage::Appearing;
    // Starts at zero unconditionally: this is always a brand-new attach (OpenHoverOverlayForSpan
    // is never called while one is already open for a different span without CloseActiveHoverOverlay
    // running first), so there is no "current" opacity to continue from yet.
    HoverFadeOpacity = 0.0f;
}

void UGV2RichTextWidgetBase::CloseActiveHoverOverlay()
{
    if (ActiveHoverInstanceKey.IsNone())
    {
        return;
    }
    if (UGV2PresentationInteractionSink* Sink = UGV2PresentationInteractionSink::Find(this))
    {
        Sink->CloseHoverOverlay(ActiveHoverInstanceKey);
    }
    ActiveHoverInstanceKey = NAME_None;
    ActiveHoverWidget.Reset();
    ActiveHoverSpanId = NAME_None;
    HoverFadeStage = EGV2HoverFadeStage::None;
    HoverFadeOpacity = 0.0f;
    HoverFadeDurationSeconds = 0.0f;
}

void UGV2RichTextWidgetBase::TickHoverFade(float DeltaTime)
{
    if (HoverFadeStage == EGV2HoverFadeStage::None)
    {
        return;
    }
    UUserWidget* Widget = ActiveHoverWidget.Get();
    if (Widget == nullptr)
    {
        // The widget disappeared out from under the fade (e.g. torn down by a session
        // rebuild) -- nothing left to fade or to close.
        HoverFadeStage = EGV2HoverFadeStage::None;
        return;
    }

    const bool bAppearing = HoverFadeStage == EGV2HoverFadeStage::Appearing;
    if (HoverFadeDurationSeconds <= 0.0f)
    {
        // No authored duration: instant, not a division by zero.
        HoverFadeOpacity = bAppearing ? 1.0f : 0.0f;
    }
    else
    {
        const float Direction = bAppearing ? 1.0f : -1.0f;
        HoverFadeOpacity = FMath::Clamp(
            HoverFadeOpacity + Direction * (DeltaTime / HoverFadeDurationSeconds),
            0.0f,
            1.0f);
    }

    GV2PresentationApply::FGV2PresentationEffectApplyResult ApplyResult;
    FGV2PresentationEffectApply::Apply(
        GV2PresentationApply::EPresentationEffectKind::Transparency,
        Widget,
        HoverFadeOpacity,
        ApplyResult);

    if (bAppearing && HoverFadeOpacity >= 1.0f)
    {
        // Fully visible and steady -- no more ticking needed until a Leaving edge starts.
        HoverFadeStage = EGV2HoverFadeStage::None;
    }
    else if (!bAppearing && HoverFadeOpacity <= 0.0f)
    {
        // The window disappears HERE, at opacity zero -- never by a timer.
        CloseActiveHoverOverlay();
    }
}

void UGV2RichTextWidgetBase::RepositionActiveHoverOverlay(
    FName SpanId,
    const TArray<FGV2RichTextSpanAnchor>& Anchors)
{
    UUserWidget* HoverWidget = ActiveHoverWidget.Get();
    IGV2ScreenAnchorHost* AnchorHost = Cast<IGV2ScreenAnchorHost>(HoverWidget);
    if (AnchorHost == nullptr)
    {
        return;
    }
    const FGV2RichTextSpanAnchor* Anchor = Anchors.FindByPredicate(
        [SpanId](const FGV2RichTextSpanAnchor& Candidate) { return Candidate.SpanId == SpanId; });
    if (Anchor == nullptr)
    {
        return;
    }
    // Below-left of the span's own rect -- an ordinary tooltip-style placement. The author
    // screen's own canvas child controls its final visible size (PEP-06B's frame decision);
    // this only places its anchor corner.
    const FVector2D AnchorPoint(Anchor->Rect.Left, Anchor->Rect.Bottom);
    AnchorHost->SetAnchoredContentPosition(HoverWidget->GetTickSpaceGeometry().AbsoluteToLocal(AnchorPoint));
}

bool UGV2RichTextWidgetBase::ApplyText(const FGV2TextViewModel& InText)
{
    CurrentText = InText;
    // PSC-11: through the accessor, not the bare BindWidget member. The retired adapter
    // redirected a commit to GetRichTextBlock(), whose name lookup finds the renderer on an
    // instance whose member was never bound; reading the member directly would have made
    // this path reject text the previous one applied.
    UCommonRichTextBlock* Renderer = GetRichTextBlock();
    if (Renderer == nullptr)
    {
        return false;
    }
    if (!FGV2WidgetTextApply::ApplyRichText(Renderer, CurrentText, this))
    {
        return false;
    }
    if (RichTextScrollBox != nullptr)
    {
        RichTextScrollBox->ScrollToStart();
    }
    return true;
}

bool UGV2RichTextWidgetBase::ApplySpans(const TArray<FGV2RichTextSpanViewModel>& InSpans)
{
    CurrentSpans = InSpans;
    SpanIndexById.Reset();
    for (int32 Index = 0; Index < CurrentSpans.Num(); ++Index)
    {
        SpanIndexById.Add(CurrentSpans[Index].SpanId, Index);
    }
    return true;
}

void UGV2RichTextWidgetBase::ApplyInteractiveRichText(
    const FGV2TextViewModel& InText,
    const TArray<FGV2RichTextSpanViewModel>& InSpans)
{
    ApplyText(InText);
    ApplySpans(InSpans);
}

bool UGV2RichTextWidgetBase::HasInteractiveSpan(const FName SpanId) const
{
    return SpanIndexById.Contains(SpanId);
}

const FGV2RichTextSpanViewModel* UGV2RichTextWidgetBase::FindInteractiveSpan(
    const FName SpanId) const
{
    const int32* Index = SpanIndexById.Find(SpanId);
    return Index != nullptr && CurrentSpans.IsValidIndex(*Index)
        ? &CurrentSpans[*Index]
        : nullptr;
}

UCommonRichTextBlock* UGV2RichTextWidgetBase::GetRichTextBlock() const
{
    return RichTextBlock != nullptr ? RichTextBlock.Get() : Cast<UCommonRichTextBlock>(GetWidgetFromName(TEXT("RichTextBlock")));
}

namespace
{
// PEP-06A: URichTextBlock/UCommonRichTextBlock hide their SRichTextBlock entirely (no
// public accessor), and SRichTextBlock itself exposes no FTextLayout getter -- there is no
// engine API to ask "give me the geometry of run X" by name. What DOES already exist,
// through completely ordinary Slate mechanisms, is a real SRichTextHyperlink child widget
// per interactive run, arranged every frame by the engine's own FSlateHyperlinkRun --
// GetTypeAsString() is the established way to identify a Slate widget type without RTTI
// when no other type tag is available.
void CollectRichTextHyperlinks(const TSharedRef<SWidget>& Root, TArray<TSharedRef<SWidget>>& OutHyperlinks)
{
    if (Root->GetTypeAsString() == TEXT("SRichTextHyperlink"))
    {
        OutHyperlinks.Add(Root);
        return;
    }
    FChildren* Children = Root->GetChildren();
    if (Children == nullptr)
    {
        return;
    }
    const int32 Num = Children->Num();
    for (int32 Index = 0; Index < Num; ++Index)
    {
        CollectRichTextHyperlinks(Children->GetChildAt(Index), OutHyperlinks);
    }
}
}

TArray<FGV2RichTextSpanAnchor> UGV2RichTextWidgetBase::CaptureHoverableSpanAnchors() const
{
    TArray<FGV2RichTextSpanAnchor> Anchors;
    UCommonRichTextBlock* Block = GetRichTextBlock();
    TSharedPtr<SWidget> CachedWidget = Block != nullptr ? Block->GetCachedWidget() : nullptr;
    if (!CachedWidget.IsValid())
    {
        return Anchors;
    }

    TArray<TSharedRef<SWidget>> Hyperlinks;
    CollectRichTextHyperlinks(CachedWidget.ToSharedRef(), Hyperlinks);

    for (const TSharedRef<SWidget>& Hyperlink : Hyperlinks)
    {
        // PEP-06A: OnGenerateTooltip is the only per-block hook the engine's
        // FSlateHyperlinkRun::Create exposes -- the FGV2RichTextSpanToolTip it attaches
        // here already closes over exactly this widget's span id (see its own doc
        // comment). Reading it back is a correlation lookup, not a use of the tooltip
        // route's own OnOpening/OnClosed timing, which this detector never touches.
        TSharedPtr<IToolTip> Tip = Hyperlink->GetToolTip();
        const FGV2RichTextSpanToolTip* SpanTip = static_cast<FGV2RichTextSpanToolTip*>(Tip.Get());
        if (SpanTip == nullptr)
        {
            continue;
        }
        const FName SpanId = SpanTip->GetSpanId();
        const FGV2RichTextSpanViewModel* Span = FindInteractiveSpan(SpanId);
        if (Span == nullptr || Span->Hover.IsEmpty())
        {
            continue;
        }
        FGV2RichTextSpanAnchor Anchor;
        Anchor.SpanId = SpanId;
        Anchor.Rect = Hyperlink->GetTickSpaceGeometry().GetLayoutBoundingRect();
        Anchors.Add(Anchor);
    }

    // PEP-08: while an overlay is open, its OWN VISIBLE CONTENT rect (never its root, which
    // is always Fill/Fill over the whole layer -- see IGV2ScreenAnchorHost::
    // GetAnchoredContentScreenRect's own doc comment) counts as an anchor for the span it
    // belongs to. Moving the cursor onto the popover itself (not just back onto the source
    // text) then also counts as "still hovering that span" through this same detector, with
    // no cross-widget signal needed. Independent of the hyperlink loop above: this span id's
    // own real anchor may not even be in Anchors right now (e.g. the text scrolled it out of
    // view), and the overlay must still be reachable regardless.
    if (!ActiveHoverSpanId.IsNone())
    {
        if (const IGV2ScreenAnchorHost* AnchorHost = Cast<IGV2ScreenAnchorHost>(ActiveHoverWidget.Get()))
        {
            const FSlateRect ContentRect = AnchorHost->GetAnchoredContentScreenRect();
            if (!ContentRect.IsEmpty())
            {
                FGV2RichTextSpanAnchor OverlayAnchor;
                OverlayAnchor.SpanId = ActiveHoverSpanId;
                OverlayAnchor.Rect = ContentRect;
                Anchors.Add(OverlayAnchor);
            }
        }
    }

    return Anchors;
}

const FGV2RichTextSpanAnchor* UGV2RichTextWidgetBase::HitTestSpanAnchors(
    TArrayView<const FGV2RichTextSpanAnchor> Anchors,
    const FVector2D& Point)
{
    for (const FGV2RichTextSpanAnchor& Anchor : Anchors)
    {
        if (Anchor.Rect.ContainsPoint(Point))
        {
            return &Anchor;
        }
    }
    return nullptr;
}

EGV2SpanHoverTransition UGV2RichTextWidgetBase::AdvanceSpanHoverState(
    FGV2SpanHoverState& State,
    TArrayView<const FGV2RichTextSpanAnchor> Anchors,
    const TOptional<FVector2D>& Point,
    FName& OutSpanId)
{
    OutSpanId = NAME_None;
    const FGV2RichTextSpanAnchor* Hit = Point.IsSet() ? HitTestSpanAnchors(Anchors, Point.GetValue()) : nullptr;
    const FName NewSpanId = Hit != nullptr ? Hit->SpanId : NAME_None;
    const FName PreviousSpanId = State.HoveredSpanId;

    if (NewSpanId == PreviousSpanId)
    {
        return EGV2SpanHoverTransition::None;
    }

    const bool bWasHovering = !PreviousSpanId.IsNone();
    const bool bNowHovering = !NewSpanId.IsNone();
    State.HoveredSpanId = NewSpanId;

    if (!bWasHovering && bNowHovering)
    {
        OutSpanId = NewSpanId;
        return EGV2SpanHoverTransition::Began;
    }
    if (bWasHovering && !bNowHovering)
    {
        // The span that ended, not NAME_None -- a caller closing whatever popover it opened
        // for PreviousSpanId needs to know which one that was.
        OutSpanId = PreviousSpanId;
        return EGV2SpanHoverTransition::Ended;
    }
    // bWasHovering && bNowHovering, different span ids.
    OutSpanId = NewSpanId;
    return EGV2SpanHoverTransition::Changed;
}

EGV2SubmitUiInteractionResult UGV2RichTextWidgetBase::SubmitSpanInteraction(
    const FName SpanId)
{
    const FGV2RichTextSpanViewModel* Span = FindInteractiveSpan(SpanId);
    EGV2SubmitUiInteractionResult Result = EGV2SubmitUiInteractionResult::InvalidBindingHandle;
    FGV2UiBindingHandle Binding;
    if (Span != nullptr)
    {
        Binding = Span->Binding;
        if (Binding.IsValid())
        {
            Result = FGV2UiInteractionEmitter::Submit(this, Binding, {});
        }
    }
    OnSpanInvoked.Broadcast(SpanId, Binding, Result);
    return Result;
}

TSharedRef<IToolTip> UGV2RichTextWidgetBase::CreateSpanToolTip(const FName SpanId)
{
    return MakeShared<FGV2RichTextSpanToolTip>(SpanId);
}

const GV2PresentationApply::FPreparedRichTextTokenStyle& UGV2RichTextWidgetBase::FindPreparedTokenStyle(FName StyleToken) const
{
    if (!StyleToken.IsNone()
        && StyleToken != FName(TEXT("default"))
        && StyleToken != PreparedStyle.DefaultTokenName)
    {
        if (const GV2PresentationApply::FPreparedRichTextTokenStyle* Found = PreparedStyle.StyleByToken.Find(StyleToken))
        {
            return *Found;
        }
    }
    return PreparedStyle.DefaultToken;
}

TSubclassOf<UCommonTextStyle> UGV2RichTextWidgetBase::ResolvePreparedStyleClass(FName StyleToken) const
{
    const GV2PresentationApply::FPreparedRichTextTokenStyle& TokenStyle = FindPreparedTokenStyle(StyleToken);
    return TokenStyle.StyleClass != nullptr ? TokenStyle.StyleClass : PreparedStyle.DefaultStyleClass;
}

FTextBlockStyle UGV2RichTextWidgetBase::ScalePreparedTokenStyle(
    const GV2PresentationApply::FPreparedRichTextTokenStyle& TokenStyle) const
{
    return ScalePreparedTokenStyleAtHeight(
        TokenStyle,
        GV2PresentationApply::ResolveLiveViewportHeight(
            this,
            PreparedStyle.ScalePolicy.ReferenceViewportHeight));
}

FTextBlockStyle UGV2RichTextWidgetBase::ScalePreparedTokenStyleAtHeight(
    const GV2PresentationApply::FPreparedRichTextTokenStyle& TokenStyle,
    float ViewportHeight) const
{
    FTextBlockStyle Result = TokenStyle.BaseStyle;
    if (TokenStyle.UnscaledFontSize > 0.0f)
    {
        GV2PresentationApply::FPreparedTextScalePolicy Policy = PreparedStyle.ScalePolicy;
        Policy.BaseFontSize = TokenStyle.UnscaledFontSize;
        Result.SetFontSize(GV2PresentationApply::EvaluatePreparedFontSize(Policy, ViewportHeight));
    }
    return Result;
}

void UGV2RichTextWidgetBase::ApplyRichTextStyleValues(const GV2PresentationApply::FPreparedRichTextStyle& InStyle)
{
    PreparedStyle = InStyle;
    PreparedStyleAnchors.Reset();
    auto AnchorClass = [this](const TSubclassOf<UCommonTextStyle>& StyleClass)
    {
        if (StyleClass != nullptr)
        {
            PreparedStyleAnchors.AddUnique(StyleClass.Get());
        }
    };
    AnchorClass(InStyle.DefaultStyleClass);
    AnchorClass(InStyle.DefaultToken.StyleClass);
    for (const TPair<FName, GV2PresentationApply::FPreparedRichTextTokenStyle>& Pair : InStyle.StyleByToken)
    {
        AnchorClass(Pair.Value.StyleClass);
    }
    if (RichTextBlock == nullptr)
    {
        return;
    }
    const GV2PresentationApply::FPreparedRichTextTokenStyle& TokenStyle =
        FindPreparedTokenStyle(CurrentText.StyleToken);
    if (const TSubclassOf<UCommonTextStyle> Style = ResolvePreparedStyleClass(CurrentText.StyleToken))
    {
        RichTextBlock->SetStyle(Style);
    }
    if (TokenStyle.bResolved)
    {
        RichTextBlock->SetDefaultTextStyle(ScalePreparedTokenStyle(TokenStyle));
    }
}

FTextBlockStyle UGV2RichTextWidgetBase::ResolveRunTextStyle(
    FName Style,
    FName Color,
    FName Size) const
{
    if (PreparedStyle.bIsResolved)
    {
        // PSC-10B: served entirely from prepared tables. This runs inside a Slate decorator
        // during rendering -- there is no transaction in flight and no authority in reach.
        const FName EffectiveStyle = Style.IsNone() ? CurrentText.StyleToken : Style;
        FTextBlockStyle Result;
        const GV2PresentationApply::FPreparedRichTextTokenStyle& TokenStyle = FindPreparedTokenStyle(EffectiveStyle);
        if (TokenStyle.bResolved)
        {
            Result = ScalePreparedTokenStyle(TokenStyle);
        }
        else if (RichTextBlock != nullptr)
        {
            Result = RichTextBlock->GetCurrentDefaultTextStyle();
        }
        if (const FLinearColor* ResolvedColor = PreparedStyle.ColorByToken.Find(Color))
        {
            Result.SetColorAndOpacity(*ResolvedColor);
        }
        if (!Size.IsNone())
        {
            GV2PresentationApply::FPreparedTextScalePolicy Policy = PreparedStyle.ScalePolicy;
            Policy.BaseFontSize = PreparedStyle.UnscaledSizeByToken.FindRef(Size);
            Result.SetFontSize(GV2PresentationApply::EvaluatePreparedFontSize(
                Policy,
                GV2PresentationApply::ResolveLiveViewportHeight(this, Policy.ReferenceViewportHeight)));
        }
        return Result;
    }

    FTextBlockStyle Result;
    if (RichTextBlock != nullptr)
    {
        Result = RichTextBlock->GetCurrentDefaultTextStyle();
    }
    return Result;
}

FHyperlinkStyle UGV2RichTextWidgetBase::ResolveInteractiveTextStyle(
    const FTextBlockStyle& RunStyle) const
{
    FHyperlinkStyle Result = FCoreStyle::Get().GetWidgetStyle<FHyperlinkStyle>(
        TEXT("Hyperlink"));
    if (PreparedStyle.bIsResolved)
    {
        Result = PreparedStyle.InteractiveStyle;
        Result.TextStyle = RunStyle;
        Result.TextStyle.SetColorAndOpacity(FSlateColor::UseForeground());
        return Result;
    }
    Result.TextStyle = RunStyle;
    Result.TextStyle.SetColorAndOpacity(FSlateColor::UseForeground());
    return Result;
}

void UGV2RichTextWidgetBase::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    OutBuilder.AddText(TEXT("text"), FName(TEXT("RichTextBlock")));
    OutBuilder.AddCustom(TEXT("spans"), EGV2PreparedUiValueKind::Array, EGV2UiCapabilityTargetType::CustomControl, NAME_None);
    OutBuilder.AddKey(TEXT("key"), NAME_None);
}

bool UGV2RichTextWidgetBase::ApplyPreparedText(
    const GV2PresentationApply::FPreparedTextValue& Value,
    bool /*bIsReset*/,
    FString& OutError)
{
    if (!ApplyText(FGV2TextViewModel::FromPrepared(Value)))
    {
        OutError = TEXT("core:diagnostic.ui_consumer.text_apply_failed: UGV2RichTextWidgetBase::ApplyText rejected the resolved text");
        return false;
    }
    return true;
}

void UGV2RichTextWidgetBase::ApplyPreparedRichTextSpans(
    const TArray<GV2PresentationApply::FPreparedRichTextSpan>& PreparedSpans)
{
    TArray<FGV2RichTextSpanViewModel> Spans;
    Spans.Reserve(PreparedSpans.Num());
    for (const GV2PresentationApply::FPreparedRichTextSpan& FlatSpan : PreparedSpans)
    {
        FGV2RichTextSpanViewModel Span;
        Span.SpanId = FlatSpan.SpanId;
        Span.Key = FlatSpan.Key;
        Span.Hover.ScreenId = FlatSpan.Hover.ScreenId;
        Span.Hover.ScreenWidget = Cast<UUserWidget>(FlatSpan.Hover.ScreenWidget.Get());
        Span.Hover.Duration = FlatSpan.Hover.Duration;
        Span.Binding = FGV2UiBindingHandle::FromSerialized(FlatSpan.SerializedBinding);
        Spans.Add(MoveTemp(Span));
    }
    ApplySpans(Spans);
}
