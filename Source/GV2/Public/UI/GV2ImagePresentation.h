#pragma once

#include "UI/GV2ImageResourceCatalog.h"

class UImage;

class GV2_API FGV2ImagePresentation
{
public:
    static bool ResolveAndApply(
        UImage* Widget,
        const FString& ResourceId,
        EGV2ImageRenderMode AcceptedRenderMode,
        TOptional<float> FixedAspectRatio,
        FGV2ResolvedImageResource& OutResource,
        FString& OutError);

    static bool ResolveAndApply(
        UImage* Widget,
        const FString& ResourceId,
        EGV2PrimitiveScalePolicy ScalePolicy,
        TOptional<float> FixedAspectRatio,
        FGV2ResolvedImageResource& OutResource,
        FString& OutError);
};
