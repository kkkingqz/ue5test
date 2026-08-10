#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "CommonButtonBase.h"
#include "GV2ButtonWidgetBase.generated.h"

class UCommonTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FGV2ButtonBindingInvoked,
    FGV2UiBindingHandle, BindingHandle,
    EGV2SubmitUiInteractionResult, Result);

UCLASS(Abstract, Blueprintable)
class GV2_API UGV2ButtonWidgetBase : public UCommonButtonBase
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    void ApplyButtonModel(const FGV2ButtonViewModel& InButtonModel);

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    FGV2ButtonViewModel GetButtonModel() const;

    UPROPERTY(BlueprintAssignable, Category = "GV2|UI")
    FGV2ButtonBindingInvoked OnBindingInvoked;

protected:
    virtual void NativeOnClicked() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCommonTextBlock> LabelText;

private:
    UPROPERTY(Transient)
    FGV2ButtonViewModel ButtonModel;
};
