#include "UI/GV2ButtonListWidgetBase.h"

#include "Blueprint/UserWidget.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2KeyedCollection.h"
#include "UI/GV2UiTheme.h"

DEFINE_LOG_CATEGORY_STATIC(LogGV2ButtonListWidget, Log, All);

void UGV2ButtonListWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyCentralStyle_Implementation();
}

bool UGV2ButtonListWidgetBase::ApplyButtonModels(const TArray<FGV2ButtonViewModel>& ButtonModels)
{
    if (!CanApplyButtonModels(ButtonModels))
    {
        return false;
    }

    const TSubclassOf<UGV2ButtonWidgetBase> ResolvedButtonWidgetClass = ResolveButtonWidgetClass();
    TArray<UGV2ButtonWidgetBase*> OrderedButtons;
    if (!FGV2KeyedCollection::Reconcile<UGV2ButtonWidgetBase, FGV2ButtonViewModel>(
        ButtonContainer,
        ButtonModels,
        ButtonsByKey,
        [](const FGV2ButtonViewModel& Model) { return Model.Key; },
        [this, ResolvedButtonWidgetClass]() -> UGV2ButtonWidgetBase*
        {
            if (GetOwningPlayer() != nullptr)
            {
                return CreateWidget<UGV2ButtonWidgetBase>(GetOwningPlayer(), ResolvedButtonWidgetClass);
            }
            return GetWorld() != nullptr
                ? CreateWidget<UGV2ButtonWidgetBase>(GetWorld(), ResolvedButtonWidgetClass)
                : nullptr;
        },
        [this](UGV2ButtonWidgetBase& Button, const FGV2ButtonViewModel& Model) -> bool
        {
            Button.ApplyButtonModel(Model);
            Button.OnBindingInvoked.AddUniqueDynamic(this, &ThisClass::HandleButtonBindingInvoked);
            return true;
        },
        OrderedButtons))
    {
        return false;
    }
    for (int32 Index = 0; Index < OrderedButtons.Num(); ++Index)
    {
        UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(ButtonContainer->GetSlots()[Index]);
        if (UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme())
        {
            if (Slot != nullptr) Slot->SetPadding(Theme->ButtonListItemPadding);
        }
    }
    AppliedButtonModels = ButtonModels;

    return true;
}

bool UGV2ButtonListWidgetBase::CanApplyButtonModels(const TArray<FGV2ButtonViewModel>& ButtonModels) const
{
    const TSubclassOf<UGV2ButtonWidgetBase> ResolvedButtonWidgetClass = ResolveButtonWidgetClass();
    if (ButtonContainer == nullptr || ResolvedButtonWidgetClass == nullptr)
    {
        UE_LOG(
            LogGV2ButtonListWidget,
            Error,
            TEXT("CanApplyButtonModels rejected: ButtonContainer=%s ButtonWidgetClass=%s"),
            ButtonContainer != nullptr ? TEXT("bound") : TEXT("null"),
            ResolvedButtonWidgetClass != nullptr ? *ResolvedButtonWidgetClass->GetPathName() : TEXT("null"));
        return false;
    }

    TSet<FName> Keys;
    for (const FGV2ButtonViewModel& ButtonModel : ButtonModels)
    {
        if (ButtonModel.Key.IsNone() || Keys.Contains(ButtonModel.Key)
            || !ButtonModel.Binding.IsValid())
        {
            UE_LOG(LogGV2ButtonListWidget, Error, TEXT("CanApplyButtonModels rejected an invalid key or binding"));
            return false;
        }
        Keys.Add(ButtonModel.Key);
    }

    return true;
}

TSubclassOf<UGV2ButtonWidgetBase> UGV2ButtonListWidgetBase::ResolveButtonWidgetClass() const
{
    if (ButtonWidgetClass != nullptr)
    {
        return ButtonWidgetClass;
    }

    const UGV2ButtonListWidgetBase* ClassDefault = GetClass()->GetDefaultObject<UGV2ButtonListWidgetBase>();
    return ClassDefault != nullptr && ClassDefault != this
        ? ClassDefault->ButtonWidgetClass
        : nullptr;
}

void UGV2ButtonListWidgetBase::HandleButtonBindingInvoked(
    const FGV2UiBindingHandle BindingHandle,
    const EGV2SubmitUiInteractionResult Result)
{
    OnBindingInvoked.Broadcast(BindingHandle, Result);
}

FGV2ScreenFieldDescriptor UGV2ButtonListWidgetBase::GetScreenFieldDescriptor_Implementation() const
{
    FGV2ScreenFieldDescriptor Descriptor;
    Descriptor.FieldId = ScreenFieldId;
    Descriptor.SchemaId = TEXT("core:schema.ui_field.button_list.v2");
    Descriptor.bRequired = bScreenFieldRequired;
    return Descriptor;
}

bool UGV2ButtonListWidgetBase::CanApplyScreenField_Implementation(
    const FGV2ScreenFieldValue& FieldValue) const
{
    return FieldValue.FieldId == ScreenFieldId
        && FieldValue.SchemaId == TEXT("core:schema.ui_field.button_list.v2")
        && CanApplyButtonModels(FieldValue.ButtonListValue);
}

bool UGV2ButtonListWidgetBase::ApplyScreenField_Implementation(
    const FGV2ScreenFieldValue& FieldValue)
{
    return CanApplyScreenField_Implementation(FieldValue)
        && ApplyButtonModels(FieldValue.ButtonListValue);
}

bool UGV2ButtonListWidgetBase::CaptureScreenField_Implementation(
    FGV2ScreenFieldValue& OutFieldValue) const
{
    if (ButtonContainer == nullptr || ScreenFieldId.IsNone())
    {
        return false;
    }
    OutFieldValue = FGV2ScreenFieldValue::MakeButtonList(
        ScreenFieldId,
        AppliedButtonModels);
    return true;
}

bool UGV2ButtonListWidgetBase::ResetScreenField_Implementation()
{
    return ApplyButtonModels({});
}

bool UGV2ButtonListWidgetBase::ApplyCentralStyle_Implementation()
{
    UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme();
    if (Theme == nullptr || ButtonContainer == nullptr)
    {
        return false;
    }

    for (UPanelSlot* PanelSlot : ButtonContainer->GetSlots())
    {
        if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(PanelSlot))
        {
            Slot->SetPadding(Theme->ButtonListItemPadding);
        }
    }
    return true;
}
