#include "UI/GV2SeparatorWidgetBase.h"

#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "UI/GV2UiTheme.h"

void UGV2SeparatorWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyCentralStyle_Implementation();
}

bool UGV2SeparatorWidgetBase::ApplyCentralStyle_Implementation()
{
    UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    if (Theme == nullptr || SeparatorSizeBox == nullptr || SeparatorImage == nullptr)
    {
        return false;
    }

    SeparatorImage->SetBrush(Theme->SeparatorBrush);
    if (Orientation == Orient_Horizontal)
    {
        SeparatorSizeBox->ClearWidthOverride();
        SeparatorSizeBox->SetHeightOverride(Theme->SeparatorThickness);
    }
    else
    {
        SeparatorSizeBox->SetWidthOverride(Theme->SeparatorThickness);
        SeparatorSizeBox->ClearHeightOverride();
    }
    return true;
}
