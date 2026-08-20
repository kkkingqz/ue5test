#pragma once

#include "CommonUserWidget.h"
#include "UI/GV2KeyedCollection.h"
#include "UI/GV2UiStyleConsumer.h"
#include "GV2ListViewWidgetBase.generated.h"

class UPanelWidget;

/**
 * UGV2ListViewWidgetBase (UIF-13, ADR-0035)
 * Generalized list container supporting vertical or horizontal layout.
 * Reconciles child widgets by unique non-empty keys using FGV2KeyedCollection.
 */
UCLASS(Abstract, Blueprintable)
class GV2_API UGV2ListViewWidgetBase
    : public UCommonUserWidget
    , public IGV2UiStyleConsumer
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "GV2|UI|ListView")
    EOrientation GetOrientation() const { return Orientation; }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|ListView")
    void SetOrientation(EOrientation InOrientation);

    UFUNCTION(BlueprintPure, Category = "GV2|UI|ListView")
    int32 GetEntryCount() const { return ActiveWidgetsByKey.Num(); }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|ListView")
    UCommonUserWidget* GetEntry(FName Key) const { return ActiveWidgetsByKey.FindRef(Key); }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|ListView")
    void ClearEntries();

    virtual bool ApplyCentralStyle_Implementation() override;

protected:
    virtual void NativePreConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> ContainerPanel;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|ListView")
    TEnumAsByte<EOrientation> Orientation = Orient_Vertical;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UCommonUserWidget>> ActiveWidgetsByKey;
};
