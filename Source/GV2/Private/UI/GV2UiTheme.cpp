#include "UI/GV2UiTheme.h"

FName UGV2UiThemeSettings::GetCategoryName() const
{
    return TEXT("Game");
}

UGV2UiTheme* UGV2UiThemeSettings::GetConfiguredTheme()
{
    const UGV2UiThemeSettings* Settings = GetDefault<UGV2UiThemeSettings>();
    return Settings != nullptr ? Settings->ThemeAsset.LoadSynchronous() : nullptr;
}
