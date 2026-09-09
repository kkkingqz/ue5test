#include "UI/GV2ImageWidgetBase.h"

#include "Components/Image.h"
#include "UI/GV2ImagePresentation.h"
#include "UI/GV2UiCapability.h"

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

bool UGV2ImageWidgetBase::ApplyResolvedImageResource(const FGV2ResolvedImageResource& Resolved, FString& OutError)
{
    const TOptional<float> RequiredAspect = (ScalePolicy == EGV2PrimitiveScalePolicy::PreserveAspect && FixedAspectRatio > 0.0f)
        ? TOptional<float>(FixedAspectRatio)
        : TOptional<float>();
    if (!FGV2ImagePresentation::ApplyResolved(Image, Resolved, ScalePolicy, RequiredAspect, OutError))
    {
        return false;
    }
    AppliedResourceId = Resolved.ResourceId;
    ResolvedAspectRatio = Resolved.FixedAspectRatio;
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
