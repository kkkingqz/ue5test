#include "UI/GV2RichTextWidgetBase.h"

#include "CommonRichTextBlock.h"
#include "CommonTextBlock.h"
#include "Blueprint/UserWidget.h"
#include "Components/ScrollBox.h"
#include "Styling/CoreStyle.h"
#include "UI/GV2RichTextPopoverWidgetBase.h"
#include "UI/GV2RichTextSpanDecorator.h"
#include "UI/GV2TextPipeline.h"
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
        SlateToolTip->ResetContentWidget();
        ActivePopover.Reset();
    }
    virtual void OnSetInteractiveWindowLocation(FVector2D& InOutDesiredLocation) const override
    {
        SlateToolTip->OnSetInteractiveWindowLocation(InOutDesiredLocation);
    }

private:
    TWeakObjectPtr<UGV2RichTextWidgetBase> Owner;
    FName SpanId;
    TSharedRef<SToolTip> SlateToolTip;
    TStrongObjectPtr<UGV2RichTextPopoverWidgetBase> ActivePopover;
};
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
    Super::NativeDestruct();
}

bool UGV2RichTextWidgetBase::ApplyText(const FGV2TextViewModel& InText)
{
    CurrentText = InText;
    if (RichTextBlock == nullptr)
    {
        return false;
    }
    if (!UGV2TextPipeline::ApplyRichText(RichTextBlock, CurrentText, this))
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
    FTextBlockStyle Result = TokenStyle.BaseStyle;
    if (TokenStyle.UnscaledFontSize > 0.0f)
    {
        GV2PresentationApply::FPreparedTextScalePolicy Policy = PreparedStyle.ScalePolicy;
        Policy.BaseFontSize = TokenStyle.UnscaledFontSize;
        Result.SetFontSize(GV2PresentationApply::EvaluatePreparedFontSize(
            Policy,
            GV2PresentationApply::ResolveLiveViewportHeight(this, Policy.ReferenceViewportHeight)));
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
        Span.Hover.Title = FGV2TextViewModel::FromPrepared(FlatSpan.Hover.Title);
        Span.Hover.Description = FGV2TextViewModel::FromPrepared(FlatSpan.Hover.Description);
        Span.Hover.ImageResourceId = FlatSpan.Hover.ImageResourceId;
        Span.Hover.ResolvedImageBrush = FlatSpan.Hover.ImageBrush;
        Span.Hover.bHasResolvedImage = FlatSpan.Hover.bHasResolvedImage;
        Span.Binding = FGV2UiBindingHandle::FromSerialized(FlatSpan.SerializedBinding);
        Spans.Add(MoveTemp(Span));
    }
    ApplySpans(Spans);
}
