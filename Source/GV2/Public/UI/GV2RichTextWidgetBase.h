#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2UiStyleConsumer.h"
#include "UI/GV2ScreenFieldHost.h"
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
    , public IGV2UiPropertyHost
    , public IGV2UiStyleConsumer
    , public IGV2ScreenFieldHost
{
    GENERATED_BODY()

public:
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

    // IGV2ScreenFieldHost (DUC-02): same shared HostIdentity every IGV2UiPropertyHost
    // carries -- see FGV2UiPropertyHostState.
    virtual FName GetScreenFieldId() const override { return GetHostIdentity(); }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Properties")
    void SetKey(FName InKey) { Key = InKey; }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Properties")
    FName GetKey() const { return Key; }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Rich Text")
    bool ApplyText(const FGV2TextViewModel& InText);

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Rich Text")
    bool ApplyTextViewModel(const FGV2TextViewModel& InText) { return ApplyText(InText); }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Rich Text")
    const FGV2TextViewModel& GetTextViewModel() const { return CurrentText; }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Rich Text")
    bool ApplySpans(const TArray<FGV2RichTextSpanViewModel>& InSpans);

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Rich Text")
    const TArray<FGV2RichTextSpanViewModel>& GetSpans() const { return CurrentSpans; }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Rich Text")
    void ApplyInteractiveRichText(const FGV2TextViewModel& InText, const TArray<FGV2RichTextSpanViewModel>& InSpans);

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

    virtual bool ApplyCentralStyle_Implementation() override;

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCommonRichTextBlock> RichTextBlock;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UScrollBox> RichTextScrollBox;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Properties")
    FName Key;

    UPROPERTY(EditAnywhere, Category = "GV2|UI|Identity", meta = (ShowOnlyInnerProperties))
    FGV2UiPropertyHostState PropertyHostState;

    UPROPERTY(Transient)
    FGV2TextViewModel CurrentText;

    UPROPERTY(Transient)
    TArray<FGV2RichTextSpanViewModel> CurrentSpans;

    TMap<FName, int32> SpanIndexById;
};
