#include "UI/GV2RecoveryScreenWidget.h"

#include "CommonTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"
#include "UI/GV2UiTheme.h"

bool UGV2RecoveryScreenWidget::InitializeRecoveryScreen(
    const FString& InTitle,
    const FString& InMessage)
{
    ErrorTitle = InTitle;
    ErrorMessage = InMessage;

    if (WidgetTree == nullptr)
    {
        WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
    }
    if (WidgetTree == nullptr)
    {
        return false;
    }

    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RecoveryCanvas"));
    if (Canvas == nullptr)
    {
        return false;
    }

    UVerticalBox* Container = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RecoveryContainer"));
    if (Container == nullptr)
    {
        return false;
    }
    UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Container);
    if (CanvasSlot != nullptr)
    {
        CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
        CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
        // DCA-15 (ADR-0035): sized to its own text content instead of a fixed
        // 800x400 footprint -- that literal was ~4% of a 3840x2160 viewport and
        // ~87% of the 1280x720 minimum, neither of which "distributes the actual
        // viewport" the way ADR-0035 requires. Content-sizing has no resolution
        // to be wrong on.
        CanvasSlot->SetAutoSize(true);
    }

    // DCA-15 (ADR-0035): the gap between title and message follows the same
    // viewport-derived scale the text itself is styled with (UGV2TextPipeline's
    // text scale curve), rather than a resolution-720p literal.
    const float ViewportScale = [this]() -> float
    {
        const UGV2UiTheme* Theme = UGV2UiTheme::GetCoreMinimalTheme();
        return Theme != nullptr
            ? Theme->EvaluateTextScale(GV2PresentationApply::ResolveLiveViewportHeight(this, Theme->ReferenceViewportHeight))
            : 1.0f;
    }();
    const float TitleToMessageGap = 20.0f * ViewportScale;

    TitleLabel = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("TitleLabel"));
    if (TitleLabel != nullptr)
    {
        TitleLabel->SetText(FText::FromString(ErrorTitle));
        UVerticalBoxSlot* TitleSlot = Container->AddChildToVerticalBox(TitleLabel);
        if (TitleSlot != nullptr)
        {
            TitleSlot->SetHorizontalAlignment(HAlign_Center);
            TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, TitleToMessageGap));
        }
    }

    MessageLabel = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("MessageLabel"));
    if (MessageLabel != nullptr)
    {
        MessageLabel->SetText(FText::FromString(ErrorMessage));
        UVerticalBoxSlot* MessageSlot = Container->AddChildToVerticalBox(MessageLabel);
        if (MessageSlot != nullptr)
        {
            MessageSlot->SetHorizontalAlignment(HAlign_Center);
        }
    }

    WidgetTree->RootWidget = Canvas;
    return true;
}

void UGV2RecoveryScreenWidget::NativeDestruct()
{
    TitleLabel = nullptr;
    MessageLabel = nullptr;
    Super::NativeDestruct();
}
