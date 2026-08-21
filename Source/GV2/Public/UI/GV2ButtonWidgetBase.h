#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "CommonButtonBase.h"
#include "UI/GV2UiStyleConsumer.h"
#include "GV2ButtonWidgetBase.generated.h"

class UCommonTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FGV2ButtonBindingInvoked,
    FGV2UiBindingHandle, BindingHandle,
    EGV2SubmitUiInteractionResult, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FGV2ButtonActivated,
    FName, Key);

UCLASS(Blueprintable)
class GV2_API UGV2ButtonWidgetBase
    : public UCommonButtonBase
    , public IGV2UiStyleConsumer
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    void ApplyButtonModel(const FGV2ButtonViewModel& InButtonModel);

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    FGV2ButtonViewModel GetButtonModel() const;

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    void SetAutomaticInteractionSubmission(bool bEnabled);

    UPROPERTY(BlueprintAssignable, Category = "GV2|UI")
    FGV2ButtonActivated OnActivated;

    UPROPERTY(BlueprintAssignable, Category = "GV2|UI")
    FGV2ButtonBindingInvoked OnBindingInvoked;

    UCommonTextBlock* GetLabelText() const { return LabelText; }

    virtual bool ApplyCentralStyle_Implementation() override;

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeOnClicked() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCommonTextBlock> LabelText;

private:
    UPROPERTY(Transient)
    FGV2ButtonViewModel ButtonModel;

    bool bAutomaticInteractionSubmission = true;
};
