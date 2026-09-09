#include "UI/GV2LoadingIndicatorWidgetBase.h"

#include "Components/CircularThrobber.h"

void UGV2LoadingIndicatorWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    // PSC-10B: runtime style arrives as FPreparedLoadingIndicatorStyle. UCircularThrobber
    // exposes no getters for its period/radius/piece count, so there is nothing to read
    // back and re-apply -- the serialized values already render, and design-time preview
    // correctly does nothing rather than inventing substitutes for them.
}

void UGV2LoadingIndicatorWidgetBase::ApplyLoadingIndicatorStyleValues(const FSlateBrush& Brush, float Period, float Radius, int32 Pieces)
{
    if (LoadingIndicator != nullptr)
    {
        LoadingIndicator->SetImage(Brush);
        LoadingIndicator->SetNumberOfPieces(Pieces);
        LoadingIndicator->SetPeriod(Period);
        LoadingIndicator->SetRadius(Radius);
    }
}

bool UGV2LoadingIndicatorWidgetBase::ApplyCentralStyle_Implementation()
{
    // PSC-10B: carried by FPreparedLoadingIndicatorStyle, written by ApplyLoadingIndicatorStyleValues.
    return true;
}
