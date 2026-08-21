#include "UI/GV2RichTextPopoverWidgetBase.h"

#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "UI/GV2UiTheme.h"
#include "UI/GV2ImagePresentation.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2TextPipeline.h"

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
    FGV2InteractiveRichTextViewModel DescriptionContent;
    DescriptionContent.Text = Model.Description;
    DescriptionText->ApplyInteractiveRichText(DescriptionContent);
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
    PopoverWidth->SetMaxDesiredWidth(Theme->RichTextPopoverMaxWidth);
    PopoverWidth->SetMaxDesiredHeight(Theme->RichTextPopoverMaxHeight);
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
