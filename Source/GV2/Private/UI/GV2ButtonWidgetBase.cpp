#include "UI/GV2ButtonWidgetBase.h"

#include "CommonTextBlock.h"
#include "Engine/GameInstance.h"
#include "Runtime/GV2RuntimeSubsystem.h"

void UGV2ButtonWidgetBase::ApplyButtonModel(const FGV2ButtonViewModel& InButtonModel)
{
    check(LabelText != nullptr);

    ButtonModel = InButtonModel;
    LabelText->SetText(ButtonModel.Text);
    SetIsEnabled(ButtonModel.Binding.IsValid());
}

FGV2ButtonViewModel UGV2ButtonWidgetBase::GetButtonModel() const
{
    return ButtonModel;
}

void UGV2ButtonWidgetBase::NativeOnClicked()
{
    Super::NativeOnClicked();

    EGV2SubmitUiInteractionResult Result = EGV2SubmitUiInteractionResult::RuntimeNotReady;
    if (const UGameInstance* GameInstance = GetGameInstance())
    {
        if (UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>())
        {
            Result = Runtime->SubmitUiInteraction(ButtonModel.Binding, {});
        }
    }

    OnBindingInvoked.Broadcast(ButtonModel.Binding, Result);
}
