#include "UI/GV2RichTextPopoverWidgetBase.h"

#include "Components/Border.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2UiCapability.h"

void UGV2RichTextPopoverWidgetBase::RefreshPreparedViewportPresentation(float ViewportHeight)
{
    if (!PreparedStyle.bIsResolved || ViewportHeight <= 0.0f)
    {
        return;
    }
    const GV2PresentationApply::FPreparedRichTextPopoverStyle& Style = PreparedStyle.PopoverStyle;
    if (PopoverWidth != nullptr)
    {
        const float ViewportScale = GV2PresentationApply::EvaluatePreparedViewportScale(
            Style.Scale,
            ViewportHeight);
        PopoverWidth->SetMaxDesiredWidth(Style.MaxWidth * ViewportScale);
        PopoverWidth->SetMaxDesiredHeight(Style.MaxHeight * ViewportScale);
    }
    // PEP-05 (PSC-11): ContentBox's one child is a generic nested screen this popover never
    // built. Its subtree refresh goes through the SAME single Apply facade every other
    // physical mutation uses -- FPreparedViewportRefreshOperation, the exact case a
    // top-level screen's own refresh already uses -- rather than a second exported entry
    // point that could mutate a widget outside that one facade.
    if (UWidget* Content = Model.ScreenWidget.Get())
    {
        GV2PresentationApply::FGV2PreparedPresentationTransaction RefreshTransaction;
        GV2PresentationApply::FPreparedViewportRefreshOperation RefreshOperation;
        RefreshOperation.RootWidget = Content;
        RefreshOperation.ViewportHeight = ViewportHeight;
        RefreshTransaction.AddViewportRefreshOperation(MoveTemp(RefreshOperation));
        FGV2PresentationApplyResult RefreshResult;
        FGV2PresentationApply::Apply(RefreshTransaction, RefreshResult);
    }
}

void UGV2RichTextPopoverWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    // PSC-10B: style arrives with the content through InitializePopover.
}

bool UGV2RichTextPopoverWidgetBase::InitializePopover(
    const FGV2RichTextHoverViewModel& InModel,
    const GV2PresentationApply::FPreparedRichTextStyle& InStyle)
{
    if (PopoverBorder == nullptr || PopoverWidth == nullptr || ContentBox == nullptr
        || !InStyle.bIsResolved || InModel.IsEmpty())
    {
        return false;
    }

    // Only the popover's OWN border/background style is applied here -- the nested screen
    // InModel.ScreenWidget carries was already styled in Prepare (GV2CentralStylePreparer::
    // PrepareForSubtree, FGV2RichTextSpansPropertyConsumer::Prepare). Creating a transaction
    // here performs no Prepare and no lookup; it keeps this one physical write behind the
    // same exhaustive facade every other role uses.
    PreparedStyle = InStyle;
    PreparedStyleAnchors.Reset();
    if (InStyle.PopoverClass != nullptr)
    {
        PreparedStyleAnchors.AddUnique(InStyle.PopoverClass.Get());
    }
    GV2PresentationApply::FGV2PreparedPresentationTransaction StyleTransaction;
    GV2PresentationApply::FPreparedCentralStyleOperation PopoverOperation;
    PopoverOperation.TargetWidget = this;
    PopoverOperation.Payload.Set<GV2PresentationApply::FPreparedRichTextPopoverStyle>(InStyle.PopoverStyle);
    StyleTransaction.AddCentralStyleOperation(MoveTemp(PopoverOperation));
    FGV2PresentationApplyResult ApplyResult;
    if (!FGV2PresentationApply::Apply(StyleTransaction, ApplyResult))
    {
        return false;
    }

    Model = InModel;
    UWidget* Content = Model.ScreenWidget.Get();
    if (Content == nullptr)
    {
        return false;
    }
    ContentBox->ClearChildren();
    ContentBox->AddChild(Content);

    OnPopoverApplied();
    return true;
}

const FGV2RichTextHoverViewModel& UGV2RichTextPopoverWidgetBase::GetPopoverModel() const
{
    return Model;
}

void UGV2RichTextPopoverWidgetBase::ApplyPopoverStyleValues(
    const GV2PresentationApply::FPreparedRichTextPopoverStyle& InStyle)
{
    if (PopoverBorder == nullptr || PopoverWidth == nullptr)
    {
        return;
    }

    // DCA-15 (ADR-0035): the popover's own box follows the same viewport-derived
    // scale as its content -- a fixed max width/height (the Theme default, unscaled)
    // would cap the box at its 1080p footprint even where the content inside is
    // rendering ~60% larger (2160p) or ~15% smaller (720p).
    PopoverBorder->SetBrush(InStyle.Background);
    PopoverBorder->SetPadding(InStyle.Padding);
    const float ViewportScale = GV2PresentationApply::EvaluatePreparedViewportScale(
        InStyle.Scale,
        GV2PresentationApply::ResolveLiveViewportHeight(this, InStyle.Scale.ReferenceViewportHeight));
    PopoverWidth->SetMaxDesiredWidth(InStyle.MaxWidth * ViewportScale);
    PopoverWidth->SetMaxDesiredHeight(InStyle.MaxHeight * ViewportScale);
}

void UGV2RichTextPopoverWidgetBase::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    // PEP-05: content is a nested screen (ContentBox), not a declared property of this
    // host -- the same reason FGV2TabContainerTabsPropertyConsumer's own screens declare
    // no per-field capability here either. Only host identity remains.
    OutBuilder.AddKey(TEXT("key"), NAME_None);
}
