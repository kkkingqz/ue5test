#include "UI/GV2DebugStartScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2UiInteractionEmitter.h"

bool UGV2DebugStartScreenWidget::InitializeStartScreen(
    const FGV2ButtonViewModel& InStartButtonModel)
{
    if (WidgetTree == nullptr || !InStartButtonModel.Binding.IsValid())
    {
        return false;
    }

    StartButtonModel = InStartButtonModel;
    StartButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StartButton"));
    StartLabel = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("StartLabel"));
    if (StartButton == nullptr || StartLabel == nullptr)
    {
        return false;
    }

    if (!UGV2TextPipeline::Apply(StartLabel, StartButtonModel.Text))
    {
        return false;
    }
    StartButton->SetContent(StartLabel);
    StartButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStartClicked);
    WidgetTree->RootWidget = StartButton;
    return true;
}

void UGV2DebugStartScreenWidget::NativeDestruct()
{
    if (StartButton != nullptr)
    {
        StartButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleStartClicked);
    }
    Super::NativeDestruct();
}

void UGV2DebugStartScreenWidget::HandleStartClicked()
{
    SubmitStart();
}

EGV2SubmitUiInteractionResult UGV2DebugStartScreenWidget::SubmitStart()
{
    return FGV2UiInteractionEmitter::Submit(this, StartButtonModel.Binding, {});
}
