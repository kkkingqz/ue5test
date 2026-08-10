#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "CommonUserWidget.h"
#include "GV2TestScreenWidgetBase.generated.h"

class UGV2ButtonListWidgetBase;
class UGV2RichTextWidgetBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FGV2TestScreenBindingInvoked,
    FGV2UiBindingHandle, BindingHandle,
    EGV2SubmitUiInteractionResult, Result);

UCLASS(Abstract, Blueprintable)
class GV2_API UGV2TestScreenWidgetBase : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "GV2|Testing")
    bool ApplyScreenModel(const FGV2TestScreenViewModel& ScreenModel);

    UPROPERTY(BlueprintAssignable, Category = "GV2|Testing")
    FGV2TestScreenBindingInvoked OnBindingInvoked;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UGV2RichTextWidgetBase> DescriptionText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UGV2ButtonListWidgetBase> ButtonList;

private:
    UFUNCTION()
    void HandleBindingInvoked(
        FGV2UiBindingHandle BindingHandle,
        EGV2SubmitUiInteractionResult Result);
};
