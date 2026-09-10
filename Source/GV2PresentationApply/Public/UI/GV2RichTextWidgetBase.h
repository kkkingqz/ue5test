#pragma once

#include "GV2PresentationApply/PreparedApplyTargets.h"
#include "GV2PresentationApply/GV2WidgetTypes.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2UiStyleConsumer.h"
#include "UI/GV2ScreenFieldHost.h"
#include "UI/GV2TextPipelineHost.h"
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
class GV2PRESENTATIONAPPLY_API UGV2RichTextWidgetBase
    : public UCommonUserWidget
    , public IGV2UiPropertyHost
    , public IGV2UiStyleConsumer
    , public IGV2ScreenFieldHost
    , public IGV2TextPipelineHost
    , public IGV2PreparedTextTarget
    , public IGV2PreparedRichTextSpansTarget
    , public IGV2PreparedRichTextStyleTarget
{
    GENERATED_BODY()

public:
    // PSC-11: value sink for this class's central-style role. It only forwards finished
    // values into the physical write that already existed; no decision happens here.
    virtual void ApplyPreparedRichTextStyle(const GV2PresentationApply::FPreparedRichTextStyle& Style) override
    {
        ApplyRichTextStyleValues(Style);
    }

    // PSC-11: value sink for the prepared rich-text spans operation.
    virtual void ApplyPreparedRichTextSpans(
        const TArray<GV2PresentationApply::FPreparedRichTextSpan>& Spans) override;

    // PSC-11: value sink for the prepared text operation.
    virtual bool ApplyPreparedText(
        const GV2PresentationApply::FPreparedTextValue& Value,
        bool bIsReset,
        FString& OutError) override;

    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

    // IGV2ScreenFieldHost (DUC-02): same shared HostIdentity every IGV2UiPropertyHost
    // carries -- see FGV2UiPropertyHostState.
    virtual FName GetScreenFieldId() const override { return GetHostIdentity(); }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Properties")
    void SetKey(FName InKey) { GetPropertyHostState().SetKey(InKey); }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Properties")
    FName GetKey() const { return GetPropertyHostState().GetKey(); }

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

    // PSC-10B: sole physical central-style write for this class. Unlike the other style
    // roles it also STORES what it receives: a rich-text run is styled by a Slate decorator
    // synchronously during rendering, so those resolutions cannot happen at Apply time.
    // What is stored is finished values, never a Theme -- the decorator can no longer reach
    // an authority even in principle, which is the property that matters.
    void ApplyRichTextStyleValues(const GV2PresentationApply::FPreparedRichTextStyle& InStyle);

    const GV2PresentationApply::FPreparedRichTextStyle& GetPreparedRichTextStyle() const { return PreparedStyle; }
    UClass* GetPreparedPopoverClass() const { return PreparedPopoverClass.Get(); }

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCommonRichTextBlock> RichTextBlock;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UScrollBox> RichTextScrollBox;


    UPROPERTY(EditAnywhere, Category = "GV2|UI|Identity", meta = (ShowOnlyInnerProperties))
    FGV2UiPropertyHostState PropertyHostState;

    const GV2PresentationApply::FPreparedRichTextTokenStyle& FindPreparedTokenStyle(FName StyleToken) const;
    TSubclassOf<UCommonTextStyle> ResolvePreparedStyleClass(FName StyleToken) const;
    FTextBlockStyle ScalePreparedTokenStyle(const GV2PresentationApply::FPreparedRichTextTokenStyle& TokenStyle) const;

    // PSC-10B: values delivered by a prepared central-style operation. bIsResolved false
    // means no runtime style has arrived; serialized widget defaults remain untouched.
    GV2PresentationApply::FPreparedRichTextStyle PreparedStyle;

    // FPreparedRichTextStyle is deliberately a non-reflected lower-module value, so nothing
    // in it is visible to the garbage collector. The widget therefore anchors every UObject
    // the stored style references -- the popover class, and every style class in the token
    // tables -- for as long as it holds the style. Anchoring only the popover class (as an
    // earlier revision did) would have left the token tables' classes unreferenced.
    UPROPERTY(Transient)
    TSubclassOf<UUserWidget> PreparedPopoverClass;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UObject>> PreparedStyleAnchors;

    UPROPERTY(Transient)
    FGV2TextViewModel CurrentText;

    UPROPERTY(Transient)
    TArray<FGV2RichTextSpanViewModel> CurrentSpans;

    TMap<FName, int32> SpanIndexById;
};
