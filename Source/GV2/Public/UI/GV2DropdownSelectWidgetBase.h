#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "CommonUserWidget.h"
#include "UI/GV2DynamicScreenElement.h"
#include "UI/GV2UiStyleConsumer.h"
#include "GV2DropdownSelectWidgetBase.generated.h"

class UBorder;
class USizeBox;
class UScrollBox;
class UGV2ButtonWidgetBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FGV2DropdownSelectionInvoked,
    FGV2UiBindingHandle, BindingHandle,
    EGV2SubmitUiInteractionResult, Result);

UCLASS(Abstract, Blueprintable)
class GV2_API UGV2DropdownSelectWidgetBase
    : public UCommonUserWidget
    , public IGV2DynamicScreenElement
    , public IGV2UiStyleConsumer
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    bool ApplyDropdownModel(const FGV2DropdownSelectViewModel& InModel);

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    bool CanApplyDropdownModel(const FGV2DropdownSelectViewModel& InModel) const;

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    bool IsDropdownOpen() const;

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    EGV2SubmitUiInteractionResult SubmitSelection(FName SelectedKey);

    UPROPERTY(BlueprintAssignable, Category = "GV2|UI")
    FGV2DropdownSelectionInvoked OnSelectionInvoked;

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
    virtual void NativeOnInitialized() override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Screen Field")
    FName ScreenFieldId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Screen Field")
    bool bScreenFieldRequired = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GV2|UI")
    TSubclassOf<UGV2ButtonWidgetBase> OptionWidgetClass;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UGV2ButtonWidgetBase> HeaderButton;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UBorder> PopupBorder;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<USizeBox> PopupSizeBox;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UScrollBox> OptionsScrollBox;

private:
    TSubclassOf<UGV2ButtonWidgetBase> ResolveOptionWidgetClass() const;

    UFUNCTION()
    void HandleHeaderClicked();

    UFUNCTION()
    void HandleOptionActivated(FName Key);

    void SetDropdownOpen(bool bOpen);
    void UpdateHeaderLabel();

    UPROPERTY(Transient)
    FGV2DropdownSelectViewModel AppliedModel;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UGV2ButtonWidgetBase>> OptionsByKey;

    bool bIsOpen = false;
};
