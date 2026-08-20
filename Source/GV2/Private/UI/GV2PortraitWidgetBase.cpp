#include "UI/GV2PortraitWidgetBase.h"

#include "Components/Image.h"
#include "UI/GV2ImagePresentation.h"
#include "UI/GV2UiTheme.h"

void UGV2PortraitWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyCentralStyle_Implementation();
}

bool UGV2PortraitWidgetBase::ApplyPortrait(
    const FString& ResourceId,
    const FString& FrameResourceId,
    FString& OutError)
{
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

FGV2ScreenFieldDescriptor UGV2PortraitWidgetBase::GetScreenFieldDescriptor_Implementation() const
{
    FGV2ScreenFieldDescriptor Desc;
    Desc.FieldId = FieldId;
    Desc.SchemaId = SchemaId;
    Desc.bRequired = bIsRequired;
    return Desc;
}

bool UGV2PortraitWidgetBase::CanApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value) const
{
    return Value.SchemaId == SchemaId && !Value.PortraitValue.ResourceId.IsEmpty();
}

bool UGV2PortraitWidgetBase::CaptureScreenField_Implementation(FGV2ScreenFieldValue& OutFieldValue) const
{
    FGV2PortraitViewModel Model;
    Model.ResourceId = AppliedPortraitId;
    Model.FrameResourceId = AppliedFrameId;
    OutFieldValue = FGV2ScreenFieldValue::MakePortrait(FieldId, Model);
    return true;
}

bool UGV2PortraitWidgetBase::ApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value)
{
    if (!CanApplyScreenField_Implementation(Value))
    {
        return false;
    }
    FString Error;
    return ApplyPortrait(Value.PortraitValue.ResourceId, Value.PortraitValue.FrameResourceId, Error);
}

bool UGV2PortraitWidgetBase::ResetScreenField_Implementation()
{
    AppliedPortraitId.Reset();
    AppliedFrameId.Reset();
    if (PortraitImage != nullptr)
    {
        PortraitImage->SetBrush(FSlateBrush());
    }
    if (FrameImage != nullptr)
    {
        FrameImage->SetBrush(FSlateBrush());
    }
    return true;
}

bool UGV2PortraitWidgetBase::ApplyCentralStyle_Implementation()
{
    UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    if (Theme == nullptr)
    {
        return false;
    }
    return true;
}
