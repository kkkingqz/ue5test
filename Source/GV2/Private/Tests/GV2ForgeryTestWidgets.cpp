#include "Tests/GV2ForgeryTestWidgets.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "UI/GV2UiCapability.h"

EGV2ForgeryMode& UGV2ForgeryEntryTestWidget::ModeForNextInstance()
{
    static EGV2ForgeryMode Mode = EGV2ForgeryMode::NoOpConsumer;
    return Mode;
}

void UGV2ForgeryEntryTestWidget::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    switch (ModeForNextInstance())
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
