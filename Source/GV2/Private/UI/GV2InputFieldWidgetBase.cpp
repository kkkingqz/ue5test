#include "UI/GV2InputFieldWidgetBase.h"

#include "CommonTextBlock.h"
#include "Components/EditableTextBox.h"
#include "UI/GV2UiTheme.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2UiInteractionEmitter.h"

void UGV2InputFieldWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyCentralStyle_Implementation();
}

void UGV2InputFieldWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();
    if (EditableTextBox)
    {
        EditableTextBox->OnTextCommitted.RemoveDynamic(this, &UGV2InputFieldWidgetBase::HandleTextCommitted);
        EditableTextBox->OnTextCommitted.AddDynamic(this, &UGV2InputFieldWidgetBase::HandleTextCommitted);
    }
}

void UGV2InputFieldWidgetBase::NativeDestruct()
{
    if (EditableTextBox)
    {
        EditableTextBox->OnTextCommitted.RemoveDynamic(this, &UGV2InputFieldWidgetBase::HandleTextCommitted);
    }
    Super::NativeDestruct();
}

bool UGV2InputFieldWidgetBase::ApplyInputFieldModel(const FGV2InputFieldViewModel& InputFieldModel)
{
    if (!CanApplyInputFieldModel(InputFieldModel))
    {
        return false;
    }

    if (LabelText)
    {
        if (InputFieldModel.Text.Text.IsEmpty())
        {
            LabelText->SetText(FText::GetEmpty());
        }
        else if (!UGV2TextPipeline::Apply(LabelText, InputFieldModel.Text))
        {
            return false;
        }
    }

    EditableTextBox->SetHintText(InputFieldModel.PlaceholderText.Text);

    AppliedInputFieldModel = InputFieldModel;
    EditableTextBox->SetText(FText::FromString(InputFieldModel.TextValue));
    SetIsEnabled(InputFieldModel.Binding.IsValid());
    return true;
}

bool UGV2InputFieldWidgetBase::CanApplyInputFieldModel(const FGV2InputFieldViewModel& InputFieldModel) const
{
    return EditableTextBox != nullptr
        && InputFieldModel.Binding.IsValid()
        && (LabelText != nullptr || InputFieldModel.Text.Text.IsEmpty())
        && (InputFieldModel.Text.Text.IsEmpty() || UGV2TextPipeline::ResolveStyleClass(InputFieldModel.Text.StyleToken) != nullptr)
        && !InputFieldModel.Text.NormalizedMarkup.Contains(TEXT("<gv2"))
        && !InputFieldModel.PlaceholderText.NormalizedMarkup.Contains(TEXT("<gv2"));
}

FGV2ScreenFieldDescriptor UGV2InputFieldWidgetBase::GetScreenFieldDescriptor_Implementation() const
{
    FGV2ScreenFieldDescriptor Descriptor;
    Descriptor.FieldId = ScreenFieldId;
    Descriptor.SchemaId = TEXT("core:schema.ui_field.input_field.v1");
    Descriptor.bRequired = bScreenFieldRequired;
    return Descriptor;
}

bool UGV2InputFieldWidgetBase::CanApplyScreenField_Implementation(
    const FGV2ScreenFieldValue& FieldValue) const
{
    return FieldValue.FieldId == ScreenFieldId
        && FieldValue.SchemaId == TEXT("core:schema.ui_field.input_field.v1")
        && CanApplyInputFieldModel(FieldValue.InputFieldValue);
}

bool UGV2InputFieldWidgetBase::ApplyScreenField_Implementation(
    const FGV2ScreenFieldValue& FieldValue)
{
    return CanApplyScreenField_Implementation(FieldValue)
        && ApplyInputFieldModel(FieldValue.InputFieldValue);
}

bool UGV2InputFieldWidgetBase::CaptureScreenField_Implementation(
    FGV2ScreenFieldValue& OutFieldValue) const
{
    if (EditableTextBox == nullptr || ScreenFieldId.IsNone())
    {
        return false;
    }
    OutFieldValue = FGV2ScreenFieldValue::MakeInputField(ScreenFieldId, AppliedInputFieldModel);
    return true;
}

bool UGV2InputFieldWidgetBase::ResetScreenField_Implementation()
{
    AppliedInputFieldModel = FGV2InputFieldViewModel();
    if (EditableTextBox)
    {
        EditableTextBox->SetText(FText::GetEmpty());
        EditableTextBox->SetHintText(FText::GetEmpty());
    }
    if (LabelText)
    {
        LabelText->SetText(FText::GetEmpty());
    }
    SetIsEnabled(false);
    return true;
}

bool UGV2InputFieldWidgetBase::ApplyCentralStyle_Implementation()
{
    UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    if (Theme == nullptr || EditableTextBox == nullptr
        || (LabelText != nullptr && Theme->InputFieldLabelStyle == nullptr))
    {
        return false;
    }

    EditableTextBox->WidgetStyle = Theme->InputFieldStyle;

    if (LabelText != nullptr && Theme->InputFieldLabelStyle != nullptr)
    {
        const TSubclassOf<UCommonTextStyle> LabelStyle = AppliedInputFieldModel.Text.StyleToken.IsNone()
            ? Theme->InputFieldLabelStyle
            : UGV2TextPipeline::ResolveStyleClass(AppliedInputFieldModel.Text.StyleToken);

        if (LabelStyle != nullptr)
        {
            LabelText->SetStyle(LabelStyle);
        }
    }

    return true;
}

void UGV2InputFieldWidgetBase::HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
    SubmitTextValue(Text.ToString());
}

EGV2SubmitUiInteractionResult UGV2InputFieldWidgetBase::SubmitTextValue(const FString& NewTextValue)
{
    if (!AppliedInputFieldModel.Binding.IsValid())
    {
        return EGV2SubmitUiInteractionResult::InvalidBindingHandle;
    }

    FGV2UiControlValue ControlValue;
    ControlValue.Name = TEXT("value");
    ControlValue.Type = EGV2UiControlValueType::String;
    ControlValue.StringValue = NewTextValue;

    const EGV2SubmitUiInteractionResult Result =
        FGV2UiInteractionEmitter::Submit(this, AppliedInputFieldModel.Binding, {ControlValue});

    OnBindingInvoked.Broadcast(AppliedInputFieldModel.Binding, NewTextValue, Result);
    return Result;
}
