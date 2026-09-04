#include "UI/GV2RichTextPopoverWidgetBase.h"

#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "UI/GV2UiTheme.h"
#include "UI/GV2ImagePresentation.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2UiCapability.h"

void UGV2RichTextPopoverWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyCentralStyle_Implementation();
}

bool UGV2RichTextPopoverWidgetBase::InitializePopover(
    const FGV2RichTextHoverViewModel& InModel)
{
    if (PopoverBorder == nullptr || PopoverWidth == nullptr
        || TitleText == nullptr || DescriptionText == nullptr)
    {
        return false;
    }

    Model = InModel;
    if (!ApplyCentralStyle_Implementation())
    {
        return false;
    }
    TitleText->SetVisibility(Model.Title.Text.IsEmpty()
        ? ESlateVisibility::Collapsed
        : ESlateVisibility::SelfHitTestInvisible);
    DescriptionText->ApplyText(Model.Description);
    DescriptionText->SetVisibility(Model.Description.Text.IsEmpty()
        ? ESlateVisibility::Collapsed
        : ESlateVisibility::SelfHitTestInvisible);

    if (Icon != nullptr)
    {
        const bool bImageApplied = !Model.ImageResourceId.IsEmpty()
            && ApplyImageResource(Model.ImageResourceId);
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

bool UGV2RichTextPopoverWidgetBase::ApplyCentralStyle_Implementation()
{
    UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    if (Theme == nullptr || PopoverBorder == nullptr || PopoverWidth == nullptr
        || TitleText == nullptr || DescriptionText == nullptr)
    {
        return false;
    }

    PopoverBorder->SetBrush(Theme->RichTextPopoverBackground);
    PopoverBorder->SetPadding(Theme->RichTextPopoverPadding);
    // DCA-15 (ADR-0035): the popover's own box follows the same viewport-derived
    // scale as the text it contains -- a fixed max width/height (the Theme
    // default, unscaled) would cap the box at its 1080p footprint even where the
    // text inside is rendering ~60% larger (2160p) or ~15% smaller (720p).
    const float ViewportScale = Theme->EvaluateTextScale(UGV2TextPipeline::GetViewportHeight(this));
    PopoverWidth->SetMaxDesiredWidth(Theme->RichTextPopoverMaxWidth * ViewportScale);
    PopoverWidth->SetMaxDesiredHeight(Theme->RichTextPopoverMaxHeight * ViewportScale);
    if (!UGV2TextPipeline::Apply(TitleText, Model.Title)
        || !IGV2UiStyleConsumer::Execute_ApplyCentralStyle(DescriptionText))
    {
        return false;
    }
    if (Icon != nullptr)
    {
        Icon->SetColorAndOpacity(Theme->ImageTint);
    }
    return true;
}

bool UGV2RichTextPopoverWidgetBase::ApplyImageResource_Implementation(
    const FString& ResourceId)
{
    FGV2ResolvedImageResource Resolved;
    FString Error;
    return FGV2ImagePresentation::ResolveAndApply(
        Icon,
        ResourceId,
        EGV2PrimitiveScalePolicy::PreserveAspect,
        {},
        Resolved,
        Error);
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
