#include "UI/GV2ProgressBarWidgetBase.h"

#include "CommonTextBlock.h"
#include "Components/ProgressBar.h"
#include "UI/GV2UiTheme.h"

void UGV2ProgressBarWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyCentralStyle_Implementation();
}

void UGV2ProgressBarWidgetBase::ApplyProgress(const float Percent)
{
    if (ProgressBar != nullptr)
    {
        ProgressBar->SetPercent(FMath::Clamp(Percent, 0.0f, 1.0f));
    }
    CurrentPercent = Percent;
}

float UGV2ProgressBarWidgetBase::GetProgress() const
{
    return ProgressBar != nullptr ? ProgressBar->GetPercent() : CurrentPercent;
}

bool UGV2ProgressBarWidgetBase::ApplyCentralStyle_Implementation()
{
    UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    if (Theme == nullptr || ProgressBar == nullptr)
    {
        return false;
    }
    ProgressBar->SetWidgetStyle(Theme->ProgressBarStyle);
    ProgressBar->SetFillColorAndOpacity(Theme->ProgressFillColor);
    return true;
}

FGV2ScreenFieldDescriptor UGV2ProgressBarWidgetBase::GetScreenFieldDescriptor_Implementation() const
{
    FGV2ScreenFieldDescriptor Desc;
    Desc.FieldId = FieldId;
    Desc.SchemaId = SchemaId;
    Desc.bRequired = bIsRequired;
    return Desc;
}

bool UGV2ProgressBarWidgetBase::CanApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value) const
{
    return Value.SchemaId == SchemaId && Value.ProgressBarValue.Percent >= 0.0f && Value.ProgressBarValue.Percent <= 1.0f;
}

FGV2ScreenFieldValue UGV2ProgressBarWidgetBase::CaptureScreenField_Implementation() const
{
    FGV2ProgressBarViewModel Model;
    Model.Percent = CurrentPercent;
    Model.Label = CurrentLabel;
    return FGV2ScreenFieldValue::MakeProgressBar(FieldId, Model);
}

bool UGV2ProgressBarWidgetBase::ApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value)
{
    if (!CanApplyScreenField_Implementation(Value))
    {
        return false;
    }
    ApplyProgress(Value.ProgressBarValue.Percent);
    CurrentLabel = Value.ProgressBarValue.Label;
    if (LabelText != nullptr)
    {
        LabelText->SetText(CurrentLabel.Text);
    }
    return true;
}

bool UGV2ProgressBarWidgetBase::ResetScreenField_Implementation()
{
    ApplyProgress(0.0f);
    CurrentLabel = {};
    if (LabelText != nullptr)
    {
        LabelText->SetText(FText::GetEmpty());
    }
    return true;
}
