#include "UI/GV2PanelWidgetBase.h"

#include "Components/Border.h"
#include "UI/GV2UiTheme.h"

void UGV2PanelWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyCentralStyle_Implementation();
    if (BackgroundBorder != nullptr)
    {
        if (BackgroundBrush.DrawAs != ESlateBrushDrawType::NoDrawType)
        {
            BackgroundBorder->SetBrush(BackgroundBrush);
        }
        BackgroundBorder->SetPadding(ContentPadding);
    }
}

void UGV2PanelWidgetBase::SetContentPadding(FMargin InPadding)
{
    ContentPadding = InPadding;
    if (BackgroundBorder != nullptr)
    {
        BackgroundBorder->SetPadding(ContentPadding);
    }
}

FMargin UGV2PanelWidgetBase::GetContentPadding() const
{
    return ContentPadding;
}

void UGV2PanelWidgetBase::SetBackgroundBrush(const FSlateBrush& InBrush)
{
    BackgroundBrush = InBrush;
    if (BackgroundBorder != nullptr)
    {
        BackgroundBorder->SetBrush(BackgroundBrush);
    }
}

FSlateBrush UGV2PanelWidgetBase::GetBackgroundBrush() const
{
    return BackgroundBrush;
}

bool UGV2PanelWidgetBase::ApplyCentralStyle_Implementation()
{
    UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    if (Theme == nullptr)
    {
        return false;
    }
    return true;
}
