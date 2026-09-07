#include "UI/GV2ImagePresentation.h"

#include "Components/Image.h"
#include "Logging/LogMacros.h"

// PAH-08: phase=commit_resolve_deferred=STATUS-012
// Reached from FGV2ImageResourcePropertyConsumer::Commit (directly for a bare
// UImage, and through UGV2ImageWidgetBase::ApplyImageResource /
// UGV2PortraitWidgetBase::ApplyPortrait for the hosts), so the image resource
// authority is consulted during application. Prepare already resolved the same
// id and validated render mode and aspect ratio against it, then kept only the
// id -- so the value applied is re-derived, not the value that was approved.
// Benign today (the catalog is session-pinned and cannot change between the two
// phases), a real INV-P5 violation structurally. STATUS-012 carries it.
bool FGV2ImagePresentation::ResolveAndApply(
    UImage* Widget,
    const FString& ResourceId,
    const EGV2PrimitiveScalePolicy ScalePolicy,
    const TOptional<float> FixedAspectRatio,
    FGV2ResolvedImageResource& OutResource,
    FString& OutError)
{
    UGV2ImageResourceCatalog* Catalog = UGV2ImageResourceCatalog::GetSessionCatalog();
    if (Widget == nullptr || Catalog == nullptr)
    {
        OutError = Widget == nullptr
            ? TEXT("Image widget is unavailable.")
            : TEXT("Configured Image Resource Catalog is unavailable.");
        return false;
    }
    FGV2ResolvedImageResource Candidate;
    if (!Catalog->Resolve(ResourceId, Candidate, OutError)) return false;
    if (!IsScalePolicyCompatible(ScalePolicy, Candidate.RenderMode))
    {
        OutError = TEXT("Image resource render mode is incompatible with the primitive scaling policy.");
        return false;
    }
    if (ScalePolicy == EGV2PrimitiveScalePolicy::PreserveAspect && FixedAspectRatio.IsSet()
        && (!FMath::IsFinite(FixedAspectRatio.GetValue()) || FixedAspectRatio.GetValue() <= 0.0f
            || !FMath::IsNearlyEqual(Candidate.FixedAspectRatio, FixedAspectRatio.GetValue(), 0.001f)))
    {
        OutError = TEXT("fixed_aspect resource ratio does not match the target block ratio.");
        return false;
    }
    FSlateBrush ResolvedBrush = Candidate.Brush;
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
    OutResource = MoveTemp(Candidate);
    OutResource.Brush = ResolvedBrush;
    OutError.Reset();
    return true;
}
