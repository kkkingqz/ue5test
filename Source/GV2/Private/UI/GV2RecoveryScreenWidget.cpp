#include "UI/GV2RecoveryScreenWidget.h"

#include "CommonTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

bool UGV2RecoveryScreenWidget::InitializeRecoveryScreen(
    const FString& InTitle,
    const FString& InMessage)
{
    ErrorTitle = InTitle;
    ErrorMessage = InMessage;

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
        CanvasSlot->SetSize(FVector2D(800.0f, 400.0f));
    }

    TitleLabel = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("TitleLabel"));
    if (TitleLabel != nullptr)
    {
        TitleLabel->SetText(FText::FromString(ErrorTitle));
        UVerticalBoxSlot* TitleSlot = Container->AddChildToVerticalBox(TitleLabel);
        if (TitleSlot != nullptr)
        {
            TitleSlot->SetHorizontalAlignment(HAlign_Center);
            TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
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
