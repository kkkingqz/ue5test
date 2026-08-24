#include "UI/GV2ProgressBarWidgetBase.h"
#include "UI/GV2TextPipeline.h"

#include "CommonTextBlock.h"
#include "Components/ProgressBar.h"
#include "UI/GV2UiTheme.h"

void UGV2ProgressBarWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyCentralStyle_Implementation();
}

void UGV2ProgressBarWidgetBase::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    OutBuilder.AddNumber(TEXT("percent"), FName(TEXT("ProgressBar")), 0.0, 1.0);
    if (LabelText != nullptr)
    {
        OutBuilder.AddText(TEXT("label"), FName(TEXT("LabelText")));
    }
    OutBuilder.AddKey(TEXT("key"), NAME_None);
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

bool UGV2ProgressBarWidgetBase::ApplyProgressBarModel(const FGV2ProgressBarViewModel& Model)
{
    if (Model.Label.NormalizedMarkup.Contains(TEXT("<gv2")))
    {
        return false;
    }
    if (LabelText != nullptr && !UGV2TextPipeline::Apply(LabelText, Model.Label))
    {
        return false;
    }
    ApplyProgress(Model.Percent);
    CurrentLabel = Model.Label;
    return true;
}
