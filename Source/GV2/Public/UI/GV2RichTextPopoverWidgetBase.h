#pragma once

#include "GV2PresentationApply/PreparedApplyTargets.h"
#include "Bridge/GV2BridgeTypes.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2TextPipelineHost.h"
#include "CommonUserWidget.h"
#include "GV2RichTextPopoverWidgetBase.generated.h"

class UBorder;
class UCommonTextBlock;
class UGV2RichTextWidgetBase;
class UImage;
class USizeBox;

UCLASS(Blueprintable)
// PSC-10B: deliberately NOT an IGV2UiStyleConsumer. That interface is the enumerator for
// widgets a screen's subtree walk styles; a popover is never in a prepared screen subtree,
// so declaring it there would have made the implementation inventory report a class as
// covered by a preparer branch production never executes. Its role
// (FPreparedRichTextPopoverStyle) is prepared with its owner's and delivered through
// InitializePopover.
class GV2_API UGV2RichTextPopoverWidgetBase
    : public UCommonUserWidget
    , public IGV2UiPropertyHost
    , public IGV2TextPipelineHost
    , public IGV2PreparedRichTextPopoverStyleTarget
{
    GENERATED_BODY()

public:
    // PSC-11: value sink for this class's central-style role. It only forwards finished
    // values into the physical write that already existed; no decision happens here.
    virtual void ApplyPreparedRichTextPopoverStyle(const GV2PresentationApply::FPreparedRichTextPopoverStyle& Style) override
    {
        ApplyPopoverStyleValues(Style);
    }

    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Properties")
    void SetKey(FName InKey) { GetPropertyHostState().SetKey(InKey); }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Properties")
    FName GetKey() const { return GetPropertyHostState().GetKey(); }

    // PSC-10B: the ONLY entry point. A popover is created at hover time, after the screen
    // transaction, so it can never be the target of its own prepared operation -- it takes
    // the values from the RichText widget that creates it, which took them from Prepare, and
    // routes them through a fresh transaction solely for physical application. Style is a
    // required argument rather than an earlier separate call precisely so an uninitialised
    // popover is not expressible.
    //
    // Passing the creator's whole FPreparedRichTextStyle is deliberate: this popover's own
    // description is a RichText widget and needs the same token tables, and a hover span
    // inside it terminates on the same values.
    bool InitializePopover(
        const FGV2RichTextHoverViewModel& InModel,
        const GV2PresentationApply::FPreparedRichTextStyle& InStyle);

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Rich Text")
    const FGV2RichTextHoverViewModel& GetPopoverModel() const;

    // PSC-10B/11: sole physical central-style write for this class. The ephemeral popover is
    // created after its owner's screen transaction, but its already-prepared role still
    // arrives through the ordinary facade, via IGV2PreparedRichTextPopoverStyleTarget.
    void ApplyPopoverStyleValues(
        const GV2PresentationApply::FPreparedRichTextPopoverStyle& InStyle);

protected:
    virtual void NativePreConstruct() override;

    UFUNCTION(BlueprintImplementableEvent, Category = "GV2|UI|Rich Text", meta = (DisplayName = "On Popover Applied"))
    void OnPopoverApplied();

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UBorder> PopoverBorder;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<USizeBox> PopoverWidth;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCommonTextBlock> TitleText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UGV2RichTextWidgetBase> DescriptionText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UImage> Icon;

    UPROPERTY(EditAnywhere, Category = "GV2|UI|Identity", meta = (ShowOnlyInnerProperties))
    FGV2UiPropertyHostState PropertyHostState;

    // See UGV2RichTextWidgetBase for why a stored prepared style needs explicit anchors:
    // the struct is a non-reflected lower-module value and the collector cannot see into it.
    GV2PresentationApply::FPreparedRichTextStyle PreparedStyle;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UObject>> PreparedStyleAnchors;

    UPROPERTY(Transient)
    FGV2RichTextHoverViewModel Model;
};
