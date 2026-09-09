#pragma once

#include "GV2PresentationApply/PreparedApplyTargets.h"
#include "CommonUserWidget.h"
#include "UI/GV2PropertyConsumers.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2UiStyleConsumer.h"
#include "UI/GV2ScreenFieldHost.h"
#include "UI/GV2TextPipelineHost.h"
#include "GV2ProgressBarWidgetBase.generated.h"

class UProgressBar;
class UCommonTextBlock;

UCLASS(Blueprintable)
class GV2_API UGV2ProgressBarWidgetBase
    : public UCommonUserWidget
    , public IGV2UiStyleConsumer
    , public IGV2UiPropertyHost
    , public IGV2ScreenFieldHost
    , public IGV2TextPipelineHost
    , public IGV2PreparedNumberTarget
    , public IGV2PreparedProgressBarStyleTarget
{
    GENERATED_BODY()

public:
    // PSC-11: value sink for this class's central-style role. It only forwards finished
    // values into the physical write that already existed; no decision happens here.
    virtual void ApplyPreparedProgressBarStyle(const GV2PresentationApply::FPreparedProgressBarStyle& Style) override
    {
        ApplyProgressBarStyleValues(Style.WidgetStyle, Style.FillColor);
    }

    // PSC-11: value sink for the prepared number operation.
    virtual void ApplyPreparedNumber(double Value) override;

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    void ApplyProgress(float Percent);

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    float GetProgress() const;

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|ProgressBar")
    void SetKey(FName InKey) { GetPropertyHostState().SetKey(InKey); }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|ProgressBar")
    FName GetKey() const { return GetPropertyHostState().GetKey(); }

    // IGV2UiPropertyHost
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

    // IGV2ScreenFieldHost (DUC-02): same shared HostIdentity every IGV2UiPropertyHost
    // carries -- see FGV2UiPropertyHostState.
    virtual FName GetScreenFieldId() const override { return GetHostIdentity(); }

    // PSC-10B: sole physical central-style write for this class -- see
    // UGV2SeparatorWidgetBase::ApplySeparatorStyleValues for why this shape.
    void ApplyProgressBarStyleValues(const FProgressBarStyle& WidgetStyle, const FLinearColor& FillColor);

protected:
    virtual void NativePreConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UProgressBar> ProgressBar;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UCommonTextBlock> LabelText;

private:
    float CurrentPercent = 0.0f;

    UPROPERTY(EditAnywhere, Category = "GV2|UI|Identity", meta = (ShowOnlyInnerProperties))
    FGV2UiPropertyHostState PropertyHostState;
};
