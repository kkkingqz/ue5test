#pragma once

#include "CommonUserWidget.h"
#include "Styling/SlateBrush.h"
#include "UI/GV2DynamicScreenElement.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2UiStyleConsumer.h"
#include "GV2ImageWidgetBase.generated.h"

class UImage;

UCLASS(Blueprintable)
class GV2_API UGV2ImageWidgetBase
    : public UCommonUserWidget
    , public IGV2DynamicScreenElement
    , public IGV2UiStyleConsumer
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    bool ApplyImageResource(const FString& ResourceId, FString& OutError);

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    FSlateBrush GetImageBrush() const;

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    FString GetAppliedResourceId() const;

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    float GetResolvedAspectRatio() const;

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    EGV2PrimitiveScalePolicy GetScalePolicy() const
    {
        return ScalePolicy;
    }

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
    TObjectPtr<UImage> Image;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Image Contract")
    FName FieldId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Image Contract")
    FString SchemaId = TEXT("core:schema.ui_field.image.v1");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Image Contract")
    bool bIsRequired = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Image Contract")
    EGV2PrimitiveScalePolicy ScalePolicy = EGV2PrimitiveScalePolicy::PreserveAspect;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Image Contract")
    EGV2ImageRenderMode AcceptedRenderMode = EGV2ImageRenderMode::FixedAspect;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Image Contract", meta = (ClampMin = "0.01", EditCondition = "ScalePolicy == EGV2PrimitiveScalePolicy::PreserveAspect", EditConditionHides))
    float FixedAspectRatio = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Image Contract")
    FString InitialResourceId;

private:
    FString AppliedResourceId;
    float ResolvedAspectRatio = 0.0f;
};
