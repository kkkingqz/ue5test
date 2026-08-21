#include "UI/GV2LocationCompositeWidgetBases.h"
#include "UI/GV2TextWidgetBase.h"
#include "UI/GV2PortraitWidgetBase.h"
#include "UI/GV2ProgressBarWidgetBase.h"
#include "UI/GV2ImageWidgetBase.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"

namespace { FGV2ScreenFieldDescriptor D(const TCHAR* Id, const TCHAR* Schema) { FGV2ScreenFieldDescriptor R; R.FieldId=FName(Id); R.SchemaId=Schema; R.bRequired=true; return R; } }

// ============================================================================
// TopBar
// ============================================================================
FGV2ScreenFieldDescriptor UGV2LocationTopBarWidgetBase::GetScreenFieldDescriptor_Implementation() const { return D(TEXT("top_bar"), TEXT("textsystem:schema.ui_field.location_top_bar.v1")); }
bool UGV2LocationTopBarWidgetBase::CanApplyScreenField_Implementation(const FGV2ScreenFieldValue& V) const
{
    return V.FieldId == TEXT("top_bar")
        && V.SchemaId == TEXT("textsystem:schema.ui_field.location_top_bar.v1");
}
bool UGV2LocationTopBarWidgetBase::CaptureScreenField_Implementation(FGV2ScreenFieldValue& O) const { O=FGV2ScreenFieldValue::MakeLocationTopBar(TEXT("top_bar"),Applied); return true; }
bool UGV2LocationTopBarWidgetBase::ApplyScreenField_Implementation(const FGV2ScreenFieldValue& V)
{
    if (!CanApplyScreenField_Implementation(V)) return false;
    Applied = V.LocationTopBarValue;
    if (DayText && !DayText->ApplyText(Applied.Day)) return false;
    if (LocationText && !LocationText->ApplyText(Applied.Location)) return false;
    if (PrimaryResourceText && !PrimaryResourceText->ApplyText(Applied.PrimaryResource)) return false;
    return true;
}
bool UGV2LocationTopBarWidgetBase::ResetScreenField_Implementation()
{
    Applied = {};
    if (DayText) DayText->ApplyText({});
    if (LocationText) LocationText->ApplyText({});
    if (PrimaryResourceText) PrimaryResourceText->ApplyText({});
    return true;
}

// ============================================================================
// PlayerStatus
// ============================================================================
FGV2ScreenFieldDescriptor UGV2LocationPlayerStatusWidgetBase::GetScreenFieldDescriptor_Implementation() const { return D(TEXT("player_status"), TEXT("textsystem:schema.ui_field.location_player_status.v1")); }

bool UGV2LocationPlayerStatusWidgetBase::CanApplyScreenField_Implementation(const FGV2ScreenFieldValue& V) const
{
    if (V.FieldId != TEXT("player_status")
        || V.SchemaId != TEXT("textsystem:schema.ui_field.location_player_status.v1"))
    {
        return false;
    }

    const FGV2LocationPlayerStatusViewModel& Model = V.LocationPlayerStatusValue;

    // Validate meter keys are non-empty and unique
    TSet<FName> MeterKeys;
    for (const FGV2LocationMeterEntry& Meter : Model.Meters)
    {
        if (Meter.Key.IsNone() || MeterKeys.Contains(Meter.Key)) return false;
        MeterKeys.Add(Meter.Key);
    }

    return true;
}

bool UGV2LocationPlayerStatusWidgetBase::CaptureScreenField_Implementation(FGV2ScreenFieldValue& O) const { O=FGV2ScreenFieldValue::MakeLocationPlayerStatus(TEXT("player_status"),Applied); return true; }

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
    if (UClass* Found = FindObject<UClass>(nullptr, TEXT("/Game/UI/Widgets/WBP_Image.WBP_Image_C")))
    {
        return Found;
    }
    return LoadClass<UGV2ImageWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Image.WBP_Image_C"));
}

TSubclassOf<UGV2ProgressBarWidgetBase> UGV2LocationPlayerStatusWidgetBase::ResolveMeterWidgetClass() const
{
    if (MeterWidgetClass != nullptr) return MeterWidgetClass;
    if (UClass* Found = FindObject<UClass>(nullptr, TEXT("/Game/UI/Widgets/WBP_ProgressBar.WBP_ProgressBar_C")))
    {
        return Found;
    }
    return LoadClass<UGV2ProgressBarWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_ProgressBar.WBP_ProgressBar_C"));
}

bool UGV2LocationPlayerStatusWidgetBase::ApplyScreenField_Implementation(const FGV2ScreenFieldValue& V)
{
    if (!CanApplyScreenField_Implementation(V)) return false;
    Applied = V.LocationPlayerStatusValue;
    FString E;
    if (PlayerNameText && !PlayerNameText->ApplyText(Applied.Name)) return false;
    if (Portrait)
    {
        if (!Applied.PortraitResourceId.IsEmpty())
        {
            Portrait->SetVisibility(ESlateVisibility::Visible);
            if (!Portrait->ApplyOptionalPortrait(Applied.PortraitResourceId, TEXT("textsystem:resource.ui.missing_portrait"), TEXT(""), E)) return false;
        }
        else
        {
            Portrait->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    if (UGV2ListViewWidgetBase* MeterRep = ResolveMeterRepeater())
    {
        const TSubclassOf<UGV2ProgressBarWidgetBase> Class = ResolveMeterWidgetClass();
        if (Class != nullptr)
        {
            const bool bMetersOk = MeterRep->ReconcileEntries<UGV2ProgressBarWidgetBase, FGV2LocationMeterEntry>(
                Applied.Meters,
                [](const FGV2LocationMeterEntry& Entry) { return Entry.Key; },
                [this, Class]() -> UGV2ProgressBarWidgetBase*
                {
                    return GetOwningPlayer()
                        ? CreateWidget<UGV2ProgressBarWidgetBase>(GetOwningPlayer(), Class)
                        : (GetWorld() ? CreateWidget<UGV2ProgressBarWidgetBase>(GetWorld(), Class) : NewObject<UGV2ProgressBarWidgetBase>(GetTransientPackage(), Class));
                },
                [](UGV2ProgressBarWidgetBase& MeterWidget, const FGV2LocationMeterEntry& Entry) -> bool
                {
                    MeterWidget.ApplyProgress(Entry.Meter.Percent);
                    return true;
                });
            if (!bMetersOk) return false;
        }
    }
    else if (StaminaMeter)
    {
        if (Applied.Meters.Num() > 0)
        {
            StaminaMeter->SetVisibility(ESlateVisibility::Visible);
            StaminaMeter->ApplyProgress(Applied.Meters[0].Meter.Percent);
        }
        else
        {
            StaminaMeter->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    if (UGV2ListViewWidgetBase* ItemRep = ResolveItemRepeater())
    {
        const TSubclassOf<UGV2ImageWidgetBase> IconClass = ResolveIconWidgetClass();
        if (IconClass != nullptr)
        {
            const bool bItemsOk = ItemRep->ReconcileEntries<UGV2ImageWidgetBase, FString>(
                Applied.ItemIconResourceIds,
                [](const FString& ResourceId) { return FName(*ResourceId); },
                [this, IconClass]() -> UGV2ImageWidgetBase*
                {
                    return GetOwningPlayer()
                        ? CreateWidget<UGV2ImageWidgetBase>(GetOwningPlayer(), IconClass)
                        : (GetWorld() ? CreateWidget<UGV2ImageWidgetBase>(GetWorld(), IconClass) : NewObject<UGV2ImageWidgetBase>(GetTransientPackage(), IconClass));
                },
                [](UGV2ImageWidgetBase& Icon, const FString& ResourceId) -> bool
                {
                    FString Error;
                    return Icon.ApplyOptionalImageResource(ResourceId, TEXT("textsystem:resource.ui.missing_icon"), Error);
                });
            if (!bItemsOk) return false;
        }
    }

    if (UGV2ListViewWidgetBase* EffectRep = ResolveEffectRepeater())
    {
        const TSubclassOf<UGV2ImageWidgetBase> IconClass = ResolveIconWidgetClass();
        if (IconClass != nullptr)
        {
            const bool bEffectsOk = EffectRep->ReconcileEntries<UGV2ImageWidgetBase, FString>(
                Applied.EffectIconResourceIds,
                [](const FString& ResourceId) { return FName(*ResourceId); },
                [this, IconClass]() -> UGV2ImageWidgetBase*
                {
                    return GetOwningPlayer()
                        ? CreateWidget<UGV2ImageWidgetBase>(GetOwningPlayer(), IconClass)
                        : (GetWorld() ? CreateWidget<UGV2ImageWidgetBase>(GetWorld(), IconClass) : NewObject<UGV2ImageWidgetBase>(GetTransientPackage(), IconClass));
                },
                [](UGV2ImageWidgetBase& Icon, const FString& ResourceId) -> bool
                {
                    FString Error;
                    return Icon.ApplyOptionalImageResource(ResourceId, TEXT("textsystem:resource.ui.missing_icon"), Error);
                });
            if (!bEffectsOk) return false;
        }
    }

    return true;
}

bool UGV2LocationPlayerStatusWidgetBase::ResetScreenField_Implementation()
{
    Applied = {};
    if (PlayerNameText)
    {
        PlayerNameText->ApplyText({});
    }
    if (Portrait)
    {
        Portrait->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (StaminaMeter)
    {
        StaminaMeter->ApplyProgress(0.0f);
        StaminaMeter->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (UGV2ListViewWidgetBase* MeterRep = ResolveMeterRepeater())
    {
        MeterRep->ClearEntries();
    }
    if (UGV2ListViewWidgetBase* ItemRep = ResolveItemRepeater())
    {
        ItemRep->ClearEntries();
    }
    if (UGV2ListViewWidgetBase* EffectRep = ResolveEffectRepeater())
    {
        EffectRep->ClearEntries();
    }
    return true;
}

// ============================================================================
// SceneView
// ============================================================================
FGV2ScreenFieldDescriptor UGV2LocationSceneWidgetBase::GetScreenFieldDescriptor_Implementation() const { return D(TEXT("scene"), TEXT("textsystem:schema.ui_field.location_scene.v1")); }

bool UGV2LocationSceneWidgetBase::CanApplyScreenField_Implementation(const FGV2ScreenFieldValue& V) const
{
    if (V.FieldId != TEXT("scene") || V.SchemaId != TEXT("textsystem:schema.ui_field.location_scene.v1"))
    {
        return false;
    }

    const FGV2LocationSceneViewModel& Model = V.LocationSceneValue;
    TSet<FName> CharacterKeys;
    for (const FGV2LocationCharacterEntry& Entry : Model.Characters)
    {
        if (Entry.Key.IsNone() || CharacterKeys.Contains(Entry.Key)) return false;
        CharacterKeys.Add(Entry.Key);
    }

    return true;
}

bool UGV2LocationSceneWidgetBase::CaptureScreenField_Implementation(FGV2ScreenFieldValue& O) const { O=FGV2ScreenFieldValue::MakeLocationScene(TEXT("scene"),Applied); return true; }

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
    if (UClass* Found = FindObject<UClass>(nullptr, TEXT("/Game/UI/Widgets/WBP_Image.WBP_Image_C")))
    {
        return Found;
    }
    return LoadClass<UGV2ImageWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Image.WBP_Image_C"));
}

bool UGV2LocationSceneWidgetBase::ApplyScreenField_Implementation(const FGV2ScreenFieldValue& V)
{
    if (!CanApplyScreenField_Implementation(V)) return false;
    Applied = V.LocationSceneValue;
    FString E;
    if (SceneContextText)
    {
        if (!Applied.ContextText.Text.IsEmpty())
        {
            SceneContextText->SetVisibility(ESlateVisibility::Visible);
            if (!SceneContextText->ApplyText(Applied.ContextText)) return false;
        }
        else
        {
            SceneContextText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    if (BackgroundTile)
    {
        if (!Applied.BackgroundTileResourceId.IsEmpty())
        {
            BackgroundTile->SetVisibility(ESlateVisibility::Visible);
            if (!BackgroundTile->ApplyImageResource(Applied.BackgroundTileResourceId, E)) return false;
        }
        else
        {
            BackgroundTile->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    if (Background)
    {
        if (!Applied.BackgroundResourceId.IsEmpty())
        {
            Background->SetVisibility(ESlateVisibility::Visible);
            if (!Background->ApplyOptionalImageResource(Applied.BackgroundResourceId, TEXT("textsystem:resource.ui.missing_background"), E)) return false;
        }
        else
        {
            Background->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    if (UGV2ListViewWidgetBase* CharRep = ResolveCharacterRepeater())
    {
        const TSubclassOf<UGV2ImageWidgetBase> CharClass = ResolveCharacterWidgetClass();
        if (CharClass != nullptr)
        {
            const bool bCharsOk = CharRep->ReconcileEntries<UGV2ImageWidgetBase, FGV2LocationCharacterEntry>(
                Applied.Characters,
                [](const FGV2LocationCharacterEntry& Entry) { return Entry.Key; },
                [this, CharClass]() -> UGV2ImageWidgetBase*
                {
                    return GetOwningPlayer()
                        ? CreateWidget<UGV2ImageWidgetBase>(GetOwningPlayer(), CharClass)
                        : (GetWorld() ? CreateWidget<UGV2ImageWidgetBase>(GetWorld(), CharClass) : NewObject<UGV2ImageWidgetBase>(GetTransientPackage(), CharClass));
                },
                [](UGV2ImageWidgetBase& CharWidget, const FGV2LocationCharacterEntry& Entry) -> bool
                {
                    FString Error;
                    return CharWidget.ApplyOptionalImageResource(Entry.ResourceId, TEXT("textsystem:resource.ui.missing_character"), Error);
                });
            if (!bCharsOk) return false;
        }
    }

    return true;
}

bool UGV2LocationSceneWidgetBase::ResetScreenField_Implementation()
{
    Applied = {};
    if (SceneContextText)
    {
        SceneContextText->ApplyText({});
        SceneContextText->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (BackgroundTile) BackgroundTile->SetVisibility(ESlateVisibility::Collapsed);
    if (Background) Background->SetVisibility(ESlateVisibility::Collapsed);
    if (UGV2ListViewWidgetBase* CharRep = ResolveCharacterRepeater())
    {
        CharRep->ClearEntries();
    }
    return true;
}

// ============================================================================
// CommandPanel
// ============================================================================
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
    if (UClass* Found = FindObject<UClass>(nullptr, TEXT("/Game/UI/Widgets/WBP_Button.WBP_Button_C")))
    {
        return Found;
    }
    return LoadClass<UGV2ButtonWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Button.WBP_Button_C"));
}

bool UGV2LocationCommandPanelWidgetBase::CanApplyButtonModels(const TArray<FGV2ButtonViewModel>& Models) const
{
    TSet<FName> Keys;
    for (const FGV2ButtonViewModel& Model : Models)
    {
        if (Model.Key.IsNone() || Keys.Contains(Model.Key) || !Model.Binding.IsValid()) return false;
        Keys.Add(Model.Key);
    }
    return true;
}

bool UGV2LocationCommandPanelWidgetBase::ApplyButtonModels(const TArray<FGV2ButtonViewModel>& Models)
{
    if (!CanApplyButtonModels(Models)) return false;
    AppliedButtonModels = Models;
    UGV2ListViewWidgetBase* Repeater = ResolveRepeater();
    if (Repeater == nullptr) return true;
    const TSubclassOf<UGV2ButtonWidgetBase> Class = ResolveButtonWidgetClass();

    const bool bSuccess = Repeater->ReconcileEntries<UGV2ButtonWidgetBase, FGV2ButtonViewModel>(
        Models,
        [](const FGV2ButtonViewModel& Model) { return Model.Key; },
        [this, Class]() -> UGV2ButtonWidgetBase*
        {
            if (Class == nullptr)
            {
                return NewObject<UGV2ButtonWidgetBase>(this);
            }
            return GetOwningPlayer()
                ? CreateWidget<UGV2ButtonWidgetBase>(GetOwningPlayer(), Class)
                : (GetWorld() ? CreateWidget<UGV2ButtonWidgetBase>(GetWorld(), Class) : NewObject<UGV2ButtonWidgetBase>(GetTransientPackage(), Class));
        },
        [this](UGV2ButtonWidgetBase& Button, const FGV2ButtonViewModel& Model) -> bool
        {
            Button.ApplyButtonModel(Model);
            Button.OnBindingInvoked.AddUniqueDynamic(this, &ThisClass::HandleButtonBindingInvoked);
            return true;
        });

    if (!bSuccess) return false;
    return true;
}

void UGV2LocationCommandPanelWidgetBase::HandleButtonBindingInvoked(FGV2UiBindingHandle BindingHandle, EGV2SubmitUiInteractionResult Result)
{
    OnBindingInvoked.Broadcast(BindingHandle, Result);
}

FGV2ScreenFieldDescriptor UGV2LocationCommandPanelWidgetBase::GetScreenFieldDescriptor_Implementation() const { return D(TEXT("commands"), TEXT("textsystem:schema.ui_field.location_commands.v1")); }
bool UGV2LocationCommandPanelWidgetBase::CanApplyScreenField_Implementation(const FGV2ScreenFieldValue& V) const { return V.FieldId == TEXT("commands") && V.SchemaId == TEXT("textsystem:schema.ui_field.location_commands.v1") && CanApplyButtonModels(V.ButtonListValue); }
bool UGV2LocationCommandPanelWidgetBase::ApplyScreenField_Implementation(const FGV2ScreenFieldValue& V) { return CanApplyScreenField_Implementation(V) && ApplyButtonModels(V.ButtonListValue); }
bool UGV2LocationCommandPanelWidgetBase::CaptureScreenField_Implementation(FGV2ScreenFieldValue& O) const { O = FGV2ScreenFieldValue::MakeLocationCommands(TEXT("commands"), AppliedButtonModels); return true; }
bool UGV2LocationCommandPanelWidgetBase::ResetScreenField_Implementation()
{
    AppliedButtonModels.Reset();
    if (UGV2ListViewWidgetBase* Repeater = ResolveRepeater())
    {
        Repeater->ClearEntries();
    }
    return true;
}
