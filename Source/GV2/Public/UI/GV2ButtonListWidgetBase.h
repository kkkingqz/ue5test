#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "CommonUserWidget.h"
#include "UI/GV2DynamicScreenElement.h"
#include "UI/GV2UiStyleConsumer.h"
#include "GV2ButtonListWidgetBase.generated.h"

class UGV2ButtonWidgetBase;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FGV2ButtonListBindingInvoked,
    FGV2UiBindingHandle, BindingHandle,
    EGV2SubmitUiInteractionResult, Result);

UCLASS(Abstract, Blueprintable)
class GV2_API UGV2ButtonListWidgetBase
    : public UCommonUserWidget
    , public IGV2DynamicScreenElement
    , public IGV2UiStyleConsumer
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    bool ApplyButtonModels(const TArray<FGV2ButtonViewModel>& ButtonModels);

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    bool CanApplyButtonModels(const TArray<FGV2ButtonViewModel>& ButtonModels) const;

    UPROPERTY(BlueprintAssignable, Category = "GV2|UI")
    FGV2ButtonListBindingInvoked OnBindingInvoked;

    virtual FGV2ScreenFieldDescriptor GetScreenFieldDescriptor_Implementation() const override;
    virtual bool CanApplyScreenField_Implementation(
        const FGV2ScreenFieldValue& FieldValue) const override;
    virtual bool ApplyScreenField_Implementation(
        const FGV2ScreenFieldValue& FieldValue) override;
    virtual bool CaptureScreenField_Implementation(
        FGV2ScreenFieldValue& OutFieldValue) const override;
    virtual bool ResetScreenField_Implementation() override;
    virtual bool ApplyCentralStyle_Implementation() override;

protected:
    virtual void NativePreConstruct() override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Screen Field")
    FName ScreenFieldId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Screen Field")
    bool bScreenFieldRequired = true;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UVerticalBox> ButtonContainer;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GV2|UI")
    TSubclassOf<UGV2ButtonWidgetBase> ButtonWidgetClass;

private:
    TSubclassOf<UGV2ButtonWidgetBase> ResolveButtonWidgetClass() const;

    UFUNCTION()
    void HandleButtonBindingInvoked(
        FGV2UiBindingHandle BindingHandle,
        EGV2SubmitUiInteractionResult Result);

    UPROPERTY(Transient)
    TArray<FGV2ButtonViewModel> AppliedButtonModels;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UGV2ButtonWidgetBase>> ButtonsByKey;
};
