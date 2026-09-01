#include "UI/GV2ButtonWidgetBase.h"

#include "CommonTextBlock.h"
#include "UI/GV2UiTheme.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2UiInteractionEmitter.h"
#include "UI/GV2UiCapability.h"

void UGV2ButtonWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyCentralStyle_Implementation();
}

bool UGV2ButtonWidgetBase::ApplyText(const FGV2TextViewModel& InText)
{
    CurrentTextStyleToken = InText.StyleToken;
    CurrentTextViewModel = InText;
    if (InText.NormalizedMarkup.Contains(TEXT("<gv2")))
    {
        return false;
    }
    if (LabelText != nullptr && !UGV2TextPipeline::Apply(LabelText, InText))
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

bool UGV2ButtonWidgetBase::ApplyCentralStyle_Implementation()
{
    UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    if (Theme == nullptr || LabelText == nullptr
        || Theme->ButtonStyle == nullptr || Theme->ButtonLabelStyle == nullptr)
    {
        return false;
    }
    SetStyle(Theme->ButtonStyle);
    const TSubclassOf<UCommonTextStyle> LabelStyle = CurrentTextStyleToken.IsNone()
        ? Theme->ButtonLabelStyle
        : UGV2TextPipeline::ResolveStyleClass(CurrentTextStyleToken);
    if (LabelStyle == nullptr) return false;
    LabelText->SetStyle(LabelStyle);
    const float ScaledFontSize = UGV2TextPipeline::ResolveEffectiveFontSize(CurrentTextStyleToken, this);
    FSlateFontInfo FontInfo = LabelText->GetFont();
    if (!FMath::IsNearlyEqual(FontInfo.Size, ScaledFontSize, 0.01f))
    {
        FontInfo.Size = ScaledFontSize;
        LabelText->SetFont(FontInfo);
    }
    return true;
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
