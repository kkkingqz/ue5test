#include "UI/GV2ScrollAreaWidgetBase.h"

void UGV2ScrollAreaWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
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
