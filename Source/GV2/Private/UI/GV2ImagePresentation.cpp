#include "UI/GV2ImagePresentation.h"

#include "Components/Image.h"
#include "Logging/LogMacros.h"

// STATUS-012: pure application. No catalog, no resolution -- everything this needs
// arrives already resolved, so what is applied is exactly what preparation validated.
bool FGV2ImagePresentation::ApplyResolved(
    UImage* Widget,
    const FGV2ResolvedImageResource& Resolved,
    const EGV2PrimitiveScalePolicy ScalePolicy,
    const TOptional<float> FixedAspectRatio,
    FString& OutError)
{
    if (Widget == nullptr)
    {
        OutError = TEXT("Image widget is unavailable.");
        return false;
    }
    if (!IsScalePolicyCompatible(ScalePolicy, Resolved.RenderMode))
    {
        OutError = TEXT("Image resource render mode is incompatible with the primitive scaling policy.");
        return false;
    }
    if (ScalePolicy == EGV2PrimitiveScalePolicy::PreserveAspect && FixedAspectRatio.IsSet()
        && (!FMath::IsFinite(FixedAspectRatio.GetValue()) || FixedAspectRatio.GetValue() <= 0.0f
            || !FMath::IsNearlyEqual(Resolved.FixedAspectRatio, FixedAspectRatio.GetValue(), 0.001f)))
    {
        OutError = TEXT("fixed_aspect resource ratio does not match the target block ratio.");
        return false;
    }
    FSlateBrush ResolvedBrush = Resolved.Brush;
    if (ScalePolicy == EGV2PrimitiveScalePolicy::Tile)
    {
        ResolvedBrush.Tiling = ESlateBrushTileType::Both;
        ResolvedBrush.DrawAs = ESlateBrushDrawType::Image;
    }
    else if (ScalePolicy == EGV2PrimitiveScalePolicy::NineSlice)
    {
        ResolvedBrush.DrawAs = ESlateBrushDrawType::Box;
    }
    else
    {
        ResolvedBrush.Tiling = ESlateBrushTileType::NoTile;
        ResolvedBrush.DrawAs = ESlateBrushDrawType::Image;
    }
    Widget->SetBrush(ResolvedBrush);
    Widget->SetDesiredSizeOverride(ResolvedBrush.ImageSize);
    OutError.Reset();
    return true;
}

// PSC-10C: the prepared-operation builder that replaced ResolveAndApply. It takes an
// ALREADY resolved resource -- the caller obtained it from the session snapshot through
// FGV2PresentationPrepareContext during Prepare -- and appends the ordinary image-host
// operation. No catalog, no lookup, no mutation: the physical write happens later, in the
// same Apply facade every other operation goes through.
void FGV2ImagePresentation::AppendPreparedImageHostOperation(
    UWidget* TargetWidget,
    const FGV2ResolvedImageResource& Resolved,
    GV2PresentationApply::FGV2PreparedPresentationTransaction& OutTransaction)
{
    if (TargetWidget == nullptr)
    {
        return;
    }
    GV2PresentationApply::FPreparedImageHostOperation Operation;
    Operation.TargetWidget = TargetWidget;
    Operation.Resolved.ResourceId = Resolved.ResourceId;
    Operation.Resolved.RenderMode = ToPreparedRenderMode(Resolved.RenderMode);
    Operation.Resolved.FixedAspectRatio = Resolved.FixedAspectRatio;
    Operation.Resolved.Brush = Resolved.Brush;
    OutTransaction.AddImageHostOperation(MoveTemp(Operation));
}

GV2PresentationApply::EPreparedImageRenderMode FGV2ImagePresentation::ToPreparedRenderMode(EGV2ImageRenderMode RenderMode)
{
    switch (RenderMode)
    {
    case EGV2ImageRenderMode::NineSlice:
        return GV2PresentationApply::EPreparedImageRenderMode::NineSlice;
    case EGV2ImageRenderMode::Tile:
        return GV2PresentationApply::EPreparedImageRenderMode::Tile;
    case EGV2ImageRenderMode::FixedAspect:
        return GV2PresentationApply::EPreparedImageRenderMode::FixedAspect;
    }
    return GV2PresentationApply::EPreparedImageRenderMode::FixedAspect;
}
