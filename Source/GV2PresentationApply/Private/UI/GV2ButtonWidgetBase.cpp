#include "UI/GV2ButtonWidgetBase.h"

#include "CommonTextBlock.h"
#include "GV2WidgetTextApply.h"
#include "UI/GV2UiInteractionEmitter.h"
#include "UI/GV2UiCapability.h"

void UGV2ButtonWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    // PSC-10B: runtime style arrives as FPreparedButtonStyle. Nothing here re-applies it at
    // design time: a button's serialized CommonUI style and its label's serialized style
    // already render on their own. See UGV2SeparatorWidgetBase::NativePreConstruct.
}

bool UGV2ButtonWidgetBase::ApplyText(const FGV2TextViewModel& InText)
{
    CurrentTextStyleToken = InText.StyleToken;
    CurrentTextViewModel = InText;
    if (InText.NormalizedMarkup.Contains(TEXT("<gv2")))
    {
        return false;
    }
    if (LabelText != nullptr && !FGV2WidgetTextApply::Apply(LabelText, InText))
    {
        return false;
    }
    return true;
}

void UGV2ButtonWidgetBase::SetKey(FName InKey)
{
    GetPropertyHostState().SetKey(InKey);
}

FName UGV2ButtonWidgetBase::GetKey() const
{
    return GetPropertyHostState().GetKey();
}

void UGV2ButtonWidgetBase::SetBindingHandle(const FGV2UiBindingHandle& InBindingHandle)
{
    BindingHandle = InBindingHandle;
    SetIsEnabled(BindingHandle.IsValid());
}

FGV2UiBindingHandle UGV2ButtonWidgetBase::GetBindingHandle() const
{
    return BindingHandle;
}

void UGV2ButtonWidgetBase::SetAutomaticInteractionSubmission(const bool bEnabled)
{
    bAutomaticInteractionSubmission = bEnabled;
}

void UGV2ButtonWidgetBase::ApplyButtonStyleValues(
    TSubclassOf<UCommonButtonStyle> InButtonStyle,
    TSubclassOf<UCommonTextStyle> InDefaultLabelStyle,
    const GV2PresentationApply::FPreparedTextScalePolicy& InDefaultLabelScale)
{
    if (InButtonStyle != nullptr)
    {
        SetStyle(InButtonStyle);
    }
    if (LabelText == nullptr || InDefaultLabelStyle == nullptr)
    {
        return;
    }

    // A label carrying a style token was already styled -- class AND font size -- by that
    // token's own text operation, which ran earlier in this same transaction. Restyling it
    // here is what the old implementation did, resolving the token a second time through
    // the Theme; central style now supplies only the default a token-less label would
    // otherwise have nothing at all from.
    if (!CurrentTextStyleToken.IsNone())
    {
        return;
    }

    LabelText->SetStyle(InDefaultLabelStyle);
    const float ScaledFontSize = GV2PresentationApply::EvaluatePreparedFontSize(
        InDefaultLabelScale,
        GV2PresentationApply::ResolveLiveViewportHeight(this, InDefaultLabelScale.ReferenceViewportHeight));
    FSlateFontInfo FontInfo = LabelText->GetFont();
    if (!FMath::IsNearlyEqual(FontInfo.Size, ScaledFontSize, 0.01f))
    {
        FontInfo.Size = ScaledFontSize;
        LabelText->SetFont(FontInfo);
    }
}

void UGV2ButtonWidgetBase::NativeOnClicked()
{
    Super::NativeOnClicked();

    OnActivated.Broadcast(GetKey());
    if (!bAutomaticInteractionSubmission)
    {
        return;
    }

    const EGV2SubmitUiInteractionResult Result =
        FGV2UiInteractionEmitter::Submit(this, BindingHandle, {});

    OnBindingInvoked.Broadcast(BindingHandle, Result);
}

void UGV2ButtonWidgetBase::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    OutBuilder.AddText(TEXT("text"), FName(TEXT("LabelText")));
    OutBuilder.AddBinding(TEXT("binding"), NAME_None);
    OutBuilder.AddKey(TEXT("key"), NAME_None);
}

bool UGV2ButtonWidgetBase::ApplyPreparedText(
    const GV2PresentationApply::FPreparedTextValue& Value,
    bool /*bIsReset*/,
    FString& OutError)
{
    if (!ApplyText(FGV2TextViewModel::FromPrepared(Value)))
    {
        OutError = TEXT("core:diagnostic.ui_consumer.text_apply_failed: UGV2ButtonWidgetBase::ApplyText rejected the resolved text");
        return false;
    }
    return true;
}
