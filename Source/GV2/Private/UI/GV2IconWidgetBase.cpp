#include "UI/GV2IconWidgetBase.h"

UGV2IconWidgetBase::UGV2IconWidgetBase()
{
    ScalePolicy = EGV2PrimitiveScalePolicy::PreserveAspect;
    AcceptedRenderMode = EGV2ImageRenderMode::FixedAspect;
    FixedAspectRatio = 1.0f;
}

bool UGV2IconWidgetBase::ApplyIcon(const FString& ResourceId, FString& OutError)
{
    return ApplyImageResource(ResourceId, OutError);
}
