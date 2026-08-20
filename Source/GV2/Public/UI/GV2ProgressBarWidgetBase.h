#pragma once

#include "CommonUserWidget.h"
#include "UI/GV2DynamicScreenElement.h"
#include "UI/GV2UiStyleConsumer.h"
#include "GV2ProgressBarWidgetBase.generated.h"

class UProgressBar;
class UCommonTextBlock;

UCLASS(Blueprintable)
class GV2_API UGV2ProgressBarWidgetBase
    : public UCommonUserWidget
    , public IGV2DynamicScreenElement
    , public IGV2UiStyleConsumer
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    void ApplyProgress(float Percent);

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    float GetProgress() const;

    // IGV2DynamicScreenElement
    virtual FGV2ScreenFieldDescriptor GetScreenFieldDescriptor_Implementation() const override;
    virtual bool CanApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value) const override;
    virtual bool CaptureScreenField_Implementation(FGV2ScreenFieldValue& OutFieldValue) const override;
    virtual bool ApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value) override;
    virtual bool ResetScreenField_Implementation() override;

    virtual bool ApplyCentralStyle_Implementation() override;

protected:
    virtual void NativePreConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UProgressBar> ProgressBar;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UCommonTextBlock> LabelText;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|ProgressBar")
    FName FieldId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|ProgressBar")
    FString SchemaId = TEXT("core:schema.ui_field.progress_bar.v1");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|ProgressBar")
    bool bIsRequired = false;

private:
    float CurrentPercent = 0.0f;
    FGV2TextViewModel CurrentLabel;
};
