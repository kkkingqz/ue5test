#pragma once

#include "CommonUserWidget.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2PropertyConsumers.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2UiStyleConsumer.h"
#include "GV2PortraitWidgetBase.generated.h"

class UImage;

/**
 * UGV2PortraitWidgetBase (UIF-14, ADR-0035)
 * Portrait widget displaying a character/actor illustration (fixed_aspect) with an optional frame.
 */
UCLASS(Blueprintable)
class GV2_API UGV2PortraitWidgetBase
    : public UCommonUserWidget
    , public IGV2UiStyleConsumer
    , public IGV2UiPropertyHost
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Portrait")
    bool ApplyPortrait(const FString& ResourceId, const FString& FrameResourceId, FString& OutError);

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Portrait")
    bool ApplyOptionalPortrait(const FString& ResourceId, const FString& PlaceholderResourceId, const FString& FrameResourceId, FString& OutError);

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Portrait")
    FString GetPortraitResourceId() const { return AppliedPortraitId; }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Portrait")
    FString GetFrameResourceId() const { return AppliedFrameId; }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Portrait")
    UImage* GetPortraitImage() const { return PortraitImage; }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Portrait")
    float GetPortraitAspectRatio() const { return PortraitAspectRatio; }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Portrait")
    void SetKey(FName InKey) { Key = InKey; }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Portrait")
    FName GetKey() const { return Key; }

    // IGV2UiPropertyHost
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

    // IGV2UiStyleConsumer
    virtual bool ApplyCentralStyle_Implementation() override;

protected:
    virtual void NativePreConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UImage> PortraitImage;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UImage> FrameImage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Portrait")
    float PortraitAspectRatio = 1.0f;

private:
    FString AppliedPortraitId;
    FString AppliedFrameId;
    FName Key;
    FGV2UiPropertyHostState PropertyHostState;
};
