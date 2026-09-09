#include "UI/GV2PortraitWidgetBase.h"

#include "Components/Image.h"
#include "UI/GV2ImagePresentation.h"
#include "UI/GV2UiTheme.h"

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

bool UGV2PortraitWidgetBase::ApplyPortrait(
    const FString& ResourceId,
    const FString& FrameResourceId,
    FString& OutError)
{
    if (!ResourceId.IsEmpty() && PortraitImage == nullptr)
    {
        OutError = TEXT("Portrait resource supplied but PortraitImage renderer is not bound");
        return false;
    }
    if (!FrameResourceId.IsEmpty() && FrameImage == nullptr)
    {
        OutError = TEXT("Portrait frame resource supplied but FrameImage renderer is not bound");
        return false;
    }

    if (PortraitImage != nullptr && !ResourceId.IsEmpty())
    {
        FGV2ResolvedImageResource Res;
        if (!FGV2ImagePresentation::ResolveAndApply(
            PortraitImage,
            ResourceId,
            EGV2PrimitiveScalePolicy::PreserveAspect,
            TOptional<float>(PortraitAspectRatio),
            Res,
            OutError))
        {
            return false;
        }
        AppliedPortraitId = ResourceId;
    }

    if (FrameImage != nullptr && !FrameResourceId.IsEmpty())
    {
        FGV2ResolvedImageResource FrameRes;
        if (!FGV2ImagePresentation::ResolveAndApply(
            FrameImage,
            FrameResourceId,
            EGV2PrimitiveScalePolicy::NineSlice,
            TOptional<float>(),
            FrameRes,
            OutError))
        {
            return false;
        }
        AppliedFrameId = FrameResourceId;
    }

    return true;
}

bool UGV2PortraitWidgetBase::ApplyResolvedPortrait(const FGV2ResolvedImageResource& Resolved, FString& OutError)
{
    if (PortraitImage == nullptr)
    {
        OutError = TEXT("Portrait resource supplied but PortraitImage renderer is not bound");
        return false;
    }
    if (!FGV2ImagePresentation::ApplyResolved(
            PortraitImage,
            Resolved,
            EGV2PrimitiveScalePolicy::PreserveAspect,
            TOptional<float>(PortraitAspectRatio),
            OutError))
    {
        return false;
    }
    AppliedPortraitId = Resolved.ResourceId;
    SetVisibility(ESlateVisibility::Visible);
    return true;
}
