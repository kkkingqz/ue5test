#pragma once

#include "GV2PresentationApply/PreparedPresentationTransaction.h"
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

    // PSC-10C: turns an ALREADY resolved resource into the ordinary prepared image-host
    // operation; the host widget applies its own scale policy when it receives it. It
    // resolved through FGV2PresentationPrepareContext during Prepare. This replaced
    // ResolveAndApply, whose contract was "consult the process-global session catalog and
    // mutate the widget", i.e. semantic resolution fused to a physical effect.
    static void AppendPreparedImageHostOperation(
        UWidget* TargetWidget,
        const FGV2ResolvedImageResource& Resolved,
        GV2PresentationApply::FGV2PreparedPresentationTransaction& OutTransaction);

    // One implementation of the render-mode projection, shared by every builder of an
    // image-host operation instead of a per-file copy.
    static GV2PresentationApply::EPreparedImageRenderMode ToPreparedRenderMode(EGV2ImageRenderMode RenderMode);

};
