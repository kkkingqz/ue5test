#include "UI/GV2ProgressBarWidgetBase.h"

#include "Components/ProgressBar.h"
#include "UI/GV2UiTheme.h"

void UGV2ProgressBarWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyCentralStyle_Implementation();
}

void UGV2ProgressBarWidgetBase::ApplyProgress(const float Percent)
{
    check(ProgressBar != nullptr);
    ProgressBar->SetPercent(FMath::Clamp(Percent, 0.0f, 1.0f));
}

float UGV2ProgressBarWidgetBase::GetProgress() const
{
    return ProgressBar != nullptr ? ProgressBar->GetPercent() : 0.0f;
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
