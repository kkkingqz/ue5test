#include "UI/GV2LocationCompositeWidgetBases.h"
#include "UI/GV2TextWidgetBase.h"
#include "UI/GV2PortraitWidgetBase.h"
#include "UI/GV2ProgressBarWidgetBase.h"
#include "UI/GV2ImageWidgetBase.h"
#include "UI/GV2IconWidgetBase.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"

// ============================================================================
// PlayerStatus
// ============================================================================
void UGV2LocationPlayerStatusWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    if (StaminaMeter)
    {
        StaminaMeter->SetVisibility(ESlateVisibility::Collapsed);
    }

    // PCC-09 (proposal 17.2 "Instance wiring check", BAI-11 precedent): internal
    // repeaters are wired once here, at instance construction, instead of lazily from
    // DescribeUiCapabilities -- querying capability must never create objects or mutate
    // the widget. Discarded return values: called for the wiring side effect only.
    ResolveMeterRepeater();
    ResolveItemRepeater();
    ResolveEffectRepeater();
}

bool UGV2LocationPlayerStatusWidgetBase::HasUsableMeterRepeaterHost() const
{
    return MeterRepeater != nullptr || MeterContainer != nullptr;
}

bool UGV2LocationPlayerStatusWidgetBase::HasUsableItemRepeaterHost() const
{
    return ItemRepeater != nullptr || ItemIcons != nullptr;
}

bool UGV2LocationPlayerStatusWidgetBase::HasUsableEffectRepeaterHost() const
{
    return EffectRepeater != nullptr || EffectIcons != nullptr;
}

UGV2ListViewWidgetBase* UGV2LocationPlayerStatusWidgetBase::ResolveItemRepeater()
{
    if (ItemRepeater != nullptr) return ItemRepeater;
    if (ItemIcons == nullptr) return nullptr;
    if (InternalItemRepeater == nullptr)
    {
        InternalItemRepeater = NewObject<UGV2ListViewWidgetBase>(this);
    }
    InternalItemRepeater->SetContainerPanel(ItemIcons);
    return InternalItemRepeater;
}

UGV2ListViewWidgetBase* UGV2LocationPlayerStatusWidgetBase::ResolveEffectRepeater()
{
    if (EffectRepeater != nullptr) return EffectRepeater;
    if (EffectIcons == nullptr) return nullptr;
    if (InternalEffectRepeater == nullptr)
    {
        InternalEffectRepeater = NewObject<UGV2ListViewWidgetBase>(this);
    }
    InternalEffectRepeater->SetContainerPanel(EffectIcons);
    return InternalEffectRepeater;
}

UGV2ListViewWidgetBase* UGV2LocationPlayerStatusWidgetBase::ResolveMeterRepeater()
{
    if (MeterRepeater != nullptr) return MeterRepeater;
    if (MeterContainer == nullptr) return nullptr;
    if (InternalMeterRepeater == nullptr)
    {
        InternalMeterRepeater = NewObject<UGV2ListViewWidgetBase>(this);
    }
    InternalMeterRepeater->SetContainerPanel(MeterContainer);
    return InternalMeterRepeater;
}

TSubclassOf<UGV2ImageWidgetBase> UGV2LocationPlayerStatusWidgetBase::ResolveIconWidgetClass() const
{
    if (IconWidgetClass != nullptr) return IconWidgetClass;
    const UGV2LocationPlayerStatusWidgetBase* CDO = GetClass()->GetDefaultObject<UGV2LocationPlayerStatusWidgetBase>();
    if (CDO != nullptr && CDO != this && CDO->IconWidgetClass != nullptr) return CDO->IconWidgetClass;
    if (UClass* DefaultIconClass = LoadClass<UGV2ImageWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Icon.WBP_Icon_C")))
    {
        return DefaultIconClass;
    }
    return UGV2IconWidgetBase::StaticClass();
}

TSubclassOf<UGV2ProgressBarWidgetBase> UGV2LocationPlayerStatusWidgetBase::ResolveMeterWidgetClass() const
{
    if (MeterWidgetClass != nullptr) return MeterWidgetClass;
    const UGV2LocationPlayerStatusWidgetBase* CDO = GetClass()->GetDefaultObject<UGV2LocationPlayerStatusWidgetBase>();
    if (CDO != nullptr && CDO != this && CDO->MeterWidgetClass != nullptr) return CDO->MeterWidgetClass;
    if (UClass* DefaultProgressClass = LoadClass<UGV2ProgressBarWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_ProgressBar.WBP_ProgressBar_C")))
    {
        return DefaultProgressClass;
    }
    return UGV2ProgressBarWidgetBase::StaticClass();
}

void UGV2LocationPlayerStatusWidgetBase::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    if (PlayerNameText != nullptr)
    {
        OutBuilder.AddText(TEXT("name"), FName(TEXT("PlayerNameText")));
    }
    if (Portrait != nullptr)
    {
        OutBuilder.AddImage(TEXT("portrait_resource_id"), FName(TEXT("Portrait")), TEXT("resource"));
    }
    if (HasUsableMeterRepeaterHost())
    {
        if (TSubclassOf<UGV2ProgressBarWidgetBase> MeterClass = ResolveMeterWidgetClass())
        {
            FGV2UiPropertyCapability MeterCap;
            MeterCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
            MeterCap.EntryWidgetClass = MeterClass;
            OutBuilder.AddKeyedCollection(TEXT("meters"), FName(TEXT("MeterRepeater")), MeterCap, TEXT("key"), MeterClass);
        }
    }
    if (HasUsableItemRepeaterHost())
    {
        if (TSubclassOf<UGV2ImageWidgetBase> IconClass = ResolveIconWidgetClass())
        {
            FGV2UiPropertyCapability IconCap;
            IconCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
            IconCap.EntryWidgetClass = IconClass;
            OutBuilder.AddKeyedCollection(TEXT("items"), FName(TEXT("ItemRepeater")), IconCap, TEXT("key"), IconClass);
        }
    }
    if (HasUsableEffectRepeaterHost())
    {
        if (TSubclassOf<UGV2ImageWidgetBase> IconClass = ResolveIconWidgetClass())
        {
            FGV2UiPropertyCapability IconCap;
            IconCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
            IconCap.EntryWidgetClass = IconClass;
            OutBuilder.AddKeyedCollection(TEXT("effects"), FName(TEXT("EffectRepeater")), IconCap, TEXT("key"), IconClass);
        }
    }
    OutBuilder.AddKey(TEXT("key"), NAME_None);
}

// ============================================================================
// SceneView
// ============================================================================
bool UGV2LocationSceneWidgetBase::HasUsableCharacterRepeaterHost() const
{
    return CharacterRepeater != nullptr || CharacterContainer != nullptr;
}

UGV2ListViewWidgetBase* UGV2LocationSceneWidgetBase::ResolveCharacterRepeater()
{
    if (CharacterRepeater != nullptr) return CharacterRepeater;
    if (CharacterContainer == nullptr) return nullptr;
    if (InternalCharacterRepeater == nullptr)
    {
        InternalCharacterRepeater = NewObject<UGV2ListViewWidgetBase>(this);
    }
    InternalCharacterRepeater->SetContainerPanel(CharacterContainer);
    return InternalCharacterRepeater;
}

TSubclassOf<UGV2ImageWidgetBase> UGV2LocationSceneWidgetBase::ResolveCharacterWidgetClass() const
{
    if (CharacterWidgetClass != nullptr) return CharacterWidgetClass;
    const UGV2LocationSceneWidgetBase* CDO = GetClass()->GetDefaultObject<UGV2LocationSceneWidgetBase>();
    if (CDO != nullptr && CDO != this && CDO->CharacterWidgetClass != nullptr) return CDO->CharacterWidgetClass;
    if (UClass* DefaultIconClass = LoadClass<UGV2ImageWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Icon.WBP_Icon_C")))
    {
        return DefaultIconClass;
    }
    return UGV2IconWidgetBase::StaticClass();
}

void UGV2LocationSceneWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    if (Character)
    {
        Character->SetVisibility(ESlateVisibility::Collapsed);
    }

    // PCC-09: see UGV2LocationPlayerStatusWidgetBase::NativePreConstruct.
    ResolveCharacterRepeater();
}

void UGV2LocationSceneWidgetBase::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    if (BackgroundTile != nullptr)
    {
        OutBuilder.AddImage(TEXT("background_tile_resource_id"), FName(TEXT("BackgroundTile")), TEXT("resource"));
    }
    if (Background != nullptr)
    {
        OutBuilder.AddImage(TEXT("background_resource_id"), FName(TEXT("Background")), TEXT("resource"));
    }
    if (SceneContextText != nullptr)
    {
        OutBuilder.AddText(TEXT("context_text"), FName(TEXT("SceneContextText")));
    }
    if (HasUsableCharacterRepeaterHost())
    {
        if (TSubclassOf<UGV2ImageWidgetBase> CharClass = ResolveCharacterWidgetClass())
        {
            FGV2UiPropertyCapability CharCap;
            CharCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
            CharCap.EntryWidgetClass = CharClass;
            OutBuilder.AddKeyedCollection(TEXT("characters"), FName(TEXT("CharacterRepeater")), CharCap, TEXT("key"), CharClass);
        }
    }
    OutBuilder.AddKey(TEXT("key"), NAME_None);
}

// ============================================================================
// CommandPanel
// ============================================================================
void UGV2LocationCommandPanelWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();

    // PCC-09: see UGV2LocationPlayerStatusWidgetBase::NativePreConstruct.
    ResolveRepeater();
}

UGV2ListViewWidgetBase* UGV2LocationCommandPanelWidgetBase::ResolveRepeater()
{
    if (ButtonRepeater != nullptr) return ButtonRepeater;
    if (ButtonContainer == nullptr) return nullptr;
    if (InternalRepeater == nullptr)
    {
        InternalRepeater = NewObject<UGV2ListViewWidgetBase>(this);
    }
    InternalRepeater->SetContainerPanel(ButtonContainer);
    return InternalRepeater;
}

TSubclassOf<UGV2ButtonWidgetBase> UGV2LocationCommandPanelWidgetBase::ResolveButtonWidgetClass() const
{
    if (ButtonWidgetClass != nullptr) return ButtonWidgetClass;
    const UGV2LocationCommandPanelWidgetBase* CDO = GetClass()->GetDefaultObject<UGV2LocationCommandPanelWidgetBase>();
    if (CDO != nullptr && CDO != this && CDO->ButtonWidgetClass != nullptr) return CDO->ButtonWidgetClass;
    if (UClass* DefaultBtnClass = LoadClass<UGV2ButtonWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Button.WBP_Button_C")))
    {
        return DefaultBtnClass;
    }
    return UGV2ButtonWidgetBase::StaticClass();
}

bool UGV2LocationCommandPanelWidgetBase::HasUsableRepeaterHost() const
{
    return ButtonRepeater != nullptr || ButtonContainer != nullptr;
}

void UGV2LocationCommandPanelWidgetBase::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    if (HasUsableRepeaterHost())
    {
        if (TSubclassOf<UGV2ButtonWidgetBase> BtnClass = ResolveButtonWidgetClass())
        {
            FGV2UiPropertyCapability BtnCap;
            BtnCap.TargetType = EGV2UiCapabilityTargetType::RendererControl;
            BtnCap.EntryWidgetClass = BtnClass;
            OutBuilder.AddKeyedCollection(TEXT("items"), FName(TEXT("ButtonRepeater")), BtnCap, TEXT("key"), BtnClass);
        }
    }
    OutBuilder.AddKey(TEXT("key"), NAME_None);
}
