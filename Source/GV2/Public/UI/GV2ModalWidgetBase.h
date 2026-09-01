#pragma once

#include "CommonUserWidget.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2UiBindingTarget.h"
#include "UI/GV2UiStyleConsumer.h"
#include "GV2ModalWidgetBase.generated.h"

class UCommonTextBlock;
class UButton;
class UGV2ButtonListWidgetBase;

/**
 * UGV2ModalWidgetBase (UIF-15, ADR-0035, UPP-22)
 * Modal dialogue widget with title, message content, action buttons, and backdrop dimming.
 * Belongs to the modal_stack presentation layer and blocks lower layers from interaction.
 * Implements IGV2UiPropertyHost for "core:schema.ui_field.modal.v1".
 */
UCLASS(Blueprintable)
class GV2_API UGV2ModalWidgetBase
    : public UCommonUserWidget
    , public IGV2UiPropertyHost
    , public IGV2UiBindingTarget
    , public IGV2UiStyleConsumer
{
    GENERATED_BODY()

public:
    // IGV2UiPropertyHost
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

    // IGV2UiBindingTarget
    virtual void SetBindingHandle(const FGV2UiBindingHandle& InBindingHandle) override;
    virtual FGV2UiBindingHandle GetBindingHandle() const override;

    // IGV2UiStyleConsumer
    virtual bool ApplyCentralStyle_Implementation() override;

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Modal")
    bool ApplyTitle(const FGV2TextViewModel& InTitle);

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Modal")
    const FGV2TextViewModel& GetTitle() const { return CurrentTitle; }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Modal")
    bool ApplyContent(const FGV2TextViewModel& InContent);

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Modal")
    const FGV2TextViewModel& GetContent() const { return CurrentContent; }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Modal")
    UGV2ButtonListWidgetBase* GetButtonList() const { return ButtonList; }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Modal")
    UCommonTextBlock* GetTitleText() const { return TitleText; }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Modal")
    UCommonTextBlock* GetContentText() const { return ContentText; }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Modal")
    UButton* GetBackdropButton() const { return BackdropButton; }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Modal")
    EGV2SubmitUiInteractionResult SubmitBackdropClose();

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Properties")
    void SetKey(FName InKey) { GetPropertyHostState().SetKey(InKey); }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Properties")
    FName GetKey() const { return GetPropertyHostState().GetKey(); }

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UFUNCTION()
    void HandleBackdropButtonClicked();

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UCommonTextBlock> TitleText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UCommonTextBlock> ContentText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UGV2ButtonListWidgetBase> ButtonList;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UButton> BackdropButton;


    FGV2UiPropertyHostState PropertyHostState;

    UPROPERTY(Transient)
    FGV2TextViewModel CurrentTitle;

    UPROPERTY(Transient)
    FGV2TextViewModel CurrentContent;

    UPROPERTY(Transient)
    FGV2UiBindingHandle BackdropCloseBinding;
};
