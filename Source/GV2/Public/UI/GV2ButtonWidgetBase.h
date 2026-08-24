#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "CommonButtonBase.h"
#include "UI/GV2UiStyleConsumer.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2UiBindingTarget.h"
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
    , public IGV2UiPropertyHost
    , public IGV2UiBindingTarget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Button")
    bool ApplyText(const FGV2TextViewModel& InText);

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Button")
    void SetKey(FName InKey);

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Button")
    FName GetKey() const;

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Button")
    void SetAutomaticInteractionSubmission(bool bEnabled);

    UPROPERTY(BlueprintAssignable, Category = "GV2|UI")
    FGV2ButtonActivated OnActivated;

    UPROPERTY(BlueprintAssignable, Category = "GV2|UI")
    FGV2ButtonBindingInvoked OnBindingInvoked;

    UCommonTextBlock* GetLabelText() const { return LabelText; }

    virtual bool ApplyCentralStyle_Implementation() override;

    // IGV2UiPropertyHost
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

    // IGV2UiBindingTarget
    virtual void SetBindingHandle(const FGV2UiBindingHandle& InBindingHandle) override;
    virtual FGV2UiBindingHandle GetBindingHandle() const override;

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeOnClicked() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCommonTextBlock> LabelText;

private:
    UPROPERTY(Transient)
    FGV2UiBindingHandle BindingHandle;

    UPROPERTY(Transient)
    FName Key;

    UPROPERTY(Transient)
    FName CurrentTextStyleToken;

    FGV2UiPropertyHostState PropertyHostState;

    bool bAutomaticInteractionSubmission = true;
};
