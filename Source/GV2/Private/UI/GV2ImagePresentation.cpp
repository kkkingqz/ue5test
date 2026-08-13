#include "UI/GV2ImagePresentation.h"

#include "Components/Image.h"

bool FGV2ImagePresentation::ResolveAndApply(
    UImage* Widget,
    const FString& ResourceId,
    const EGV2ImageRenderMode AcceptedRenderMode,
    const TOptional<float> FixedAspectRatio,
    FGV2ResolvedImageResource& OutResource,
    FString& OutError)
{
    UGV2ImageResourceCatalog* Catalog = UGV2ImageResourceCatalogSettings::GetConfiguredCatalog();
    if (Widget == nullptr || Catalog == nullptr)
    {
        OutError = Widget == nullptr
            ? TEXT("Image widget is unavailable.")
            : TEXT("Configured Image Resource Catalog is unavailable.");
        return false;
    }
    FGV2ResolvedImageResource Candidate;
    if (!Catalog->Resolve(ResourceId, Candidate, OutError)) return false;
    if (Candidate.RenderMode != AcceptedRenderMode)
    {
        OutError = TEXT("Image resource render mode is incompatible with the target block.");
        return false;
    }
    if (AcceptedRenderMode == EGV2ImageRenderMode::FixedAspect && FixedAspectRatio.IsSet()
        && (!FMath::IsFinite(FixedAspectRatio.GetValue()) || FixedAspectRatio.GetValue() <= 0.0f
            || !FMath::IsNearlyEqual(Candidate.FixedAspectRatio, FixedAspectRatio.GetValue(), 0.001f)))
    {
        OutError = TEXT("fixed_aspect resource ratio does not match the target block ratio.");
        return false;
    }
    Widget->SetBrush(Candidate.Brush);
    Widget->SetDesiredSizeOverride(Candidate.Brush.ImageSize);
    OutResource = MoveTemp(Candidate);
    OutError.Reset();
    return true;
}
