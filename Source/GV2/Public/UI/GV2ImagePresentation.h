#pragma once

#include "UI/GV2ImageResourceCatalog.h"

class UImage;

class GV2_API FGV2ImagePresentation
{
public:
    // Optional content never leaves a Screen partially applied: a compatible
    // placeholder is resolved through the same catalog path and the missing ID
    // remains visible in the UE diagnostic log.
    static bool ResolveOptionalAndApply(
        UImage* Widget,
        const FString& ResourceId,
        const FString& PlaceholderResourceId,
        EGV2PrimitiveScalePolicy ScalePolicy,
        TOptional<float> FixedAspectRatio,
        FGV2ResolvedImageResource& OutResource,
        FString& OutError);

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
