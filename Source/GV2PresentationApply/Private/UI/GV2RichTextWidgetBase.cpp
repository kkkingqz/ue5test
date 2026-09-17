#include "UI/GV2RichTextWidgetBase.h"

#include "CommonRichTextBlock.h"
#include "CommonTextBlock.h"
#include "Blueprint/UserWidget.h"
#include "Components/ScrollBox.h"
#include "Styling/CoreStyle.h"
#include "UI/GV2RichTextPopoverWidgetBase.h"
#include "UI/GV2RichTextSpanDecorator.h"
#include "GV2WidgetTextApply.h"
#include "UI/GV2UiCapability.h"
#include "UI/GV2UiInteractionEmitter.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
class FGV2RichTextSpanToolTip final : public IToolTip
{
public:
    FGV2RichTextSpanToolTip(UGV2RichTextWidgetBase* InOwner, const FName InSpanId)
        : Owner(InOwner)
        , SpanId(InSpanId)
        , SlateToolTip(SNew(SToolTip).IsInteractive(true))
    {
    }

    virtual TSharedRef<SWidget> AsWidget() override { return SlateToolTip; }
    virtual TSharedRef<SWidget> GetContentWidget() override { return SlateToolTip->GetContentWidget(); }
    virtual void SetContentWidget(const TSharedRef<SWidget>& InContentWidget) override
    {
        SlateToolTip->SetContentWidget(InContentWidget);
    }
    virtual void ResetContentWidget() override { SlateToolTip->ResetContentWidget(); }
    virtual bool IsEmpty() const override
    {
        const UGV2RichTextWidgetBase* Widget = Owner.Get();
        const FGV2RichTextSpanViewModel* Span = Widget != nullptr
            ? Widget->FindInteractiveSpan(SpanId)
            : nullptr;
        return Span == nullptr || Span->Hover.IsEmpty();
    }
    virtual bool IsInteractive() const override { return true; }
    virtual void OnOpening() override
    {
        UGV2RichTextWidgetBase* Widget = Owner.Get();
        const FGV2RichTextSpanViewModel* Span = Widget != nullptr
            ? Widget->FindInteractiveSpan(SpanId)
            : nullptr;
        if (Widget == nullptr || Span == nullptr || Span->Hover.IsEmpty())
        {
            return;
        }

        const GV2PresentationApply::FPreparedRichTextStyle& PreparedStyle = Widget->GetPreparedRichTextStyle();
        UClass* PopoverClass = PreparedStyle.bIsResolved
            ? Widget->GetPreparedPopoverClass()
            : nullptr;
        if (PopoverClass != nullptr && Widget->GetWorld() != nullptr)
        {
            UGV2RichTextPopoverWidgetBase* Popover = CreateWidget<UGV2RichTextPopoverWidgetBase>(
                Widget->GetWorld(),
                PopoverClass);
            const bool bPopoverInitialized = Popover != nullptr
                && Popover->InitializePopover(Span->Hover, PreparedStyle);
            if (bPopoverInitialized)
            {
                ActivePopover.Reset(Popover);
                Widget->SetActivePopoverForViewportRefresh(Popover);
                SlateToolTip->SetContentWidget(Popover->TakeWidget());
                return;
            }
        }

        UE_LOG(
            LogTemp,
            Error,
            TEXT("RichText hover popover renderer unavailable (class=%s); hover content is not shown"),
            PopoverClass != nullptr ? *PopoverClass->GetPathName() : TEXT("null"));
        SlateToolTip->SetContentWidget(SNullWidget::NullWidget);
    }
    virtual void OnClosed() override
    {
        if (UGV2RichTextWidgetBase* Widget = Owner.Get())
        {
            Widget->ClearActivePopoverForViewportRefresh(ActivePopover.Get());
        }
        SlateToolTip->ResetContentWidget();
        ActivePopover.Reset();
    }
    virtual void OnSetInteractiveWindowLocation(FVector2D& InOutDesiredLocation) const override
    {
        SlateToolTip->OnSetInteractiveWindowLocation(InOutDesiredLocation);
    }

    // PEP-06A: read-only, for CaptureHoverableSpanAnchors's own correlation below --
    // OnGenerateTooltip is the only per-block hook FSlateHyperlinkRun::Create exposes, and
    // this is the one place a fresh instance of this class already closes over the span id
    // for exactly the widget it gets attached to. Never used by anything to change what
    // this class does; the tooltip route itself is untouched.
    FName GetSpanId() const { return SpanId; }

private:
    TWeakObjectPtr<UGV2RichTextWidgetBase> Owner;
    FName SpanId;
    TSharedRef<SToolTip> SlateToolTip;
    TStrongObjectPtr<UGV2RichTextPopoverWidgetBase> ActivePopover;
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
    if (UGV2RichTextPopoverWidgetBase* Popover = ActivePopoverForViewportRefresh.Get())
    {
        Popover->RefreshPreparedViewportPresentation(ViewportHeight);
    }
}

void UGV2RichTextWidgetBase::SetActivePopoverForViewportRefresh(UGV2RichTextPopoverWidgetBase* Popover)
{
    ActivePopoverForViewportRefresh = Popover;
}

void UGV2RichTextWidgetBase::ClearActivePopoverForViewportRefresh(UGV2RichTextPopoverWidgetBase* Popover)
{
    if (ActivePopoverForViewportRefresh.Get() == Popover)
    {
        ActivePopoverForViewportRefresh.Reset();
    }
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
    SpanIndexById.Reset();
    CurrentText = {};
    CurrentSpans.Reset();
    PreparedStyle = {};
    PreparedPopoverClass = nullptr;
    PreparedStyleAnchors.Reset();
    ActivePopoverForViewportRefresh.Reset();
    Super::NativeDestruct();
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
    return MakeShared<FGV2RichTextSpanToolTip>(this, SpanId);
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
    PreparedPopoverClass = InStyle.PopoverClass;
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
        Span.Binding = FGV2UiBindingHandle::FromSerialized(FlatSpan.SerializedBinding);
        Spans.Add(MoveTemp(Span));
    }
    ApplySpans(Spans);
}
