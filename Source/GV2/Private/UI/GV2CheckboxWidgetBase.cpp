#include "UI/GV2CheckboxWidgetBase.h"

#include "CommonTextBlock.h"
#include "Components/CheckBox.h"
#include "UI/GV2UiTheme.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2UiInteractionEmitter.h"

void UGV2CheckboxWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyCentralStyle_Implementation();
}

void UGV2CheckboxWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();
    if (Checkbox)
    {
        Checkbox->OnCheckStateChanged.AddDynamic(this, &UGV2CheckboxWidgetBase::HandleCheckStateChanged);
    }
}

void UGV2CheckboxWidgetBase::NativeDestruct()
{
    if (Checkbox)
    {
        Checkbox->OnCheckStateChanged.RemoveDynamic(this, &UGV2CheckboxWidgetBase::HandleCheckStateChanged);
    }
    Super::NativeDestruct();
}

bool UGV2CheckboxWidgetBase::ApplyCheckboxModel(const FGV2CheckboxViewModel& CheckboxModel)
{
    if (!Checkbox || !LabelText)
    {
        return false;
    }

    if (!UGV2TextPipeline::Apply(LabelText, CheckboxModel.Text))
    {
        return false;
    }

    AppliedCheckboxModel = CheckboxModel;
    Checkbox->SetIsChecked(CheckboxModel.bIsChecked);
    SetIsEnabled(CheckboxModel.Binding.IsValid());
    return true;
}

bool UGV2CheckboxWidgetBase::CanApplyCheckboxModel(const FGV2CheckboxViewModel& CheckboxModel) const
{
    return Checkbox != nullptr
        && LabelText != nullptr
        && CheckboxModel.Binding.IsValid()
        && UGV2TextPipeline::ResolveStyleClass(CheckboxModel.Text.StyleToken) != nullptr
        && !CheckboxModel.Text.NormalizedMarkup.Contains(TEXT("<gv2"));
}

FGV2ScreenFieldDescriptor UGV2CheckboxWidgetBase::GetScreenFieldDescriptor_Implementation() const
{
    FGV2ScreenFieldDescriptor Descriptor;
    Descriptor.FieldId = ScreenFieldId;
    Descriptor.SchemaId = TEXT("core:schema.ui_field.checkbox.v1");
    Descriptor.bRequired = bScreenFieldRequired;
    return Descriptor;
}

bool UGV2CheckboxWidgetBase::CanApplyScreenField_Implementation(
    const FGV2ScreenFieldValue& FieldValue) const
{
    return FieldValue.SchemaId == TEXT("core:schema.ui_field.checkbox.v1")
        && CanApplyCheckboxModel(FieldValue.CheckboxValue);
}

bool UGV2CheckboxWidgetBase::ApplyScreenField_Implementation(
    const FGV2ScreenFieldValue& FieldValue)
{
    return ApplyCheckboxModel(FieldValue.CheckboxValue);
}

bool UGV2CheckboxWidgetBase::CaptureScreenField_Implementation(
    FGV2ScreenFieldValue& OutFieldValue) const
{
    OutFieldValue = FGV2ScreenFieldValue::MakeCheckbox(ScreenFieldId, AppliedCheckboxModel);
    return true;
}

bool UGV2CheckboxWidgetBase::ResetScreenField_Implementation()
{
    AppliedCheckboxModel = FGV2CheckboxViewModel();
    if (Checkbox)
    {
        Checkbox->SetIsChecked(false);
    }
    if (LabelText)
    {
        LabelText->SetText(FText::GetEmpty());
    }
    SetIsEnabled(false);
    return true;
}

bool UGV2CheckboxWidgetBase::ApplyCentralStyle_Implementation()
{
    UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    if (Theme == nullptr || Checkbox == nullptr || LabelText == nullptr
        || Theme->CheckboxLabelStyle == nullptr)
    {
        return false;
    }

    Checkbox->SetWidgetStyle(Theme->CheckboxStyle);
    
    const TSubclassOf<UCommonTextStyle> LabelStyle = AppliedCheckboxModel.Text.StyleToken.IsNone()
        ? Theme->CheckboxLabelStyle
        : UGV2TextPipeline::ResolveStyleClass(AppliedCheckboxModel.Text.StyleToken);
    
    if (LabelStyle == nullptr) return false;
    LabelText->SetStyle(LabelStyle);

    return true;
}

void UGV2CheckboxWidgetBase::HandleCheckStateChanged(bool bIsChecked)
{
    SubmitCheckboxState(bIsChecked);
}

EGV2SubmitUiInteractionResult UGV2CheckboxWidgetBase::SubmitCheckboxState(bool bIsChecked)
{
    if (!AppliedCheckboxModel.Binding.IsValid())
    {
        return EGV2SubmitUiInteractionResult::InvalidBindingHandle;
    }

    FGV2UiControlValue ControlValue;
    ControlValue.Name = TEXT("is_checked");
    ControlValue.Type = EGV2UiControlValueType::Boolean;
    ControlValue.BooleanValue = bIsChecked;

    const EGV2SubmitUiInteractionResult Result =
        FGV2UiInteractionEmitter::Submit(this, AppliedCheckboxModel.Binding, {ControlValue});

    OnBindingInvoked.Broadcast(AppliedCheckboxModel.Binding, bIsChecked, Result);
    return Result;
}
