#pragma once

#include "CommonUserWidget.h"
#include "UI/GV2UiStyleConsumer.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2UiBindingTarget.h"
#include "UI/GV2UiInteractionEmitter.h"
#include "UI/GV2ScreenFieldHost.h"
#include "UI/GV2TextPipelineHost.h"
#include "GV2CheckboxWidgetBase.generated.h"

class UCommonTextBlock;
class UCheckBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FGV2CheckboxBindingInvoked,
    FGV2UiBindingHandle, BindingHandle,
    bool, bIsChecked,
    EGV2SubmitUiInteractionResult, Result);

UCLASS(Blueprintable)
class GV2_API UGV2CheckboxWidgetBase
    : public UCommonUserWidget
    , public IGV2UiStyleConsumer
    , public IGV2UiPropertyHost
    , public IGV2UiBindingTarget
    , public IGV2ScreenFieldHost
    , public IGV2TextPipelineHost
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    EGV2SubmitUiInteractionResult SubmitCheckboxState(bool bInIsChecked);

    UPROPERTY(BlueprintAssignable, Category = "GV2|UI")
    FGV2CheckboxBindingInvoked OnBindingInvoked;

    UCheckBox* GetCheckBox() const { return Checkbox; }
    UCommonTextBlock* GetLabelText() const { return LabelText; }

    // IGV2UiPropertyHost interface
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

    // IGV2UiBindingTarget interface
    virtual void SetBindingHandle(const FGV2UiBindingHandle& InHandle) override;
    virtual FGV2UiBindingHandle GetBindingHandle() const override;

    // IGV2ScreenFieldHost (DUC-02): same shared HostIdentity every IGV2UiPropertyHost
    // carries -- see FGV2UiPropertyHostState.
    virtual FName GetScreenFieldId() const override { return GetHostIdentity(); }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    void SetKey(FName InKey) { GetPropertyHostState().SetKey(InKey); }

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    FName GetKey() const { return GetPropertyHostState().GetKey(); }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    void SetIsChecked(bool bInIsChecked);

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    bool IsChecked() const;

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    void SetIsReadOnly(bool bInIsReadOnly);

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    bool IsReadOnly() const;

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    bool ApplyText(const FGV2TextViewModel& InText);

    virtual bool ApplyCentralStyle_Implementation() override;

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCheckBox> Checkbox;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCommonTextBlock> LabelText;

private:
    UFUNCTION()
    void HandleCheckStateChanged(bool bInIsChecked);

    UPROPERTY(Transient)
    FGV2UiBindingHandle BindingHandle;


    UPROPERTY(Transient)
    bool bIsReadOnly = false;

    UPROPERTY(Transient)
    FGV2TextViewModel AppliedText;

    UPROPERTY(EditAnywhere, Category = "GV2|UI|Identity", meta = (ShowOnlyInnerProperties))
    FGV2UiPropertyHostState PropertyHostState;
};
