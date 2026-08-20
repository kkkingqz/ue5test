#include "UI/GV2ImagePresentation.h"

#include "Components/Image.h"
#include "Logging/LogMacros.h"

bool FGV2ImagePresentation::ResolveOptionalAndApply(
    UImage* Widget,
    const FString& ResourceId,
    const FString& PlaceholderResourceId,
    const EGV2PrimitiveScalePolicy ScalePolicy,
    const TOptional<float> FixedAspectRatio,
    FGV2ResolvedImageResource& OutResource,
    FString& OutError)
{
    FString RequestedError;
    if (ResolveAndApply(Widget, ResourceId, ScalePolicy, FixedAspectRatio, OutResource, RequestedError))
    {
        OutError.Reset();
        return true;
    }
    UE_LOG(LogTemp, Warning, TEXT("GV2 optional image '%s' unavailable: %s; using placeholder '%s'."), *ResourceId, *RequestedError, *PlaceholderResourceId);
    if (PlaceholderResourceId.IsEmpty()
        || !ResolveAndApply(Widget, PlaceholderResourceId, ScalePolicy, FixedAspectRatio, OutResource, OutError))
    {
        OutError = FString::Printf(TEXT("Optional resource '%s' failed (%s); placeholder '%s' failed (%s)."), *ResourceId, *RequestedError, *PlaceholderResourceId, *OutError);
        return false;
    }
    return true;
}

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

bool FGV2ImagePresentation::ResolveAndApply(
    UImage* Widget,
    const FString& ResourceId,
    const EGV2PrimitiveScalePolicy ScalePolicy,
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
    Widget->SetBrush(Candidate.Brush);
    Widget->SetDesiredSizeOverride(Candidate.Brush.ImageSize);
    OutResource = MoveTemp(Candidate);
    OutError.Reset();
    return true;
}
