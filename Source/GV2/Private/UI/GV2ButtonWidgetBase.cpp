#include "UI/GV2ButtonWidgetBase.h"

#include "CommonTextBlock.h"
#include "UI/GV2UiTheme.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2UiInteractionEmitter.h"

void UGV2ButtonWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyCentralStyle_Implementation();
}

void UGV2ButtonWidgetBase::ApplyButtonModel(const FGV2ButtonViewModel& InButtonModel)
{
    check(LabelText != nullptr);

    ButtonModel = InButtonModel;
    check(UGV2TextPipeline::Apply(LabelText, ButtonModel.Text));
    SetIsEnabled(ButtonModel.Binding.IsValid());
}

FGV2ButtonViewModel UGV2ButtonWidgetBase::GetButtonModel() const
{
    return ButtonModel;
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
    const TSubclassOf<UCommonTextStyle> LabelStyle = ButtonModel.Text.StyleToken.IsNone()
        ? Theme->ButtonLabelStyle
        : UGV2TextPipeline::ResolveStyleClass(ButtonModel.Text.StyleToken);
    if (LabelStyle == nullptr) return false;
    LabelText->SetStyle(LabelStyle);
    return true;
}

void UGV2ButtonWidgetBase::NativeOnClicked()
{
    Super::NativeOnClicked();

    OnActivated.Broadcast(ButtonModel.Key);
    if (!bAutomaticInteractionSubmission)
    {
        return;
    }

    const EGV2SubmitUiInteractionResult Result =
        FGV2UiInteractionEmitter::Submit(this, ButtonModel.Binding, {});

    OnBindingInvoked.Broadcast(ButtonModel.Binding, Result);
}
