#include "UI/GV2ListViewWidgetBase.h"

#include "UI/GV2UiTheme.h"

void UGV2ListViewWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyCentralStyle_Implementation();
}

void UGV2ListViewWidgetBase::SetOrientation(EOrientation InOrientation)
{
    Orientation = InOrientation;
}

void UGV2ListViewWidgetBase::ClearEntries()
{
    if (ContainerPanel != nullptr)
    {
        ContainerPanel->ClearChildren();
    }
    ActiveWidgetsByKey.Reset();
}

bool UGV2ListViewWidgetBase::ApplyCentralStyle_Implementation()
{
    UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    if (Theme == nullptr)
    {
        return false;
    }
    return true;
}
