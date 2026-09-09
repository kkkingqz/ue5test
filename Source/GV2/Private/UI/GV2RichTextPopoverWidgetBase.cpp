#include "UI/GV2RichTextPopoverWidgetBase.h"

#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2LegacyPresentationApplyAdapter.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2UiCapability.h"

void UGV2RichTextPopoverWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    // PSC-10B: style arrives with the content through InitializePopover.
}

bool UGV2RichTextPopoverWidgetBase::InitializePopover(
    const FGV2RichTextHoverViewModel& InModel,
    const GV2PresentationApply::FPreparedRichTextStyle& InStyle)
{
    if (PopoverBorder == nullptr || PopoverWidth == nullptr
        || TitleText == nullptr || DescriptionText == nullptr
        || !InStyle.bIsResolved)
    {
        return false;
    }

    // Style before content: both this ephemeral target and its nested rich text receive
    // roles already prepared for the owner. Creating a transaction here performs no Prepare
    // and no lookup; it keeps every physical style write behind the same exhaustive facade.
    PreparedStyle = InStyle;
    PreparedStyleAnchors.Reset();
    if (InStyle.PopoverClass != nullptr)
    {
        PreparedStyleAnchors.AddUnique(InStyle.PopoverClass.Get());
    }
    if (InStyle.DefaultStyleClass != nullptr)
    {
        PreparedStyleAnchors.AddUnique(InStyle.DefaultStyleClass.Get());
    }
    for (const TPair<FName, GV2PresentationApply::FPreparedRichTextTokenStyle>& Pair : InStyle.StyleByToken)
    {
        if (Pair.Value.StyleClass != nullptr)
        {
            PreparedStyleAnchors.AddUnique(Pair.Value.StyleClass.Get());
        }
    }
    GV2PresentationApply::FGV2PreparedPresentationTransaction StyleTransaction;
    GV2PresentationApply::FPreparedCentralStyleOperation PopoverOperation;
    PopoverOperation.TargetWidget = this;
    PopoverOperation.Payload.Set<GV2PresentationApply::FPreparedRichTextPopoverStyle>(InStyle.PopoverStyle);
    StyleTransaction.AddCentralStyleOperation(MoveTemp(PopoverOperation));
    GV2PresentationApply::FPreparedCentralStyleOperation DescriptionOperation;
    DescriptionOperation.TargetWidget = DescriptionText;
    DescriptionOperation.Payload.Set<GV2PresentationApply::FPreparedRichTextStyle>(InStyle);
    StyleTransaction.AddCentralStyleOperation(MoveTemp(DescriptionOperation));
    FString StyleError;
    if (!GV2PresentationApply::Apply(StyleTransaction, StyleError)
        || !GV2LegacyPresentationApplyAdapter::Apply(StyleTransaction, StyleError))
    {
        return false;
    }

    Model = InModel;
    TitleText->SetVisibility(Model.Title.Text.IsEmpty()
        ? ESlateVisibility::Collapsed
        : ESlateVisibility::SelfHitTestInvisible);
    if (!UGV2TextPipeline::Apply(TitleText, Model.Title))
    {
        return false;
    }
    if (!DescriptionText->ApplyText(Model.Description))
    {
        return false;
    }
    DescriptionText->SetVisibility(Model.Description.Text.IsEmpty()
        ? ESlateVisibility::Collapsed
        : ESlateVisibility::SelfHitTestInvisible);

    if (Icon != nullptr)
    {
        const bool bImageApplied = Model.bHasResolvedImage;
        if (bImageApplied)
        {
            Icon->SetBrush(Model.ResolvedImageBrush);
            Icon->SetDesiredSizeOverride(Model.ResolvedImageBrush.ImageSize);
        }
        Icon->SetVisibility(bImageApplied
            ? ESlateVisibility::SelfHitTestInvisible
            : ESlateVisibility::Collapsed);
    }
    OnPopoverApplied();
    return !Model.IsEmpty();
}

const FGV2RichTextHoverViewModel& UGV2RichTextPopoverWidgetBase::GetPopoverModel() const
{
    return Model;
}

void UGV2RichTextPopoverWidgetBase::ApplyPopoverStyleValues(
    const GV2PresentationApply::FPreparedRichTextPopoverStyle& InStyle)
{
    if (PopoverBorder == nullptr || PopoverWidth == nullptr
        || TitleText == nullptr || DescriptionText == nullptr)
    {
        return;
    }

    // DCA-15 (ADR-0035): the popover's own box follows the same viewport-derived
    // scale as the text it contains -- a fixed max width/height (the Theme
    // default, unscaled) would cap the box at its 1080p footprint even where the
    // text inside is rendering ~60% larger (2160p) or ~15% smaller (720p).
    PopoverBorder->SetBrush(InStyle.Background);
    PopoverBorder->SetPadding(InStyle.Padding);
    const float ViewportScale = GV2PresentationApply::EvaluatePreparedViewportScale(
        InStyle.Scale,
        GV2PresentationApply::ResolveLiveViewportHeight(this, InStyle.Scale.ReferenceViewportHeight));
    PopoverWidth->SetMaxDesiredWidth(InStyle.MaxWidth * ViewportScale);
    PopoverWidth->SetMaxDesiredHeight(InStyle.MaxHeight * ViewportScale);
    if (Icon != nullptr)
    {
        Icon->SetColorAndOpacity(InStyle.ImageTint);
    }
}

void UGV2RichTextPopoverWidgetBase::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    OutBuilder.AddText(TEXT("title"), FName(TEXT("TitleText")));
    OutBuilder.AddText(TEXT("description"), FName(TEXT("DescriptionText")));
    if (Icon != nullptr)
    {
        OutBuilder.AddImage(TEXT("image"), FName(TEXT("Icon")), TEXT("resource"));
    }
    OutBuilder.AddKey(TEXT("key"), NAME_None);
}
