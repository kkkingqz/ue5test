#pragma once

#include "CommonUserWidget.h"
#include "UI/GV2DynamicScreenElement.h"
#include "UI/GV2UiStyleConsumer.h"
#include "GV2ModalWidgetBase.generated.h"

class UCommonTextBlock;
class UButton;
class UGV2ButtonListWidgetBase;

/**
 * UGV2ModalWidgetBase (UIF-15, ADR-0035)
 * Modal dialogue widget with title, message content, action buttons, and backdrop dimming.
 * Belongs to the modal_stack presentation layer and blocks lower layers from interaction.
 * Implements IGV2DynamicScreenElement for "core:schema.ui_field.modal.v1".
 */
UCLASS(Blueprintable)
class GV2_API UGV2ModalWidgetBase
    : public UCommonUserWidget
    , public IGV2DynamicScreenElement
    , public IGV2UiStyleConsumer
{
    GENERATED_BODY()

public:
    // IGV2DynamicScreenElement
    virtual FGV2ScreenFieldDescriptor GetScreenFieldDescriptor_Implementation() const override;
    virtual bool CanApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value) const override;
    virtual bool CaptureScreenField_Implementation(FGV2ScreenFieldValue& OutFieldValue) const override;
    virtual bool ApplyScreenField_Implementation(const FGV2ScreenFieldValue& Value) override;
    virtual bool ResetScreenField_Implementation() override;

    // IGV2UiStyleConsumer
    virtual bool ApplyCentralStyle_Implementation() override;

protected:
    virtual void NativePreConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UCommonTextBlock> TitleText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UCommonTextBlock> ContentText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UGV2ButtonListWidgetBase> ButtonList;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> BackdropButton;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Modal")
    FName FieldId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Modal")
    FString SchemaId = TEXT("core:schema.ui_field.modal.v1");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Modal")
    bool bIsRequired = false;

private:
    FGV2ModalViewModel CurrentModel;
};
