#include "UI/GV2ModalWidgetBase.h"
#include "UI/GV2TextPipeline.h"

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

bool UGV2ModalWidgetBase::CaptureScreenField_Implementation(FGV2ScreenFieldValue& OutFieldValue) const
{
    OutFieldValue = FGV2ScreenFieldValue::MakeModal(FieldId, CurrentModel);
    return true;
}

bool UGV2ModalWidgetBase::ApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value)
{
    if (!CanApplyScreenField_Implementation(Value))
    {
        return false;
    }
    const FGV2ModalViewModel& Candidate = Value.ModalValue;
    if (TitleText != nullptr && !UGV2TextPipeline::Apply(TitleText, Candidate.Title))
    {
        return false;
    }
    if (ContentText != nullptr && !UGV2TextPipeline::Apply(ContentText, Candidate.Content))
    {
        return false;
    }
    if (ButtonList != nullptr && !ButtonList->ApplyButtonModels(Candidate.Buttons))
    {
        return false;
    }
    CurrentModel = Candidate;
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
        IGV2DynamicScreenElement::Execute_ResetScreenField(ButtonList);
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
