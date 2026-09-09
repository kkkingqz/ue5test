#pragma once

#include "GV2PresentationApply/PreparedApplyTargets.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"
#include "CommonUserWidget.h"
#include "UI/GV2UiStyleConsumer.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2UiBindingTarget.h"
#include "UI/GV2UiInteractionEmitter.h"
#include "UI/GV2ScreenFieldHost.h"
#include "UI/GV2TextPipelineHost.h"
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
    , public IGV2UiStyleConsumer
    , public IGV2UiPropertyHost
    , public IGV2UiBindingTarget
    , public IGV2ScreenFieldHost
    , public IGV2TextPipelineHost
    , public IGV2PreparedKeyTarget
    , public IGV2PreparedBindingTarget
    , public IGV2PreparedIntegerTarget
    , public IGV2PreparedInputFieldStyleTarget
{
    GENERATED_BODY()

public:
    // PSC-11: value sink for this class's central-style role. It only forwards finished
    // values into the physical write that already existed; no decision happens here.
    virtual void ApplyPreparedInputFieldStyle(const GV2PresentationApply::FPreparedInputFieldStyle& Style) override
    {
        ApplyInputFieldStyleValues(Style.WidgetStyle, Style.DefaultLabelStyle, Style.DefaultLabelScale);
    }

    // PSC-11: value sink for the prepared integer operation.
    virtual void ApplyPreparedInteger(int64 Value) override;

    // PSC-11: value sink for the prepared binding operation.
    virtual void ApplyPreparedBinding(const FString& SerializedHandle) override
    {
        SetBindingHandle(FGV2UiBindingHandle::FromSerialized(SerializedHandle));
    }

    // PSC-11: value sink for the prepared `key` operation. The generic identity write is the
    // same one every property host already performs; a host that routes a NAMED key
    // capability overrides this and falls back to it.
    virtual bool ApplyPreparedKey(FName PropertyName, FName Value) override
    {
        GetPropertyHostState().SetKey(Value);
        return true;
    }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    EGV2SubmitUiInteractionResult SubmitTextValue(const FString& NewTextValue);

    UPROPERTY(BlueprintAssignable, Category = "GV2|UI")
    FGV2InputFieldBindingInvoked OnBindingInvoked;

    UEditableTextBox* GetEditableTextBox() const { return EditableTextBox; }
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
    void SetIsReadOnly(bool bInIsReadOnly);

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    bool GetIsReadOnly() const;

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    void SetMaxLength(int64 InMaxLength);

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    int64 GetMaxLength() const { return MaxLength; }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    void SetValue(const FString& InValue);

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    FString GetValue() const;

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    bool ApplyText(const FGV2TextViewModel& InText);

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    bool ApplyPlaceholderText(const FGV2TextViewModel& InPlaceholder);

    // PSC-10B: sole physical central-style write for this class -- see
    // UGV2SeparatorWidgetBase::ApplySeparatorStyleValues for why this shape.
    void ApplyInputFieldStyleValues(
        const FEditableTextBoxStyle& InWidgetStyle,
        TSubclassOf<UCommonTextStyle> InDefaultLabelStyle,
        const GV2PresentationApply::FPreparedTextScalePolicy& InDefaultLabelScale);

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UEditableTextBox> EditableTextBox;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, OptionalWidget = true))
    TObjectPtr<UCommonTextBlock> LabelText;

private:
    UFUNCTION()
    void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

    UPROPERTY(Transient)
    FGV2UiBindingHandle BindingHandle;


    UPROPERTY(Transient)
    int64 MaxLength = 0;

    UPROPERTY(Transient)
    FGV2TextViewModel AppliedLabelText;

    UPROPERTY(Transient)
    FGV2TextViewModel AppliedPlaceholderText;

    UPROPERTY(EditAnywhere, Category = "GV2|UI|Identity", meta = (ShowOnlyInnerProperties))
    FGV2UiPropertyHostState PropertyHostState;
};
