#include "UI/GV2ButtonWidgetBase.h"

#include "CommonTextBlock.h"
#include "GV2WidgetTextApply.h"
#include "UI/GV2UiInteractionEmitter.h"
#include "UI/GV2UiCapability.h"

void UGV2ButtonWidgetBase::RefreshPreparedViewportPresentation(float ViewportHeight)
{
    if (CurrentTextStyleToken.IsNone() && bHasPreparedDefaultLabelScale)
    {
        FGV2WidgetTextApply::RefreshFont(LabelText, PreparedDefaultLabelScale, ViewportHeight);
        return;
    }
    FGV2WidgetTextApply::RefreshFont(LabelText, CurrentTextViewModel, ViewportHeight);
}

void UGV2ButtonWidgetBase::RestorePreparedLabelPresentation()
{
    if (LabelText == nullptr)
    {
        return;
    }

    if (!CurrentTextStyleToken.IsNone() && CurrentTextViewModel.bHasResolvedPresentation)
    {
        // CommonUI changes its current text style for every control-state transition and
        // while SetStyle rebuilds the Slate styles. Desired presentation remains the owner
        // of semantic typography, so restore the complete prepared text payload afterwards.
        FGV2WidgetTextApply::Apply(LabelText, CurrentTextViewModel);
        return;
    }

    if (PreparedDefaultLabelStyle != nullptr && bHasPreparedDefaultLabelScale)
    {
        LabelText->SetStyle(PreparedDefaultLabelStyle);
        FGV2WidgetTextApply::RefreshFont(
            LabelText,
            PreparedDefaultLabelScale,
            GV2PresentationApply::ResolveLiveViewportHeight(
                this,
                PreparedDefaultLabelScale.ReferenceViewportHeight));
    }
}

void UGV2ButtonWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    // PSC-10B: runtime style arrives as FPreparedButtonStyle. Nothing here re-applies it at
    // design time: a button's serialized CommonUI style and its label's serialized style
    // already render on their own. See UGV2SeparatorWidgetBase::NativePreConstruct.
}

void UGV2ButtonWidgetBase::NativeOnCurrentTextStyleChanged()
{
    Super::NativeOnCurrentTextStyleChanged();
    RestorePreparedLabelPresentation();
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
    PreparedDefaultLabelScale = InDefaultLabelScale;
    PreparedDefaultLabelStyle = InDefaultLabelStyle;
    bHasPreparedDefaultLabelScale = true;
    if (InButtonStyle != nullptr)
    {
        SetStyle(InButtonStyle);
    }
    // SetStyle can synchronously publish a CommonUI text-style change. Repeat the restore
    // even when the style class was already current and CommonUI therefore emitted nothing.
    RestorePreparedLabelPresentation();
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
