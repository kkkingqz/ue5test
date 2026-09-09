#pragma once

#include "GV2PresentationApply/PreparedApplyTargets.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"
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
    , public IGV2PreparedTextTarget
    , public IGV2PreparedKeyedCollectionTarget
    , public IGV2PreparedBooleanTarget
    , public IGV2PreparedDropdownStyleTarget
{
    GENERATED_BODY()

public:
    // PSC-11: value sink for this class's central-style role. It only forwards finished
    // values into the physical write that already existed; no decision happens here.
    virtual void ApplyPreparedDropdownStyle(const GV2PresentationApply::FPreparedDropdownStyle& Style) override
    {
        ApplyDropdownStyleValues(Style.HeaderStyle, Style.PopupBackground, Style.PopupPadding, Style.OptionItemPadding, Style.MaxPopupHeight, Style.PopupScale);
    }

    // PSC-11: value sink for the prepared boolean operation.
    virtual void ApplyPreparedBoolean(FName PropertyName, bool bValue) override;

    // PSC-11: value sinks for the prepared keyed-collection operation.
    virtual UPanelWidget* GetPreparedCollectionPanel() const override;
    virtual void ResetPreparedCollection() override;
    virtual void OnPreparedCollectionSettled(
        const TArray<GV2PresentationApply::FPreparedKeyedCollectionEntry>& Entries) override;

    // PSC-11: value sink for the prepared text operation.
    virtual bool ApplyPreparedText(
        const GV2PresentationApply::FPreparedTextValue& Value,
        bool bIsReset,
        FString& OutError) override;

    // PSC-11: this host routes a NAMED key capability of its own before the generic
    // identity write; see the implementation.
    virtual bool ApplyPreparedKey(FName PropertyName, FName Value) override;

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

    // DCA-03: class element of the collection is set explicitly on the declaring asset
    // (Designer), not derived at runtime. A plain accessor, not a resolver -- there is
    // no fallback chain, so an unset class simply reads back as null.
    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    TSubclassOf<UGV2ButtonWidgetBase> GetOptionWidgetClass() const { return OptionWidgetClass; }
    void UpdateHeaderLabel();

    // PSC-10B: sole physical central-style write for this class -- see
    // UGV2SeparatorWidgetBase::ApplySeparatorStyleValues for why this shape. It styles its
    // own HeaderButton, which is why GV2CentralStylePreparer does not descend into a
    // dropdown: a dropdown's header is not a plain button and must not also receive the
    // generic button role.
    void ApplyDropdownStyleValues(
        TSubclassOf<UCommonButtonStyle> InHeaderStyle,
        const FSlateBrush& InPopupBackground,
        const FMargin& InPopupPadding,
        const FMargin& InOptionItemPadding,
        float InMaxPopupHeight,
        const GV2PresentationApply::FPreparedViewportScalePolicy& InPopupScale);

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
