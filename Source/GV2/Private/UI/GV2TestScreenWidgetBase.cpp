#include "UI/GV2TestScreenWidgetBase.h"

#include "UI/GV2ButtonListWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"

bool UGV2TestScreenWidgetBase::ApplyScreenModel(const FGV2TestScreenViewModel& ScreenModel)
{
    if (DescriptionText == nullptr
        || ButtonList == nullptr
        || !ButtonList->CanApplyButtonModels(ScreenModel.Buttons))
    {
        return false;
    }

    DescriptionText->ApplyRichTextContent(ScreenModel.DescriptionText);
    return ButtonList->ApplyButtonModels(ScreenModel.Buttons);
}

void UGV2TestScreenWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();
    check(ButtonList != nullptr);
    ButtonList->OnBindingInvoked.AddUniqueDynamic(this, &ThisClass::HandleBindingInvoked);
}

void UGV2TestScreenWidgetBase::NativeDestruct()
{
    if (ButtonList != nullptr)
    {
        ButtonList->OnBindingInvoked.RemoveDynamic(this, &ThisClass::HandleBindingInvoked);
    }

    Super::NativeDestruct();
}

void UGV2TestScreenWidgetBase::HandleBindingInvoked(
    const FGV2UiBindingHandle BindingHandle,
    const EGV2SubmitUiInteractionResult Result)
{
    OnBindingInvoked.Broadcast(BindingHandle, Result);
}
