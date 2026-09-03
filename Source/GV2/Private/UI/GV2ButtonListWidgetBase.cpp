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
    // DCA-03: ButtonWidgetClass is read as-is, not resolved through a fallback chain. An
    // unset class still declares the capability; instantiating a NEW entry against it
    // fails downstream (missing_entry_class) instead of silently substituting a default.
    FGV2UiCapabilityTree ButtonCaps;
    if (ButtonWidgetClass != nullptr)
    {
        if (const IGV2UiPropertyHost* HostCDO = Cast<IGV2UiPropertyHost>(ButtonWidgetClass->GetDefaultObject()))
        {
            FGV2UiCapabilityBuilder BtnBuilder;
            HostCDO->DescribeUiCapabilities(BtnBuilder);
            ButtonCaps = BtnBuilder.Build();
        }
    }

    OutBuilder.AddKeyedCollection(
        TEXT("items"),
        FName(TEXT("ButtonContainer")),
        ButtonCaps,
        TEXT("key"),
        ButtonWidgetClass);
    OutBuilder.AddKey(TEXT("key"), NAME_None);
}

UGV2ButtonWidgetBase* UGV2ButtonListWidgetBase::GetButton(const FName ButtonKey) const
{
    if (ButtonContainer == nullptr || ButtonKey.IsNone())
    {
        return nullptr;
    }

    for (UWidget* Child : ButtonContainer->GetAllChildren())
    {
        if (UGV2ButtonWidgetBase* Btn = Cast<UGV2ButtonWidgetBase>(Child))
        {
            if (Btn->GetKey() == ButtonKey)
            {
                return Btn;
            }
        }
    }
    return nullptr;
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

