#pragma once

#include "UI/GV2ImageResourceCatalog.h"

class UImage;

class GV2_API FGV2ImagePresentation
{
public:
    // STATUS-012 (ADR-0042, INV-P5): the apply half, with no authority access. A
    // prepared plan already carries the resolved resource, so application never
    // asks the catalog again -- and therefore cannot apply something other than
    // what preparation validated.
    static bool ApplyResolved(
        UImage* Widget,
        const FGV2ResolvedImageResource& Resolved,
        EGV2PrimitiveScalePolicy ScalePolicy,
        TOptional<float> FixedAspectRatio,
        FString& OutError);

    // Resolve + ApplyResolved, for callers that legitimately resolve: preparation,
    // and widget-local paths outside a presentation transaction (NativePreConstruct,
    // Blueprint-facing apply). Not reachable from the application phase.
    static bool ResolveAndApply(
        UImage* Widget,
        const FString& ResourceId,
        EGV2PrimitiveScalePolicy ScalePolicy,
        TOptional<float> FixedAspectRatio,
        FGV2ResolvedImageResource& OutResource,
        FString& OutError);
};
