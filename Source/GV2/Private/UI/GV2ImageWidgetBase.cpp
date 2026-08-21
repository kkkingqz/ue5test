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
            UGV2ImageResourceCatalog* Catalog = UGV2ImageResourceCatalogSettings::GetConfiguredCatalog();
            FGV2ResolvedImageResource Candidate;
            FString TempErr;
            if (Catalog != nullptr && Catalog->Resolve(InitialResourceId, Candidate, TempErr))
            {
                if (Candidate.RenderMode == EGV2ImageRenderMode::Tile)
                {
                    ScalePolicy = EGV2PrimitiveScalePolicy::Tile;
                }
                else if (Candidate.RenderMode == EGV2ImageRenderMode::NineSlice)
                {
                    ScalePolicy = EGV2PrimitiveScalePolicy::NineSlice;
                }
                ApplyImageResource(InitialResourceId, Error);
            }
            if (AppliedResourceId.IsEmpty())
            {
                UE_LOG(LogTemp, Warning, TEXT("Cannot apply initial image resource '%s': %s"), *InitialResourceId, *Error);
            }
        }
    }
}

bool UGV2ImageWidgetBase::ApplyImageResource(const FString& ResourceId, FString& OutError)
{
    FGV2ResolvedImageResource Resource;
    const TOptional<float> RequiredAspect = (ScalePolicy == EGV2PrimitiveScalePolicy::PreserveAspect && FixedAspectRatio > 0.0f)
        ? TOptional<float>(FixedAspectRatio)
        : TOptional<float>();
    if (!FGV2ImagePresentation::ResolveAndApply(
        Image, ResourceId, ScalePolicy, RequiredAspect, Resource, OutError))
    {
        return false;
    }
    AppliedResourceId = Resource.ResourceId;
    ResolvedAspectRatio = Resource.FixedAspectRatio;
    return true;
}

bool UGV2ImageWidgetBase::ApplyOptionalImageResource(
    const FString& ResourceId,
    const FString& PlaceholderResourceId,
    FString& OutError)
{
    FGV2ResolvedImageResource Resource;
    const TOptional<float> RequiredAspect = (ScalePolicy == EGV2PrimitiveScalePolicy::PreserveAspect && FixedAspectRatio > 0.0f)
        ? TOptional<float>(FixedAspectRatio)
        : TOptional<float>();
    if (!FGV2ImagePresentation::ResolveOptionalAndApply(
        Image, ResourceId, PlaceholderResourceId, ScalePolicy, RequiredAspect, Resource, OutError))
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

FGV2ScreenFieldDescriptor UGV2ImageWidgetBase::GetScreenFieldDescriptor_Implementation() const
{
    FGV2ScreenFieldDescriptor Desc;
    Desc.FieldId = FieldId;
    Desc.SchemaId = SchemaId;
    Desc.bRequired = bIsRequired;
    return Desc;
}

bool UGV2ImageWidgetBase::CanApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value) const
{
    return Value.SchemaId == SchemaId && !Value.ImageValue.ResourceId.IsEmpty();
}

bool UGV2ImageWidgetBase::CaptureScreenField_Implementation(FGV2ScreenFieldValue& OutFieldValue) const
{
    FGV2ImageFieldViewModel Model;
    Model.ResourceId = AppliedResourceId;
    OutFieldValue = FGV2ScreenFieldValue::MakeImage(FieldId, Model);
    return true;
}

bool UGV2ImageWidgetBase::ApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value)
{
    if (!CanApplyScreenField_Implementation(Value))
    {
        return false;
    }
    FString Error;
    return ApplyImageResource(Value.ImageValue.ResourceId, Error);
}

bool UGV2ImageWidgetBase::ResetScreenField_Implementation()
{
    AppliedResourceId.Reset();
    ResolvedAspectRatio = 0.0f;
    if (Image != nullptr)
    {
        Image->SetBrush(FSlateBrush());
    }
    return true;
}
