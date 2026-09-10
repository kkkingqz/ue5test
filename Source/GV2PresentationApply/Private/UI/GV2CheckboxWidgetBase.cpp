#include "UI/GV2CheckboxWidgetBase.h"

#include "CommonTextBlock.h"
#include "Components/CheckBox.h"
#include "GV2PresentationApply/GV2WidgetTextApply.h"

void UGV2CheckboxWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    // PSC-10B: runtime style arrives as FPreparedCheckboxStyle; serialized checkbox and
    // label styles already render at design time. See UGV2SeparatorWidgetBase.
}

void UGV2CheckboxWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();
    if (Checkbox)
    {
        Checkbox->OnCheckStateChanged.RemoveDynamic(this, &UGV2CheckboxWidgetBase::HandleCheckStateChanged);
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

void UGV2CheckboxWidgetBase::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    OutBuilder.AddText(TEXT("text"), FName(TEXT("LabelText")));
    OutBuilder.AddBoolean(TEXT("is_checked"), FName(TEXT("Checkbox")));
    OutBuilder.AddBoolean(TEXT("is_read_only"), FName(TEXT("Checkbox")));
    OutBuilder.AddBinding(TEXT("binding"), NAME_None);
    OutBuilder.AddKey(TEXT("key"), NAME_None);
}

void UGV2CheckboxWidgetBase::SetBindingHandle(const FGV2UiBindingHandle& InHandle)
{
    BindingHandle = InHandle;
    SetIsEnabled(BindingHandle.IsValid());
}

FGV2UiBindingHandle UGV2CheckboxWidgetBase::GetBindingHandle() const
{
    return BindingHandle;
}

void UGV2CheckboxWidgetBase::SetIsChecked(bool bInIsChecked)
{
    if (Checkbox)
    {
        Checkbox->SetIsChecked(bInIsChecked);
    }
}

bool UGV2CheckboxWidgetBase::IsChecked() const
{
    return Checkbox ? Checkbox->IsChecked() : false;
}

void UGV2CheckboxWidgetBase::SetIsReadOnly(bool bInIsReadOnly)
{
    bIsReadOnly = bInIsReadOnly;
    if (Checkbox)
    {
        Checkbox->SetIsEnabled(!bIsReadOnly);
    }
}

bool UGV2CheckboxWidgetBase::IsReadOnly() const
{
    return bIsReadOnly;
}

bool UGV2CheckboxWidgetBase::ApplyText(const FGV2TextViewModel& InText)
{
    if (InText.NormalizedMarkup.Contains(TEXT("<gv2")))
    {
        return false;
    }
    AppliedText = InText;
    if (LabelText != nullptr)
    {
        return FGV2WidgetTextApply::Apply(LabelText, InText);
    }
    return true;
}

void UGV2CheckboxWidgetBase::ApplyCheckboxStyleValues(const FCheckBoxStyle& InWidgetStyle, TSubclassOf<UCommonTextStyle> InDefaultLabelStyle)
{
    if (Checkbox != nullptr)
    {
        Checkbox->SetWidgetStyle(InWidgetStyle);
    }
    // See UGV2ButtonWidgetBase::ApplyButtonStyleValues for why a token-carrying label is
    // deliberately left to its own text operation.
    if (LabelText != nullptr && InDefaultLabelStyle != nullptr && AppliedText.StyleToken.IsNone())
    {
        LabelText->SetStyle(InDefaultLabelStyle);
    }
}

void UGV2CheckboxWidgetBase::HandleCheckStateChanged(bool bInIsChecked)
{
    SubmitCheckboxState(bInIsChecked);
}

EGV2SubmitUiInteractionResult UGV2CheckboxWidgetBase::SubmitCheckboxState(bool bInIsChecked)
{
    if (!BindingHandle.IsValid())
    {
        return EGV2SubmitUiInteractionResult::InvalidBindingHandle;
    }

    FGV2UiControlValue ControlValue;
    ControlValue.Name = TEXT("is_checked");
    ControlValue.Type = EGV2UiControlValueType::Boolean;
    ControlValue.BooleanValue = bInIsChecked;

    const EGV2SubmitUiInteractionResult Result =
        FGV2UiInteractionEmitter::Submit(this, BindingHandle, {ControlValue});

    OnBindingInvoked.Broadcast(BindingHandle, bInIsChecked, Result);
    return Result;
}
