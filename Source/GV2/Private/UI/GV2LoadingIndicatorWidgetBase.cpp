#include "UI/GV2LoadingIndicatorWidgetBase.h"

#include "Components/CircularThrobber.h"
#include "UI/GV2UiTheme.h"

void UGV2LoadingIndicatorWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyCentralStyle_Implementation();
}

bool UGV2LoadingIndicatorWidgetBase::ApplyCentralStyle_Implementation()
{
    UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    if (Theme == nullptr || LoadingIndicator == nullptr)
    {
        return false;
    }
    LoadingIndicator->SetImage(Theme->LoadingIndicatorBrush);
    LoadingIndicator->SetNumberOfPieces(Theme->LoadingIndicatorPieces);
    LoadingIndicator->SetPeriod(Theme->LoadingIndicatorPeriod);
    LoadingIndicator->SetRadius(Theme->LoadingIndicatorRadius);
    return true;
}
