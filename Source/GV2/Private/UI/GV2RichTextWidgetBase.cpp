#include "UI/GV2RichTextWidgetBase.h"

#include "Bridge/GV2StableIdUE.h"
#include "CommonRichTextBlock.h"
#include "CommonTextBlock.h"
#include "Blueprint/UserWidget.h"
#include "Components/ScrollBox.h"
#include "Framework/Text/RichTextMarkupProcessing.h"
#include "Styling/CoreStyle.h"
#include "UI/GV2RichTextPopoverWidgetBase.h"
#include "UI/GV2RichTextSpanDecorator.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2UiInteractionEmitter.h"
#include "UI/GV2UiTheme.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
bool IsCanonicalSpanId(const FName SpanId)
{
    const FString Value = SpanId.ToString();
    return GV2StableIdUE::IsValidSegment(Value);
}

bool ValidateInteractiveContent(const FGV2InteractiveRichTextViewModel& Content)
{
    TMap<FName, const FGV2RichTextSpanViewModel*> Spans;
    for (const FGV2RichTextSpanViewModel& Span : Content.Spans)
    {
        if (!IsCanonicalSpanId(Span.SpanId) || Spans.Contains(Span.SpanId)
            || (Span.Hover.IsEmpty() && !Span.Binding.IsValid()))
        {
            return false;
        }
        Spans.Add(Span.SpanId, &Span);
    }

    const FString SourceMarkup = Content.Text.Text.ToString();
    FString Markup;
    FString MarkupError;
    if (!UGV2TextPipeline::NormalizeMarkup(SourceMarkup, Markup, MarkupError))
    {
        return false;
    }
    TArray<FTextLineParseResults> Lines;
    FString Processed;
    FDefaultRichTextMarkupParser::GetStaticInstance()->Process(Lines, Markup, Processed);
    TSet<FName> ReferencedSpans;
    for (const FTextLineParseResults& Line : Lines)
    {
        for (const FTextRunParseResults& Run : Line.Runs)
        {
            if (Run.Name != TEXT("gv2"))
            {
                continue;
            }
            const FTextRange* IdRange = Run.MetaData.Find(TEXT("interactive"));
            if (IdRange == nullptr)
            {
                continue;
            }
            const FName SpanId(*Processed.Mid(
                IdRange->BeginIndex,
                IdRange->EndIndex - IdRange->BeginIndex));
            if (!Spans.Contains(SpanId))
            {
                return false;
            }
            ReferencedSpans.Add(SpanId);
        }
    }

    return ReferencedSpans.Num() == Spans.Num();
}

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

        UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
        UClass* PopoverClass = Theme != nullptr
            ? Theme->RichTextPopoverClass.LoadSynchronous()
            : nullptr;
        if (PopoverClass != nullptr && Widget->GetWorld() != nullptr)
        {
            UGV2RichTextPopoverWidgetBase* Popover = CreateWidget<UGV2RichTextPopoverWidgetBase>(
                Widget->GetWorld(),
                PopoverClass);
            if (Popover != nullptr && Popover->InitializePopover(Span->Hover))
            {
                ActivePopover.Reset(Popover);
                SlateToolTip->SetContentWidget(Popover->TakeWidget());
                return;
            }
        }

        FText Fallback = Span->Hover.Title.Text;
        if (!Span->Hover.Description.Text.IsEmpty())
        {
            Fallback = FText::FromString(Fallback.IsEmpty()
                ? Span->Hover.Description.Text.ToString()
                : Fallback.ToString() + TEXT("\n") + Span->Hover.Description.Text.ToString());
        }
        SlateToolTip->SetContentWidget(SNew(STextBlock).Text(Fallback));
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
    ApplyCentralStyle_Implementation();
}

void UGV2RichTextWidgetBase::NativeDestruct()
{
    SpanIndexById.Reset();
    CurrentContent = {};
    Super::NativeDestruct();
}

void UGV2RichTextWidgetBase::ApplyInteractiveRichText(
    const FGV2InteractiveRichTextViewModel& Content)
{
    check(ValidateInteractiveContent(Content));
    CurrentContent = Content;
    if (RichTextBlock == nullptr)
    {
        return;
    }
    if (const TSubclassOf<UCommonTextStyle> Style =
            UGV2TextPipeline::ResolveStyleClass(CurrentContent.Text.StyleToken))
    {
        RichTextBlock->SetStyle(Style);
    }
    FTextBlockStyle DefaultStyle;
    if (UGV2TextPipeline::ResolveStyle(CurrentContent.Text.StyleToken, DefaultStyle, this))
    {
        RichTextBlock->SetDefaultTextStyle(DefaultStyle);
    }
    SpanIndexById.Reset();
    for (int32 Index = 0; Index < CurrentContent.Spans.Num(); ++Index)
    {
        SpanIndexById.Add(CurrentContent.Spans[Index].SpanId, Index);
    }
    FString Markup = CurrentContent.Text.NormalizedMarkup;
    if (Markup.IsEmpty() && !CurrentContent.Text.Text.IsEmpty())
    {
        FString Error;
        check(UGV2TextPipeline::NormalizeMarkup(CurrentContent.Text.Text.ToString(), Markup, Error));
    }
    RichTextBlock->SetText(FText::FromString(Markup));
    if (RichTextScrollBox != nullptr)
    {
        RichTextScrollBox->ScrollToStart();
    }
}

bool UGV2RichTextWidgetBase::HasInteractiveSpan(const FName SpanId) const
{
    return SpanIndexById.Contains(SpanId);
}

const FGV2RichTextSpanViewModel* UGV2RichTextWidgetBase::FindInteractiveSpan(
    const FName SpanId) const
{
    const int32* Index = SpanIndexById.Find(SpanId);
    return Index != nullptr && CurrentContent.Spans.IsValidIndex(*Index)
        ? &CurrentContent.Spans[*Index]
        : nullptr;
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

FTextBlockStyle UGV2RichTextWidgetBase::ResolveRunTextStyle(
    FName Style,
    FName Color,
    FName Size) const
{
    FTextBlockStyle Result;
    const UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    const FName EffectiveStyle = Style.IsNone() ? CurrentContent.Text.StyleToken : Style;
    if (EffectiveStyle == TEXT("default") || EffectiveStyle.IsNone())
    {
        if (Theme != nullptr && Theme->RichTextStyle != nullptr)
        {
            if (const UCommonTextStyle* RichStyle = Cast<UCommonTextStyle>(Theme->RichTextStyle->GetDefaultObject()))
            {
                RichStyle->ToTextBlockStyle(Result);
            }
        }
        else if (RichTextBlock != nullptr)
        {
            Result = RichTextBlock->GetCurrentDefaultTextStyle();
        }
    }
    else if (!UGV2TextPipeline::ResolveStyle(EffectiveStyle, Result, this) && RichTextBlock != nullptr)
    {
        Result = RichTextBlock->GetCurrentDefaultTextStyle();
        const float EffectiveSize = UGV2TextPipeline::ResolveEffectiveFontSize(EffectiveStyle, this);
        Result.SetFontSize(EffectiveSize);
    }
    if (const FLinearColor* ResolvedColor = Theme != nullptr ? Theme->TextColorTokens.Find(Color) : nullptr)
    {
        Result.SetColorAndOpacity(*ResolvedColor);
    }
    if (!Size.IsNone())
    {
        const float EffectiveFontSize = UGV2TextPipeline::ResolveEffectiveFontSize(Size, this);
        Result.SetFontSize(EffectiveFontSize);
    }
    return Result;
}

FHyperlinkStyle UGV2RichTextWidgetBase::ResolveInteractiveTextStyle(
    const FTextBlockStyle& RunStyle) const
{
    FHyperlinkStyle Result = FCoreStyle::Get().GetWidgetStyle<FHyperlinkStyle>(
        TEXT("Hyperlink"));
    if (const UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme())
    {
        Result = Theme->RichTextInteractiveStyle;
    }
    Result.TextStyle = RunStyle;
    Result.TextStyle.SetColorAndOpacity(FSlateColor::UseForeground());
    return Result;
}

FGV2ScreenFieldDescriptor UGV2RichTextWidgetBase::GetScreenFieldDescriptor_Implementation() const
{
    FGV2ScreenFieldDescriptor Descriptor;
    Descriptor.FieldId = ScreenFieldId;
    Descriptor.SchemaId = TEXT("core:schema.ui_field.rich_text.v3");
    Descriptor.bRequired = bScreenFieldRequired;
    return Descriptor;
}

bool UGV2RichTextWidgetBase::CanApplyScreenField_Implementation(
    const FGV2ScreenFieldValue& FieldValue) const
{
    return RichTextBlock != nullptr
        && FieldValue.FieldId == ScreenFieldId
        && FieldValue.SchemaId == TEXT("core:schema.ui_field.rich_text.v3")
        && ValidateInteractiveContent(FieldValue.InteractiveRichTextValue);
}

bool UGV2RichTextWidgetBase::ApplyScreenField_Implementation(
    const FGV2ScreenFieldValue& FieldValue)
{
    if (!CanApplyScreenField_Implementation(FieldValue))
    {
        return false;
    }
    ApplyInteractiveRichText(FieldValue.InteractiveRichTextValue);
    return true;
}

bool UGV2RichTextWidgetBase::CaptureScreenField_Implementation(
    FGV2ScreenFieldValue& OutFieldValue) const
{
    if (RichTextBlock == nullptr || ScreenFieldId.IsNone())
    {
        return false;
    }
    OutFieldValue = FGV2ScreenFieldValue::MakeInteractiveRichText(
        ScreenFieldId,
        CurrentContent);
    return true;
}

bool UGV2RichTextWidgetBase::ResetScreenField_Implementation()
{
    if (RichTextBlock == nullptr)
    {
        return false;
    }
    ApplyInteractiveRichText({});
    return true;
}

bool UGV2RichTextWidgetBase::ApplyCentralStyle_Implementation()
{
    UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    if (Theme == nullptr || RichTextBlock == nullptr || Theme->RichTextStyle == nullptr)
    {
        return false;
    }
    const TSubclassOf<UCommonTextStyle> Style = CurrentContent.Text.StyleToken.IsNone()
        ? Theme->RichTextStyle
        : UGV2TextPipeline::ResolveStyleClass(CurrentContent.Text.StyleToken);
    if (Style == nullptr) return false;
    RichTextBlock->SetStyle(Style);
    FTextBlockStyle DefaultStyle;
    if (UGV2TextPipeline::ResolveStyle(CurrentContent.Text.StyleToken, DefaultStyle, this))
    {
        RichTextBlock->SetDefaultTextStyle(DefaultStyle);
    }
    return true;
}
