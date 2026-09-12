#include "Tests/GV2ForgeryTestWidgets.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "CommonTextBlock.h"
#include "CommonRichTextBlock.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2UiCapability.h"

namespace
{
static EGV2ForgeryMode GForgeryModeForNextInstance = EGV2ForgeryMode::NoOpConsumer;
}

FScopedForgeryMode::FScopedForgeryMode(EGV2ForgeryMode InMode)
    : PreviousMode(UGV2ForgeryEntryTestWidget::GetModeForNextInstance())
{
    UGV2ForgeryEntryTestWidget::SetModeForNextInstance(InMode);
}

FScopedForgeryMode::~FScopedForgeryMode()
{
    UGV2ForgeryEntryTestWidget::SetModeForNextInstance(PreviousMode);
}

EGV2ForgeryMode UGV2ForgeryEntryTestWidget::GetModeForNextInstance()
{
    return GForgeryModeForNextInstance;
}

void UGV2ForgeryEntryTestWidget::SetModeForNextInstance(EGV2ForgeryMode NewMode)
{
    GForgeryModeForNextInstance = NewMode;
}

void UGV2ForgeryEntryTestWidget::PostInitProperties()
{
    Super::PostInitProperties();
    if (!HasAnyFlags(RF_ClassDefaultObject))
    {
        ActiveMode = GetModeForNextInstance();
    }
}

void UGV2ForgeryEntryTestWidget::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    if (!ActiveMode.IsSet())
    {
        ActiveMode = GetModeForNextInstance();
    }

    switch (*ActiveMode)
    {
    case EGV2ForgeryMode::NoOpConsumer:
        OutBuilder.AddBinding(TEXT("forgery_binding"), NAME_None);
        break;

    case EGV2ForgeryMode::DetachedRenderer:
        // No child named this exists anywhere on this widget -- the renderer the
        // capability claims to drive was never wired into the tree.
        OutBuilder.AddText(TEXT("forgery_orphan_text"), FName(TEXT("NonexistentDetachedChild")));
        break;

    case EGV2ForgeryMode::UnimplementableKind:
        // StableId with a TargetKind other than "resource" has no probe pair the harness
        // knows how to synthesize (MakeDistinctValuePair only special-cases "resource") --
        // a capability declared for a kind the harness cannot even attempt.
        OutBuilder.AddCustom(
            TEXT("forgery_unimplementable_ref"),
            EGV2PreparedUiValueKind::StableId,
            EGV2UiCapabilityTargetType::RendererControl,
            NAME_None);
        break;
    }
}

void UGV2NewHostAddedOnlyInTestWidget::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    OutBuilder.AddKey(TEXT("key"), NAME_None);
}

void UGV2SeparatorBoundTestWidget::BuildBoundSubWidgets()
{
    WidgetTree = NewObject<UWidgetTree>(this);
    SeparatorSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SeparatorSizeBox"));
    SeparatorImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("SeparatorImage"));
    SeparatorSizeBox->AddChild(SeparatorImage);
    WidgetTree->RootWidget = SeparatorSizeBox;
}

float UGV2SeparatorBoundTestWidget::ReadAppliedThickness() const
{
    if (SeparatorSizeBox == nullptr)
    {
        return -1.0f;
    }
    return IsHorizontal() ? SeparatorSizeBox->GetHeightOverride() : SeparatorSizeBox->GetWidthOverride();
}

FSlateBrush UGV2SeparatorBoundTestWidget::ReadAppliedBrush() const
{
    return SeparatorImage != nullptr ? SeparatorImage->GetBrush() : FSlateBrush();
}

void UGV2ProgressBarBoundTestWidget::BuildBoundSubWidgets()
{
    WidgetTree = NewObject<UWidgetTree>(this);
    ProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ProgressBar"));
    WidgetTree->RootWidget = ProgressBar;
}

FLinearColor UGV2ProgressBarBoundTestWidget::ReadAppliedFillColor() const
{
    return ProgressBar != nullptr ? ProgressBar->GetFillColorAndOpacity() : FLinearColor::Transparent;
}

void UGV2RichTextBoundTestWidget::BuildBoundSubWidgets()
{
    WidgetTree = NewObject<UWidgetTree>(this);
    RichTextBlock = WidgetTree->ConstructWidget<UCommonRichTextBlock>(
        UCommonRichTextBlock::StaticClass(), TEXT("RichTextBlock"));
    WidgetTree->RootWidget = RichTextBlock;
}

void UGV2RichTextPopoverBoundTestWidget::BuildBoundSubWidgets()
{
    WidgetTree = NewObject<UWidgetTree>(this);
    PopoverBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PopoverBorder"));
    PopoverWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PopoverWidth"));
    TitleText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("TitleText"));
    UGV2RichTextBoundTestWidget* BoundDescription = WidgetTree->ConstructWidget<UGV2RichTextBoundTestWidget>(
        UGV2RichTextBoundTestWidget::StaticClass(), TEXT("DescriptionText"));
    BoundDescription->BuildBoundSubWidgets();
    DescriptionText = BoundDescription;
    Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Icon"));
    PopoverBorder->AddChild(PopoverWidth);
    WidgetTree->RootWidget = PopoverBorder;
}

FSlateBrush UGV2RichTextPopoverBoundTestWidget::ReadAppliedBackground() const
{
    return PopoverBorder != nullptr ? PopoverBorder->Background : FSlateBrush();
}

FMargin UGV2RichTextPopoverBoundTestWidget::ReadAppliedPadding() const
{
    return PopoverBorder != nullptr ? PopoverBorder->GetPadding() : FMargin();
}

float UGV2RichTextPopoverBoundTestWidget::ReadAppliedMaxWidth() const
{
    return PopoverWidth != nullptr ? PopoverWidth->GetMaxDesiredWidth() : -1.0f;
}
