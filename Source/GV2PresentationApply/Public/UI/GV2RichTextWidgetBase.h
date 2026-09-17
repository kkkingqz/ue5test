#pragma once

#include "GV2PresentationApply/PreparedApplyTargets.h"
#include "GV2PresentationApply/GV2WidgetTypes.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2UiStyleConsumer.h"
#include "UI/GV2ScreenFieldHost.h"
#include "UI/GV2TextPipelineHost.h"
#include "CommonUserWidget.h"
#include "Layout/SlateRect.h"
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

// PEP-06A: replacement for IToolTip::OnOpening/OnClosed as the hover signal. Rect is in
// absolute (desktop) space -- the same space FGeometry::AbsoluteToLocal converts from, so
// a caller positioning content inside any layer (overlay_stack included) can convert
// against that layer's own geometry without this type needing to know which layer that is.
struct GV2PRESENTATIONAPPLY_API FGV2RichTextSpanAnchor
{
    FName SpanId;
    FSlateRect Rect;
};

enum class EGV2SpanHoverTransition : uint8
{
    None,
    Began,
    Ended,
    // The point moved directly from one span's rect to a different span's, with no gap
    // reported in between -- the caller decides whether that is one continuous hover or a
    // close-then-open; this detector only reports what geometrically happened.
    Changed,
};

struct GV2PRESENTATIONAPPLY_API FGV2SpanHoverState
{
    FName HoveredSpanId;
};

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
    , public IGV2PreparedViewportRefreshTarget
{
    GENERATED_BODY()

public:
    virtual void RefreshPreparedViewportPresentation(float ViewportHeight) override;

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

    // PEP-06B: walks the real rendered interactive-span sub-widgets this text block's
    // decorator created (SRichTextHyperlink instances, found via SWidget::GetChildren())
    // and returns one anchor per span whose hover content is non-empty. Requires a real
    // paint pass to have happened (the geometry comes from live Slate arrangement, not a
    // manual layout walk); returns an empty array before first paint or if RichTextBlock is
    // unbound. Polled every NativeTick to drive the hover overlay below.
    TArray<FGV2RichTextSpanAnchor> CaptureHoverableSpanAnchors() const;

    // Pure point-in-rect test, no widget or cursor access -- callable with a synthetic
    // point, which is exactly how this is tested (see GV2RichTextSpanHoverDetectorTests).
    static const FGV2RichTextSpanAnchor* HitTestSpanAnchors(
        TArrayView<const FGV2RichTextSpanAnchor> Anchors,
        const FVector2D& Point);

    // Advances State against this poll's Anchors and Point (unset meaning the cursor is not
    // over this widget at all, e.g. it left the widget's own bounds entirely) and returns
    // which transition happened, if any; OutSpanId names the span that began or ended, or
    // the newly-hovered span for Changed. Pure state advance, no tick/timer dependency --
    // feeding a manual sequence of points reproduces any real hover sequence exactly.
    static EGV2SpanHoverTransition AdvanceSpanHoverState(
        FGV2SpanHoverState& State,
        TArrayView<const FGV2RichTextSpanAnchor> Anchors,
        const TOptional<FVector2D>& Point,
        FName& OutSpanId);

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeDestruct() override;

    // PEP-06B: replaces the old Slate-tooltip open/close signal (CreateSpanToolTip's
    // FGV2RichTextSpanToolTip is now only a correlation vessel, never a visible tooltip --
    // see its own doc comment in the .cpp). Every tick: recapture anchors, advance the
    // hover state machine against the live cursor position, open/close/reposition the
    // hover overlay to match. This is the sole caller of Open/CloseHoverOverlay.
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    void OpenHoverOverlayForSpan(FName SpanId);
    void CloseActiveHoverOverlay();
    void RepositionActiveHoverOverlay(FName SpanId, const TArray<FGV2RichTextSpanAnchor>& Anchors);

protected:

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCommonRichTextBlock> RichTextBlock;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UScrollBox> RichTextScrollBox;


    UPROPERTY(EditAnywhere, Category = "GV2|UI|Identity", meta = (ShowOnlyInnerProperties))
    FGV2UiPropertyHostState PropertyHostState;

    const GV2PresentationApply::FPreparedRichTextTokenStyle& FindPreparedTokenStyle(FName StyleToken) const;
    TSubclassOf<UCommonTextStyle> ResolvePreparedStyleClass(FName StyleToken) const;
    FTextBlockStyle ScalePreparedTokenStyle(const GV2PresentationApply::FPreparedRichTextTokenStyle& TokenStyle) const;
    FTextBlockStyle ScalePreparedTokenStyleAtHeight(
        const GV2PresentationApply::FPreparedRichTextTokenStyle& TokenStyle,
        float ViewportHeight) const;

    // PSC-10B: values delivered by a prepared central-style operation. bIsResolved false
    // means no runtime style has arrived; serialized widget defaults remain untouched.
    GV2PresentationApply::FPreparedRichTextStyle PreparedStyle;

    // FPreparedRichTextStyle is deliberately a non-reflected lower-module value, so nothing
    // in it is visible to the garbage collector. The widget therefore anchors every UObject
    // the stored style references -- every style class in the token tables -- for as long
    // as it holds the style.
    UPROPERTY(Transient)
    TArray<TObjectPtr<UObject>> PreparedStyleAnchors;

    UPROPERTY(Transient)
    FGV2TextViewModel CurrentText;

    UPROPERTY(Transient)
    TArray<FGV2RichTextSpanViewModel> CurrentSpans;

    TMap<FName, int32> SpanIndexById;

    // PEP-06B: the hover overlay currently open for this widget, if any -- transient,
    // per-instance detector state, never serialized/reflected the same way HoverState below
    // isn't. ActiveHoverWidget is the exact widget CloseHoverOverlay's owner (the sink) was
    // handed at open time; not itself an owning reference (the sink/reconciler owns it once
    // attached, PSC-11), used only to reposition it and to know it was this widget.
    FGV2SpanHoverState HoverState;
    FName ActiveHoverInstanceKey;
    TWeakObjectPtr<UUserWidget> ActiveHoverWidget;
};
