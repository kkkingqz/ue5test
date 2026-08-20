#pragma once

#include "CommonUserWidget.h"
#include "Components/ScrollBox.h"
#include "UI/GV2UiStyleConsumer.h"
#include "GV2ScrollAreaWidgetBase.generated.h"

class UNamedSlot;

/**
 * UGV2ScrollAreaWidgetBase (UIF-12, ADR-0035)
 * Scrollable layout area container wrapping a ScrollBox.
 * Scroll state is strictly UI-local and does not persist across session/save boundaries.
 * Enables responsive layouts on smaller viewports (e.g. 720p) without clipping.
 */
UCLASS(Abstract, Blueprintable)
class GV2_API UGV2ScrollAreaWidgetBase
    : public UCommonUserWidget
    , public IGV2UiStyleConsumer
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "GV2|UI|ScrollArea")
    void ScrollToStart();

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|ScrollArea")
    void ScrollToEnd();

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|ScrollArea")
    void SetScrollOffset(float NewOffset);

    UFUNCTION(BlueprintPure, Category = "GV2|UI|ScrollArea")
    float GetScrollOffset() const;

    UFUNCTION(BlueprintPure, Category = "GV2|UI|ScrollArea")
    EOrientation GetOrientation() const { return Orientation; }

    virtual bool ApplyCentralStyle_Implementation() override;

protected:
    virtual void NativePreConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UScrollBox> ScrollBox;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UNamedSlot> ContentSlot;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|ScrollArea")
    TEnumAsByte<EOrientation> Orientation = Orient_Vertical;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|ScrollArea")
    bool bAlwaysShowScrollbar = false;
};
