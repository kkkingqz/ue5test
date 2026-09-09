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
