#pragma once

#include "CommonUserWidget.h"
#include "UI/GV2PropertyConsumers.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2UiStyleConsumer.h"
#include "GV2ProgressBarWidgetBase.generated.h"

class UProgressBar;
class UCommonTextBlock;

UCLASS(Blueprintable)
class GV2_API UGV2ProgressBarWidgetBase
    : public UCommonUserWidget
    , public IGV2UiStyleConsumer
    , public IGV2UiPropertyHost
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    void ApplyProgress(float Percent);

    /** Applies percent and label together. Label goes through UGV2TextPipeline, so a
     *  missing or invalid style token fails instead of silently rendering unstyled text. */
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    bool ApplyProgressBarModel(const FGV2ProgressBarViewModel& Model);

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    float GetProgress() const;

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|ProgressBar")
    void SetKey(FName InKey) { Key = InKey; }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|ProgressBar")
    FName GetKey() const { return Key; }

    // IGV2UiPropertyHost
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

    virtual bool ApplyCentralStyle_Implementation() override;

protected:
    virtual void NativePreConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UProgressBar> ProgressBar;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UCommonTextBlock> LabelText;

private:
    float CurrentPercent = 0.0f;
    FGV2TextViewModel CurrentLabel;
    FName Key;
    FGV2UiPropertyHostState PropertyHostState;
};
