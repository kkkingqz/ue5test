#pragma once

#include "GV2PresentationApply/PreparedApplyTargets.h"
#include "CommonUserWidget.h"
#include "Styling/SlateBrush.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2UiStyleConsumer.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2ScreenFieldHost.h"
#include "GV2ImageWidgetBase.generated.h"

class UImage;

UCLASS(Blueprintable)
class GV2_API UGV2ImageWidgetBase
    : public UCommonUserWidget
    , public IGV2UiStyleConsumer
    , public IGV2UiPropertyHost
    , public IGV2ScreenFieldHost
    , public IGV2PreparedKeyTarget
    , public IGV2PreparedImageHostTarget
    , public IGV2PreparedTintStyleTarget
{
    GENERATED_BODY()

public:
    // PSC-11: value sink for this class's central-style role. It only forwards finished
    // values into the physical write that already existed; no decision happens here.
    virtual void ApplyPreparedTintStyle(const GV2PresentationApply::FPreparedTintStyle& Style) override
    {
        ApplyImageTintStyleValue(Style.Tint);
    }

    // PSC-11: value sinks for the prepared image-host operation.
    virtual bool ApplyPreparedImageHost(
        const GV2PresentationApply::FPreparedResolvedImageValue& Resolved,
        FString& OutError) override;
    virtual void ResetPreparedImageHost() override;

    // PSC-11: value sink for the prepared `key` operation. The generic identity write is the
    // same one every property host already performs; a host that routes a NAMED key
    // capability overrides this and falls back to it.
    virtual bool ApplyPreparedKey(FName PropertyName, FName Value) override
    {
        GetPropertyHostState().SetKey(Value);
        return true;
    }


    // STATUS-012: application-phase entry point. Takes what preparation already
    // resolved instead of re-deriving it from the id.
    bool ApplyResolvedImageResource(const FGV2ResolvedImageResource& Resolved, FString& OutError);

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    UImage* GetImageWidget() const { return Image; }

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    FSlateBrush GetImageBrush() const;

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    FString GetAppliedResourceId() const;

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    float GetResolvedAspectRatio() const;

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    float GetFixedAspectRatio() const
    {
        return FixedAspectRatio;
    }

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    // PSC-10C: read by the subtree preparer, which resolves this authoring-time content
    // reference against the session snapshot. The widget itself never resolves it.
    const FString& GetInitialResourceId() const { return InitialResourceId; }

    EGV2PrimitiveScalePolicy GetScalePolicy() const
    {
        return ScalePolicy;
    }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    void SetScalePolicy(EGV2PrimitiveScalePolicy InScalePolicy)
    {
        ScalePolicy = InScalePolicy;
    }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Image")
    void SetKey(FName InKey) { GetPropertyHostState().SetKey(InKey); }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Image")
    FName GetKey() const { return GetPropertyHostState().GetKey(); }

    // PSC-10B: sole physical central-style write for this class -- see
    // UGV2SeparatorWidgetBase::ApplySeparatorStyleValues for why this shape.
    void ApplyImageTintStyleValue(const FLinearColor& Tint);

    // IGV2UiPropertyHost
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

    // IGV2ScreenFieldHost (DUC-02): same shared HostIdentity every IGV2UiPropertyHost
    // carries -- see FGV2UiPropertyHostState.
    virtual FName GetScreenFieldId() const override { return GetHostIdentity(); }

protected:
    virtual void PostLoad() override;
    virtual void NativePreConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UImage> Image;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Image Contract")
    EGV2PrimitiveScalePolicy ScalePolicy = EGV2PrimitiveScalePolicy::Unset;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Image Contract", meta = (ClampMin = "0.01", EditCondition = "ScalePolicy == EGV2PrimitiveScalePolicy::PreserveAspect", EditConditionHides))
    float FixedAspectRatio = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Image Contract")
    FString InitialResourceId;

private:
    FString AppliedResourceId;
    float ResolvedAspectRatio = 0.0f;

    UPROPERTY(EditAnywhere, Category = "GV2|UI|Identity", meta = (ShowOnlyInnerProperties))
    FGV2UiPropertyHostState PropertyHostState;
};
