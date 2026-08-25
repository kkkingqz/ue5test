#include "UI/GV2ButtonListWidgetBase.h"

#include "Blueprint/UserWidget.h"
#include "CommonTextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/GV2ButtonWidgetBase.h"
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

