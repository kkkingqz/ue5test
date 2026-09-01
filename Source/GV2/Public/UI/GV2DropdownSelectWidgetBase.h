#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "CommonUserWidget.h"
#include "UI/GV2UiBindingTarget.h"
#include "UI/GV2UiPropertyHost.h"
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

UCLASS(Blueprintable)
class GV2_API UGV2DropdownSelectWidgetBase
    : public UCommonUserWidget
    , public IGV2UiPropertyHost
    , public IGV2UiBindingTarget
    , public IGV2UiStyleConsumer
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    bool IsDropdownOpen() const { return bIsOpen; }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    void SetDropdownOpen(bool bOpen);

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    void SetSelectedKey(FName InKey);

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    FName GetSelectedKey() const { return SelectedKey; }

    bool ApplyPlaceholderText(const FGV2TextViewModel& InText);
    const FGV2TextViewModel& GetPlaceholderText() const { return CurrentPlaceholder; }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    EGV2SubmitUiInteractionResult SubmitSelection(FName InSelectedKey);

    UPROPERTY(BlueprintAssignable, Category = "GV2|UI")
    FGV2DropdownSelectionInvoked OnSelectionInvoked;

    UGV2ButtonWidgetBase* GetHeaderButton() const { return HeaderButton; }
    UScrollBox* GetOptionsScrollBox() const { return OptionsScrollBox; }

    void SetHeaderButton(UGV2ButtonWidgetBase* InBtn) { HeaderButton = InBtn; }
    void SetOptionsScrollBox(UScrollBox* InBox) { OptionsScrollBox = InBox; }
    void SetPopupBorder(UBorder* InBorder) { PopupBorder = InBorder; }
    void SetPopupSizeBox(USizeBox* InSizeBox) { PopupSizeBox = InSizeBox; }

    TSubclassOf<UGV2ButtonWidgetBase> ResolveOptionWidgetClass() const;
    void UpdateHeaderLabel();

    virtual bool ApplyCentralStyle_Implementation() override;

    // IGV2UiPropertyHost
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

    // IGV2UiBindingTarget
    virtual void SetBindingHandle(const FGV2UiBindingHandle& InBindingHandle) override;
    virtual FGV2UiBindingHandle GetBindingHandle() const override { return CurrentBinding; }
    void SetInteractionEnabled(bool bEnabled);

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeOnInitialized() override;

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
    UFUNCTION()
    void HandleHeaderClicked();

    UFUNCTION()
    void HandleHeaderActivated(FName Key);

    UFUNCTION()
    void HandleOptionActivated(FName Key);

    UPROPERTY(Transient)
    FGV2TextViewModel CurrentPlaceholder;

    UPROPERTY(Transient)
    FName SelectedKey;

    UPROPERTY(Transient)
    FGV2UiBindingHandle CurrentBinding;

    UPROPERTY(EditAnywhere, Category = "GV2|UI|Identity", meta = (ShowOnlyInnerProperties))
    FGV2UiPropertyHostState PropertyHostState;

    bool bIsOpen = false;
};
