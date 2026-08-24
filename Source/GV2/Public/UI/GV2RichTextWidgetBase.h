#pragma once

#include "UI/GV2DynamicScreenElement.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2UiStyleConsumer.h"
#include "CommonUserWidget.h"
#include "GV2RichTextWidgetBase.generated.h"

class UCommonRichTextBlock;
class UScrollBox;
class IToolTip;
struct FHyperlinkStyle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FGV2RichTextSpanInvoked,
    FName, SpanId,
    FGV2UiBindingHandle, BindingHandle,
    EGV2SubmitUiInteractionResult, Result);

UCLASS(Blueprintable)
class GV2_API UGV2RichTextWidgetBase
    : public UCommonUserWidget
    , public IGV2DynamicScreenElement
    , public IGV2UiPropertyHost
    , public IGV2UiStyleConsumer
{
    GENERATED_BODY()

public:
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Properties")
    void SetKey(FName InKey) { Key = InKey; }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Properties")
    FName GetKey() const { return Key; }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Rich Text")
    void ApplyInteractiveRichText(const FGV2InteractiveRichTextViewModel& Content);

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Rich Text")
    bool HasInteractiveSpan(FName SpanId) const;

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Rich Text")
    EGV2SubmitUiInteractionResult SubmitSpanInteraction(FName SpanId);

    const FGV2RichTextSpanViewModel* FindInteractiveSpan(FName SpanId) const;
    TSharedRef<IToolTip> CreateSpanToolTip(FName SpanId);
    FTextBlockStyle ResolveRunTextStyle(FName Style, FName Color, FName Size) const;
    FHyperlinkStyle ResolveInteractiveTextStyle(const FTextBlockStyle& RunStyle) const;
    UCommonRichTextBlock* GetRichTextBlock() const;

    UPROPERTY(BlueprintAssignable, Category = "GV2|UI|Rich Text")
    FGV2RichTextSpanInvoked OnSpanInvoked;

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
    virtual void NativeDestruct() override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Screen Field")
    FName ScreenFieldId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Screen Field")
    bool bScreenFieldRequired = true;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCommonRichTextBlock> RichTextBlock;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UScrollBox> RichTextScrollBox;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Properties")
    FName Key;

    FGV2UiPropertyHostState PropertyHostState;

    UPROPERTY(Transient)
    FGV2InteractiveRichTextViewModel CurrentContent;

    TMap<FName, int32> SpanIndexById;
};
