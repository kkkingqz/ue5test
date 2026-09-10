#include "UI/GV2PortraitWidgetBase.h"

#include "Components/Image.h"

void UGV2PortraitWidgetBase::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    if (PortraitImage != nullptr)
    {
        OutBuilder.AddImage(TEXT("resource_id"), FName(TEXT("PortraitImage")), TEXT("resource"));
    }
    if (FrameImage != nullptr)
    {
        OutBuilder.AddImage(TEXT("frame_resource_id"), FName(TEXT("FrameImage")), TEXT("resource"));
    }
    OutBuilder.AddKey(TEXT("key"), NAME_None);
}


bool UGV2PortraitWidgetBase::ApplyResolvedPortrait(
    const GV2PresentationApply::FPreparedResolvedImageValue& Resolved,
    FString& OutError)
{
    if (PortraitImage == nullptr)
    {
        OutError = TEXT("Portrait resource supplied but PortraitImage renderer is not bound");
        return false;
    }
    if (Resolved.RenderMode != GV2PresentationApply::EPreparedImageRenderMode::FixedAspect)
    {
        OutError = TEXT("Portrait requires a fixed_aspect image resource.");
        return false;
    }
    if (!FMath::IsFinite(PortraitAspectRatio) || PortraitAspectRatio <= 0.0f
        || !FMath::IsNearlyEqual(Resolved.FixedAspectRatio, PortraitAspectRatio, 0.001f))
    {
        OutError = TEXT("fixed_aspect resource ratio does not match the portrait ratio.");
        return false;
    }
    PortraitImage->SetBrush(Resolved.Brush);
    PortraitImage->SetDesiredSizeOverride(Resolved.Brush.ImageSize);
    AppliedPortraitId = Resolved.ResourceId;
    SetVisibility(ESlateVisibility::Visible);
    OutError.Reset();
    return true;
}

bool UGV2PortraitWidgetBase::ApplyPreparedImageHost(
    const GV2PresentationApply::FPreparedResolvedImageValue& Resolved,
    FString& OutError)
{
    return ApplyResolvedPortrait(Resolved, OutError);
}

void UGV2PortraitWidgetBase::ResetPreparedImageHost()
{
    // Reaches past the resolved-apply path straight into the inner UImage, exactly as Reset
    // always did: a reset clears the physical brush without the bookkeeping a resolved
    // commit performs.
    SetVisibility(ESlateVisibility::Collapsed);
    if (PortraitImage != nullptr)
    {
        PortraitImage->SetBrush(FSlateBrush());
    }
}
