#pragma once

#include "CommonUserWidget.h"
#include "Styling/SlateBrush.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2UiStyleConsumer.h"
#include "UI/GV2UiPropertyHost.h"
#include "GV2ImageWidgetBase.generated.h"

class UImage;

UCLASS(Blueprintable)
class GV2_API UGV2ImageWidgetBase
    : public UCommonUserWidget
    , public IGV2UiStyleConsumer
    , public IGV2UiPropertyHost
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    bool ApplyImageResource(const FString& ResourceId, FString& OutError);

    /** Applies ResourceId, using PlaceholderResourceId and emitting a diagnostic when it cannot resolve. */
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    bool ApplyOptionalImageResource(const FString& ResourceId, const FString& PlaceholderResourceId, FString& OutError);

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
    EGV2PrimitiveScalePolicy GetScalePolicy() const
    {
        return ScalePolicy;
    }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    void SetScalePolicy(EGV2PrimitiveScalePolicy InScalePolicy)
    {
        ScalePolicy = InScalePolicy;
    }

    virtual bool ApplyCentralStyle_Implementation() override;

    // IGV2UiPropertyHost
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

protected:
    virtual void PostLoad() override;
    virtual void NativePreConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UImage> Image;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Image Contract")
    EGV2PrimitiveScalePolicy ScalePolicy = EGV2PrimitiveScalePolicy::Unset;

    UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use ScalePolicy instead."))
    EGV2ImageRenderMode AcceptedRenderMode_DEPRECATED = EGV2ImageRenderMode::FixedAspect;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Image Contract", meta = (ClampMin = "0.01", EditCondition = "ScalePolicy == EGV2PrimitiveScalePolicy::PreserveAspect", EditConditionHides))
    float FixedAspectRatio = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Image Contract")
    FString InitialResourceId;

private:
    FString AppliedResourceId;
    float ResolvedAspectRatio = 0.0f;
    FGV2UiPropertyHostState PropertyHostState;
};
