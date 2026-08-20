#include "UI/GV2ModalWidgetBase.h"

#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "UI/GV2ButtonListWidgetBase.h"
#include "UI/GV2UiTheme.h"

void UGV2ModalWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyCentralStyle_Implementation();
}

FGV2ScreenFieldDescriptor UGV2ModalWidgetBase::GetScreenFieldDescriptor_Implementation() const
{
    FGV2ScreenFieldDescriptor Desc;
    Desc.FieldId = FieldId;
    Desc.SchemaId = SchemaId;
    Desc.bRequired = bIsRequired;
    return Desc;
}

bool UGV2ModalWidgetBase::CanApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value) const
{
    return Value.SchemaId == SchemaId;
}

FGV2ScreenFieldValue UGV2ModalWidgetBase::CaptureScreenField_Implementation() const
{
    return FGV2ScreenFieldValue::MakeModal(FieldId, CurrentModel);
}

bool UGV2ModalWidgetBase::ApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value)
{
    if (!CanApplyScreenField_Implementation(Value))
    {
        return false;
    }
    CurrentModel = Value.ModalValue;
    if (TitleText != nullptr)
    {
        TitleText->SetText(CurrentModel.Title.Text);
    }
    if (ContentText != nullptr)
    {
        ContentText->SetText(CurrentModel.Content.Text);
    }
    if (ButtonList != nullptr)
    {
        ButtonList->ApplyButtons(CurrentModel.Buttons);
    }
    return true;
}

bool UGV2ModalWidgetBase::ResetScreenField_Implementation()
{
    CurrentModel = {};
    if (TitleText != nullptr)
    {
        TitleText->SetText(FText::GetEmpty());
    }
    if (ContentText != nullptr)
    {
        ContentText->SetText(FText::GetEmpty());
    }
    if (ButtonList != nullptr)
    {
        ButtonList->ResetButtons();
    }
    return true;
}

bool UGV2ModalWidgetBase::ApplyCentralStyle_Implementation()
{
    UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    if (Theme == nullptr)
    {
        return false;
    }
    return true;
}
