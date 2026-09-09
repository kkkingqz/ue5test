#include "UI/GV2ScrollAreaWidgetBase.h"

#include "UI/GV2UiTheme.h"

void UGV2ScrollAreaWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyCentralStyle_Implementation();
    if (ScrollBox != nullptr)
    {
        ScrollBox->SetOrientation(Orientation);
        ScrollBox->SetScrollBarVisibility(bAlwaysShowScrollbar ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

void UGV2ScrollAreaWidgetBase::ScrollToStart()
{
    if (ScrollBox != nullptr)
    {
        ScrollBox->ScrollToStart();
    }
}

void UGV2ScrollAreaWidgetBase::ScrollToEnd()
{
    if (ScrollBox != nullptr)
    {
        ScrollBox->ScrollToEnd();
    }
}

void UGV2ScrollAreaWidgetBase::SetScrollOffset(float NewOffset)
{
    if (ScrollBox != nullptr)
    {
        ScrollBox->SetScrollOffset(NewOffset);
    }
}

float UGV2ScrollAreaWidgetBase::GetScrollOffset() const
{
    return ScrollBox != nullptr ? ScrollBox->GetScrollOffset() : 0.0f;
}

bool UGV2ScrollAreaWidgetBase::ApplyCentralStyle_Implementation()
{
    // PSC-10B: this implementation applies nothing from the Theme. The fetch that used
    // to stand here read the configured Theme only to null-check it and threw the value
    // away -- a value obtained and discarded, the same family as ResourceIcon /
    // ApplyOptionalXxx / OnBindingInvoked / bFatal. It also inflated this task's scope,
    // because the class has no central style payload at all.
    return true;
}
