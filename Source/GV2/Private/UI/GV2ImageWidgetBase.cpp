#include "UI/GV2ImageWidgetBase.h"

#include "Components/Image.h"
#include "UI/GV2ImagePresentation.h"
#include "UI/GV2UiTheme.h"

void UGV2ImageWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyCentralStyle_Implementation();
    if (!InitialResourceId.IsEmpty() && InitialResourceId != AppliedResourceId)
    {
        FString Error;
        if (!ApplyImageResource(InitialResourceId, Error))
        {
            UE_LOG(LogTemp, Warning, TEXT("Cannot apply initial image resource '%s': %s"), *InitialResourceId, *Error);
        }
    }
}

bool UGV2ImageWidgetBase::ApplyImageResource(const FString& ResourceId, FString& OutError)
{
    FGV2ResolvedImageResource Resource;
    const TOptional<float> RequiredAspect = AcceptedRenderMode == EGV2ImageRenderMode::FixedAspect
        ? TOptional<float>(FixedAspectRatio)
        : TOptional<float>();
    if (!FGV2ImagePresentation::ResolveAndApply(
        Image, ResourceId, AcceptedRenderMode, RequiredAspect, Resource, OutError))
    {
        return false;
    }
    AppliedResourceId = Resource.ResourceId;
    ResolvedAspectRatio = Resource.FixedAspectRatio;
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

bool UGV2ImageWidgetBase::ApplyCentralStyle_Implementation()
{
    UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    if (Theme == nullptr || Image == nullptr)
    {
        return false;
    }
    Image->SetColorAndOpacity(Theme->ImageTint);
    return true;
}
