#pragma once

#include "GV2PresentationApply/PreparedApplyTargets.h"
#include "CommonUserWidget.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2PropertyConsumers.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2ScreenFieldHost.h"
#include "GV2PortraitWidgetBase.generated.h"

class UImage;

/**
 * UGV2PortraitWidgetBase (UIF-14, ADR-0035)
 * Portrait widget displaying a character/actor illustration (fixed_aspect) with an optional frame.
 */
UCLASS(Blueprintable)
class GV2_API UGV2PortraitWidgetBase
    : public UCommonUserWidget
    , public IGV2UiPropertyHost
    , public IGV2ScreenFieldHost
    , public IGV2PreparedKeyTarget
    , public IGV2PreparedImageHostTarget
{
    GENERATED_BODY()

public:
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


    // STATUS-012: application-phase entry point; the portrait resource arrives
    // already resolved. Frame is not part of the prepared value today (no consumer
    // supplies one), so this variant covers the portrait only.
    bool ApplyResolvedPortrait(const FGV2ResolvedImageResource& Resolved, FString& OutError);

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Portrait")
    FString GetPortraitResourceId() const { return AppliedPortraitId; }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Portrait")
    FString GetFrameResourceId() const { return AppliedFrameId; }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Portrait")
    UImage* GetPortraitImage() const { return PortraitImage; }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Portrait")
    float GetPortraitAspectRatio() const { return PortraitAspectRatio; }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Portrait")
    void SetKey(FName InKey) { GetPropertyHostState().SetKey(InKey); }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Portrait")
    FName GetKey() const { return GetPropertyHostState().GetKey(); }

    // IGV2UiPropertyHost
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

    // IGV2ScreenFieldHost (DUC-02): same shared HostIdentity every IGV2UiPropertyHost
    // carries -- see FGV2UiPropertyHostState.
    virtual FName GetScreenFieldId() const override { return GetHostIdentity(); }

protected:
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UImage> PortraitImage;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UImage> FrameImage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Portrait")
    float PortraitAspectRatio = 1.0f;

private:
    FString AppliedPortraitId;
    FString AppliedFrameId;

    UPROPERTY(EditAnywhere, Category = "GV2|UI|Identity", meta = (ShowOnlyInnerProperties))
    FGV2UiPropertyHostState PropertyHostState;
};
