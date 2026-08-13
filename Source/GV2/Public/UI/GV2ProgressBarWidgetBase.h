#pragma once

#include "CommonUserWidget.h"
#include "UI/GV2UiStyleConsumer.h"
#include "GV2ProgressBarWidgetBase.generated.h"

class UProgressBar;

UCLASS(Abstract, Blueprintable)
class GV2_API UGV2ProgressBarWidgetBase
    : public UCommonUserWidget
    , public IGV2UiStyleConsumer
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    void ApplyProgress(float Percent);

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    float GetProgress() const;

    virtual bool ApplyCentralStyle_Implementation() override;

protected:
    virtual void NativePreConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UProgressBar> ProgressBar;
};
