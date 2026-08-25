#include "UI/GV2ButtonListWidgetBase.h"

#include "Blueprint/UserWidget.h"
#include "CommonTextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2KeyedCollection.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2UiCapability.h"
#include "UI/GV2UiTheme.h"

DEFINE_LOG_CATEGORY_STATIC(LogGV2ButtonListWidget, Log, All);

void UGV2ButtonListWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyCentralStyle_Implementation();
}

void UGV2ButtonListWidgetBase::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    FGV2UiPropertyCapability ButtonCap;
    ButtonCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
    ButtonCap.EntryWidgetClass = ResolveButtonWidgetClass();
    OutBuilder.AddKeyedCollection(
        TEXT("items"),
        FName(TEXT("ButtonContainer")),
        ButtonCap,
        TEXT("key"),
        ResolveButtonWidgetClass());
}

bool UGV2ButtonListWidgetBase::CanApplyButtonModels(const TArray<FGV2ButtonViewModel>& InModels) const
{
    const TSubclassOf<UGV2ButtonWidgetBase> ResolvedClass = ResolveButtonWidgetClass();
    if (ButtonContainer == nullptr || ResolvedClass == nullptr)
    {
        return false;
    }
    TSet<FName> SeenKeys;
    for (const FGV2ButtonViewModel& Model : InModels)
    {
        if (Model.Key.IsNone() || SeenKeys.Contains(Model.Key) || !Model.Binding.IsValid())
        {
            return false;
        }
        SeenKeys.Add(Model.Key);
        if (Model.Text.NormalizedMarkup.Contains(TEXT("<gv2"))
            || UGV2TextPipeline::ResolveStyleClass(Model.Text.StyleToken).Get() == nullptr)
        {
            return false;
        }
    }
    return true;
}

bool UGV2ButtonListWidgetBase::ApplyButtonModels(const TArray<FGV2ButtonViewModel>& InModels)
{
    if (!CanApplyButtonModels(InModels))
    {
        return false;
    }
    const TSubclassOf<UGV2ButtonWidgetBase> ResolvedClass = ResolveButtonWidgetClass();
    TArray<UGV2ButtonWidgetBase*> OrderedWidgets;
    if (!FGV2KeyedCollection::Reconcile<UGV2ButtonWidgetBase, FGV2ButtonViewModel>(
        ButtonContainer,
        InModels,
        ButtonsByKey,
        [](const FGV2ButtonViewModel& Model) { return Model.Key; },
        [this, ResolvedClass]() -> UGV2ButtonWidgetBase*
        {
            if (GetOwningPlayer() != nullptr)
            {
                return CreateWidget<UGV2ButtonWidgetBase>(GetOwningPlayer(), ResolvedClass);
            }
            return GetWorld() != nullptr
                ? CreateWidget<UGV2ButtonWidgetBase>(GetWorld(), ResolvedClass)
                : nullptr;
        },
        [this](UGV2ButtonWidgetBase& Button, const FGV2ButtonViewModel& Model) -> bool
        {
            Button.SetKey(Model.Key);
            Button.SetBindingHandle(Model.Binding);
            if (!Button.ApplyText(Model.Text)) return false;
            Button.OnBindingInvoked.AddUniqueDynamic(
                this, &ThisClass::HandleButtonBindingInvoked);
            return true;
        },
        OrderedWidgets))
    {
        return false;
    }

    if (UGV2UiTheme* Theme = UGV2UiThemeSettings::GetConfiguredTheme())
    {
        for (int32 Index = 0; Index < OrderedWidgets.Num(); ++Index)
        {
            if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(ButtonContainer->GetSlots()[Index]))
            {
                Slot->SetPadding(Theme->ButtonListItemPadding);
            }
        }
    }

    AppliedModels = InModels;
    return true;
}

void UGV2ButtonListWidgetBase::ResetButtonModels()
{
    AppliedModels.Reset();
    ButtonsByKey.Reset();
    if (ButtonContainer != nullptr)
    {
        ButtonContainer->ClearChildren();
    }
}

UGV2ButtonWidgetBase* UGV2ButtonListWidgetBase::GetButton(const FName Key) const
{
    if (ButtonContainer == nullptr || Key.IsNone())
    {
        return nullptr;
    }

    for (UWidget* Child : ButtonContainer->GetAllChildren())
    {
        if (UGV2ButtonWidgetBase* Btn = Cast<UGV2ButtonWidgetBase>(Child))
        {
            if (Btn->GetKey() == Key)
            {
                return Btn;
            }
        }
    }
    return nullptr;
}

TSubclassOf<UGV2ButtonWidgetBase> UGV2ButtonListWidgetBase::ResolveButtonWidgetClass() const
{
    if (ButtonWidgetClass != nullptr)
    {
        return ButtonWidgetClass;
    }

    const UGV2ButtonListWidgetBase* ClassDefault = GetClass()->GetDefaultObject<UGV2ButtonListWidgetBase>();
    if (ClassDefault != nullptr && ClassDefault != this && ClassDefault->ButtonWidgetClass != nullptr)
    {
        return ClassDefault->ButtonWidgetClass;
    }

    if (UClass* Found = FindObject<UClass>(nullptr, TEXT("/Game/UI/Widgets/WBP_Button.WBP_Button_C")))
    {
        return Found;
    }

    if (UClass* Loaded = LoadClass<UGV2ButtonWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Button.WBP_Button_C")))
    {
        return Loaded;
    }

    return UGV2ButtonWidgetBase::StaticClass();
}

void UGV2ButtonListWidgetBase::HandleButtonBindingInvoked(
    const FGV2UiBindingHandle BindingHandle,
    const EGV2SubmitUiInteractionResult Result)
{
    OnBindingInvoked.Broadcast(BindingHandle, Result);
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

