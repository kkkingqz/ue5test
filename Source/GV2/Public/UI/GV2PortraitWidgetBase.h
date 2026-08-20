#pragma once

#include "CommonUserWidget.h"
#include "UI/GV2DynamicScreenElement.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2UiStyleConsumer.h"
#include "GV2PortraitWidgetBase.generated.h"

class UImage;

/**
 * UGV2PortraitWidgetBase (UIF-14, ADR-0035)
 * Portrait widget displaying a character/actor illustration (fixed_aspect) with an optional frame.
 * Implements IGV2DynamicScreenElement for "core:schema.ui_field.portrait.v1".
 */
UCLASS(Blueprintable)
class GV2_API UGV2PortraitWidgetBase
    : public UCommonUserWidget
    , public IGV2DynamicScreenElement
    , public IGV2UiStyleConsumer
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

    // IGV2DynamicScreenElement
    virtual FGV2ScreenFieldDescriptor GetScreenFieldDescriptor_Implementation() const override;
    virtual bool CanApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value) const override;
    virtual bool CaptureScreenField_Implementation(FGV2ScreenFieldValue& OutFieldValue) const override;
    virtual bool ApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value) override;
    virtual bool ResetScreenField_Implementation() override;

    // IGV2UiStyleConsumer
    virtual bool ApplyCentralStyle_Implementation() override;

protected:
    virtual void NativePreConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UImage> PortraitImage;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UImage> FrameImage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Portrait")
    FName FieldId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Portrait")
    FString SchemaId = TEXT("core:schema.ui_field.portrait.v1");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Portrait")
    bool bIsRequired = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Portrait")
    float PortraitAspectRatio = 1.0f;

private:
    FString AppliedPortraitId;
    FString AppliedFrameId;
};
