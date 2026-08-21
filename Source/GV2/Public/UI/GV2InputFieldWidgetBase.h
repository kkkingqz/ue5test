#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "CommonUserWidget.h"
#include "UI/GV2DynamicScreenElement.h"
#include "UI/GV2UiStyleConsumer.h"
#include "Types/SlateEnums.h"
#include "GV2InputFieldWidgetBase.generated.h"

class UCommonTextBlock;
class UEditableTextBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FGV2InputFieldBindingInvoked,
    FGV2UiBindingHandle, BindingHandle,
    FString, TextValue,
    EGV2SubmitUiInteractionResult, Result);

UCLASS(Blueprintable)
class GV2_API UGV2InputFieldWidgetBase
    : public UCommonUserWidget
    , public IGV2DynamicScreenElement
    , public IGV2UiStyleConsumer
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    bool ApplyInputFieldModel(const FGV2InputFieldViewModel& InputFieldModel);

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    bool CanApplyInputFieldModel(const FGV2InputFieldViewModel& InputFieldModel) const;

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    EGV2SubmitUiInteractionResult SubmitTextValue(const FString& NewTextValue);

    UPROPERTY(BlueprintAssignable, Category = "GV2|UI")
    FGV2InputFieldBindingInvoked OnBindingInvoked;

    UEditableTextBox* GetEditableTextBox() const { return EditableTextBox; }
    UCommonTextBlock* GetLabelText() const { return LabelText; }

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
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Screen Field")
    FName ScreenFieldId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Screen Field")
    bool bScreenFieldRequired = true;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UEditableTextBox> EditableTextBox;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, OptionalWidget = true))
    TObjectPtr<UCommonTextBlock> LabelText;

private:
    UFUNCTION()
    void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

    UPROPERTY(Transient)
    FGV2InputFieldViewModel AppliedInputFieldModel;
};
