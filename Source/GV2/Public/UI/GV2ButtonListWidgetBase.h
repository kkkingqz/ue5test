#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "CommonUserWidget.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2UiStyleConsumer.h"
#include "GV2ButtonListWidgetBase.generated.h"

class UGV2ButtonWidgetBase;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FGV2ButtonListBindingInvoked,
    FGV2UiBindingHandle, BindingHandle,
    EGV2SubmitUiInteractionResult, Result);

UCLASS(Blueprintable)
class GV2_API UGV2ButtonListWidgetBase
    : public UCommonUserWidget
    , public IGV2UiPropertyHost
    , public IGV2UiStyleConsumer
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "GV2|UI")
    FGV2ButtonListBindingInvoked OnBindingInvoked;

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    bool ApplyButtonModels(const TArray<FGV2ButtonViewModel>& InModels);

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    bool CanApplyButtonModels(const TArray<FGV2ButtonViewModel>& InModels) const;

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    void ResetButtonModels();

    const TArray<FGV2ButtonViewModel>& GetAppliedButtonModels() const { return AppliedModels; }

    UVerticalBox* GetButtonContainer() const { return ButtonContainer; }
    void SetButtonContainer(UVerticalBox* InContainer) { ButtonContainer = InContainer; }

    UGV2ButtonWidgetBase* GetButton(FName Key) const;

    TSubclassOf<UGV2ButtonWidgetBase> ResolveButtonWidgetClass() const;

    virtual bool ApplyCentralStyle_Implementation() override;

    // IGV2UiPropertyHost
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

protected:
    virtual void NativePreConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UVerticalBox> ButtonContainer;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GV2|UI")
    TSubclassOf<UGV2ButtonWidgetBase> ButtonWidgetClass;

private:
    UFUNCTION()
    void HandleButtonBindingInvoked(
        FGV2UiBindingHandle BindingHandle,
        EGV2SubmitUiInteractionResult Result);

    UPROPERTY(Transient)
    TArray<FGV2ButtonViewModel> AppliedModels;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UGV2ButtonWidgetBase>> ButtonsByKey;

    FGV2UiPropertyHostState PropertyHostState;
};
