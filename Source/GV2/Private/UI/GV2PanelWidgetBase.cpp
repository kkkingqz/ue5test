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
    // PSC-10B: this implementation applies nothing from the Theme. The fetch that used
    // to stand here read the configured Theme only to null-check it and threw the value
    // away -- a value obtained and discarded, the same family as ResourceIcon /
    // ApplyOptionalXxx / OnBindingInvoked / bFatal. It also inflated this task's scope,
    // because the class has no central style payload at all.
    return true;
}
