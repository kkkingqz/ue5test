#pragma once

#include "CommonUserWidget.h"
#include "Styling/SlateBrush.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2UiStyleConsumer.h"
#include "GV2PanelWidgetBase.generated.h"

class UNamedSlot;
class UBorder;

/**
 * UGV2PanelWidgetBase (UIF-12, ADR-0035)
 * Base layout panel container providing a background surface, content slot, and padding.
 * Scaling policy for the background surface defaults to NineSlice.
 */
UCLASS(Blueprintable)
class GV2_API UGV2PanelWidgetBase
    : public UCommonUserWidget
    , public IGV2UiStyleConsumer
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Panel")
    void SetContentPadding(FMargin InPadding);

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Panel")
    FMargin GetContentPadding() const;

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Panel")
    void SetBackgroundBrush(const FSlateBrush& InBrush);

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Panel")
    FSlateBrush GetBackgroundBrush() const;

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Panel")
    EGV2PrimitiveScalePolicy GetScalePolicy() const { return ScalePolicy; }

    virtual bool ApplyCentralStyle_Implementation() override;

protected:
    virtual void NativePreConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UBorder> BackgroundBorder;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UNamedSlot> ContentSlot;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Panel")
    EGV2PrimitiveScalePolicy ScalePolicy = EGV2PrimitiveScalePolicy::NineSlice;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Panel")
    FMargin ContentPadding = FMargin(16.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Panel")
    FSlateBrush BackgroundBrush;
};
