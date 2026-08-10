#include "UI/GV2ButtonListWidgetBase.h"

#include "Blueprint/UserWidget.h"
#include "Components/VerticalBox.h"
#include "UI/GV2ButtonWidgetBase.h"

bool UGV2ButtonListWidgetBase::ApplyButtonModels(const TArray<FGV2ButtonViewModel>& ButtonModels)
{
    if (!CanApplyButtonModels(ButtonModels))
    {
        return false;
    }

    TArray<UGV2ButtonWidgetBase*> NewButtons;
    NewButtons.Reserve(ButtonModels.Num());

    for (const FGV2ButtonViewModel& ButtonModel : ButtonModels)
    {
        UGV2ButtonWidgetBase* Button = nullptr;
        if (APlayerController* OwningPlayer = GetOwningPlayer())
        {
            Button = CreateWidget<UGV2ButtonWidgetBase>(OwningPlayer, ButtonWidgetClass);
        }
        else if (UWorld* World = GetWorld())
        {
            Button = CreateWidget<UGV2ButtonWidgetBase>(World, ButtonWidgetClass);
        }

        if (Button == nullptr)
        {
            return false;
        }

        Button->ApplyButtonModel(ButtonModel);
        Button->OnBindingInvoked.AddDynamic(this, &ThisClass::HandleButtonBindingInvoked);
        NewButtons.Add(Button);
    }

    ButtonContainer->ClearChildren();
    for (UGV2ButtonWidgetBase* Button : NewButtons)
    {
        ButtonContainer->AddChildToVerticalBox(Button);
    }

    return true;
}

bool UGV2ButtonListWidgetBase::CanApplyButtonModels(const TArray<FGV2ButtonViewModel>& ButtonModels) const
{
    if (ButtonContainer == nullptr || ButtonWidgetClass == nullptr)
    {
        return false;
    }

    for (const FGV2ButtonViewModel& ButtonModel : ButtonModels)
    {
        if (!ButtonModel.Binding.IsValid())
        {
            return false;
        }
    }

    return true;
}

void UGV2ButtonListWidgetBase::HandleButtonBindingInvoked(
    const FGV2UiBindingHandle BindingHandle,
    const EGV2SubmitUiInteractionResult Result)
{
    OnBindingInvoked.Broadcast(BindingHandle, Result);
}
