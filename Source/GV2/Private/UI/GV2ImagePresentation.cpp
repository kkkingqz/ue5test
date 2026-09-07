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

// PAH-08: phase=prepare
// Resolve + apply, for callers that legitimately consult the catalog: the
// consumer's Prepare, and widget-local paths outside a presentation transaction.
// STATUS-012 removed the application-phase callers, so this is no longer reachable
// from a Commit root -- validate_presentation_authority_phase.py asserts that.
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
    if (!Catalog->Resolve(ResourceId, Candidate, OutError))
    {
        return false;
    }
    if (!ApplyResolved(Widget, Candidate, ScalePolicy, FixedAspectRatio, OutError))
    {
        return false;
    }
    OutResource = MoveTemp(Candidate);
    OutResource.Brush = Widget->GetBrush();
    OutError.Reset();
    return true;
}
