#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "CommonUserWidget.h"
#include "GV2ButtonListWidgetBase.generated.h"

class UGV2ButtonWidgetBase;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FGV2ButtonListBindingInvoked,
    FGV2UiBindingHandle, BindingHandle,
    EGV2SubmitUiInteractionResult, Result);

UCLASS(Abstract, Blueprintable)
class GV2_API UGV2ButtonListWidgetBase : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    bool ApplyButtonModels(const TArray<FGV2ButtonViewModel>& ButtonModels);

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    bool CanApplyButtonModels(const TArray<FGV2ButtonViewModel>& ButtonModels) const;

    UPROPERTY(BlueprintAssignable, Category = "GV2|UI")
    FGV2ButtonListBindingInvoked OnBindingInvoked;

protected:
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UVerticalBox> ButtonContainer;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GV2|UI")
    TSubclassOf<UGV2ButtonWidgetBase> ButtonWidgetClass;

private:
    UFUNCTION()
    void HandleButtonBindingInvoked(
        FGV2UiBindingHandle BindingHandle,
        EGV2SubmitUiInteractionResult Result);
};
