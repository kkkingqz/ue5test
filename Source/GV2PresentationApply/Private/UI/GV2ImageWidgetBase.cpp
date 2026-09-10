#include "UI/GV2ImageWidgetBase.h"

#include "Components/Image.h"
#include "UI/GV2UiCapability.h"

namespace
{
bool IsPreparedScalePolicyCompatible(
    EGV2PrimitiveScalePolicy Policy,
    GV2PresentationApply::EPreparedImageRenderMode RenderMode)
{
    switch (Policy)
    {
    case EGV2PrimitiveScalePolicy::FreeStretch:
    case EGV2PrimitiveScalePolicy::Tile:
        return RenderMode == GV2PresentationApply::EPreparedImageRenderMode::Tile;
    case EGV2PrimitiveScalePolicy::NineSlice:
        return RenderMode == GV2PresentationApply::EPreparedImageRenderMode::NineSlice;
    case EGV2PrimitiveScalePolicy::PreserveAspect:
        return RenderMode == GV2PresentationApply::EPreparedImageRenderMode::FixedAspect;
    case EGV2PrimitiveScalePolicy::Unset:
    default:
        return false;
    }
}
}

void UGV2ImageWidgetBase::PostLoad()
{
    Super::PostLoad();
}

void UGV2ImageWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    // PSC-10B: runtime tint arrives as FPreparedTintStyle; design-time preview re-applies the
    // serialized tint this widget already carries. See UGV2SeparatorWidgetBase for the rule.
    if (IsDesignTime() && Image != nullptr)
    {
        ApplyImageTintStyleValue(Image->GetColorAndOpacity());
    }
    // PSC-10C: InitialResourceId is NOT applied here. It is a content reference, and a
    // lifecycle callback -- which runs on a CDO, in the asset editor, and long before any
    // session exists -- is not a place that may resolve content. GV2CentralStylePreparer
    // resolves it against the session snapshot during Prepare and ships it as an ordinary
    // image-host operation; at design time the widget's own serialized brush is what shows.
}

bool UGV2ImageWidgetBase::ApplyResolvedImageResource(
    const GV2PresentationApply::FPreparedResolvedImageValue& Resolved,
    FString& OutError)
{
    if (Image == nullptr)
    {
        OutError = TEXT("Image widget is unavailable.");
        return false;
    }
    if (!IsPreparedScalePolicyCompatible(ScalePolicy, Resolved.RenderMode))
    {
        OutError = TEXT("Image resource render mode is incompatible with the primitive scaling policy.");
        return false;
    }
    if (ScalePolicy == EGV2PrimitiveScalePolicy::PreserveAspect && FixedAspectRatio > 0.0f
        && !FMath::IsNearlyEqual(Resolved.FixedAspectRatio, FixedAspectRatio, 0.001f))
    {
        OutError = TEXT("fixed_aspect resource ratio does not match the target block ratio.");
        return false;
    }
    FSlateBrush Brush = Resolved.Brush;
    if (ScalePolicy == EGV2PrimitiveScalePolicy::Tile)
    {
        Brush.Tiling = ESlateBrushTileType::Both;
        Brush.DrawAs = ESlateBrushDrawType::Image;
    }
    else if (ScalePolicy == EGV2PrimitiveScalePolicy::NineSlice)
    {
        Brush.DrawAs = ESlateBrushDrawType::Box;
    }
    else
    {
        Brush.Tiling = ESlateBrushTileType::NoTile;
        Brush.DrawAs = ESlateBrushDrawType::Image;
    }
    Image->SetBrush(Brush);
    Image->SetDesiredSizeOverride(Brush.ImageSize);
    AppliedResourceId = Resolved.ResourceId;
    ResolvedAspectRatio = Resolved.FixedAspectRatio;
    OutError.Reset();
    return true;
}


FSlateBrush UGV2ImageWidgetBase::GetImageBrush() const
{
    return Image != nullptr ? Image->GetBrush() : FSlateBrush();
}

FString UGV2ImageWidgetBase::GetAppliedResourceId() const
{
    return AppliedResourceId;
}

float UGV2ImageWidgetBase::GetResolvedAspectRatio() const
{
    return ResolvedAspectRatio;
}

void UGV2ImageWidgetBase::ApplyImageTintStyleValue(const FLinearColor& Tint)
{
    if (Image != nullptr)
    {
        Image->SetColorAndOpacity(Tint);
    }
}

void UGV2ImageWidgetBase::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    OutBuilder.AddImage(TEXT("resource_id"), FName(TEXT("Image")), TEXT("resource"));
    OutBuilder.AddKey(TEXT("key"), NAME_None);
}

bool UGV2ImageWidgetBase::ApplyPreparedImageHost(
    const GV2PresentationApply::FPreparedResolvedImageValue& Resolved,
    FString& OutError)
{
    return ApplyResolvedImageResource(Resolved, OutError);
}

void UGV2ImageWidgetBase::ResetPreparedImageHost()
{
    // Reaches past the resolved-apply path straight into the inner UImage, exactly as Reset
    // always did: a reset clears the physical brush without the bookkeeping a resolved
    // commit performs.
    if (Image != nullptr)
    {
        Image->SetBrush(FSlateBrush());
    }
}
