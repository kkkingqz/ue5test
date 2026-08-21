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
bool UGV2LocationTopBarWidgetBase::CanApplyScreenField_Implementation(const FGV2ScreenFieldValue& V) const { return V.SchemaId==TEXT("textsystem:schema.ui_field.location_top_bar.v1") && DayText && LocationText && PrimaryResourceText; }
bool UGV2LocationTopBarWidgetBase::CaptureScreenField_Implementation(FGV2ScreenFieldValue& O) const { O=FGV2ScreenFieldValue::MakeLocationTopBar(TEXT("top_bar"),Applied); return true; }
bool UGV2LocationTopBarWidgetBase::ApplyScreenField_Implementation(const FGV2ScreenFieldValue& V) { if(!CanApplyScreenField_Implementation(V)) return false; Applied=V.LocationTopBarValue; return DayText->ApplyText(Applied.Day)&&LocationText->ApplyText(Applied.Location)&&PrimaryResourceText->ApplyText(Applied.PrimaryResource); }
bool UGV2LocationTopBarWidgetBase::ResetScreenField_Implementation() { Applied={}; return true; }

// ============================================================================
// PlayerStatus
// ============================================================================
FGV2ScreenFieldDescriptor UGV2LocationPlayerStatusWidgetBase::GetScreenFieldDescriptor_Implementation() const { return D(TEXT("player_status"), TEXT("textsystem:schema.ui_field.location_player_status.v1")); }
bool UGV2LocationPlayerStatusWidgetBase::CanApplyScreenField_Implementation(const FGV2ScreenFieldValue& V) const { return V.SchemaId==TEXT("textsystem:schema.ui_field.location_player_status.v1") && PlayerNameText; }
bool UGV2LocationPlayerStatusWidgetBase::CaptureScreenField_Implementation(FGV2ScreenFieldValue& O) const { O=FGV2ScreenFieldValue::MakeLocationPlayerStatus(TEXT("player_status"),Applied); return true; }

UGV2ListViewWidgetBase* UGV2LocationPlayerStatusWidgetBase::ResolveItemRepeater()
{
    if (ItemRepeater != nullptr) return ItemRepeater;
    if (InternalItemRepeater == nullptr)
    {
        InternalItemRepeater = NewObject<UGV2ListViewWidgetBase>(this);
    }
    if (ItemIcons != nullptr)
    {
        InternalItemRepeater->SetContainerPanel(ItemIcons);
    }
    return InternalItemRepeater;
}

UGV2ListViewWidgetBase* UGV2LocationPlayerStatusWidgetBase::ResolveEffectRepeater()
{
    if (EffectRepeater != nullptr) return EffectRepeater;
    if (InternalEffectRepeater == nullptr)
    {
        InternalEffectRepeater = NewObject<UGV2ListViewWidgetBase>(this);
    }
    if (EffectIcons != nullptr)
    {
        InternalEffectRepeater->SetContainerPanel(EffectIcons);
    }
    return InternalEffectRepeater;
}

TSubclassOf<UGV2ImageWidgetBase> UGV2LocationPlayerStatusWidgetBase::ResolveIconWidgetClass() const
{
    if (IconWidgetClass != nullptr) return IconWidgetClass;
    return LoadClass<UGV2ImageWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Image.WBP_Image_C"));
}

bool UGV2LocationPlayerStatusWidgetBase::ApplyScreenField_Implementation(const FGV2ScreenFieldValue& V)
{
    if (!CanApplyScreenField_Implementation(V)) return false;
    Applied = V.LocationPlayerStatusValue;
    FString E;
    if (PlayerNameText && !PlayerNameText->ApplyText(Applied.Name)) return false;
    if (Portrait && !Portrait->ApplyOptionalPortrait(Applied.PortraitResourceId, TEXT("textsystem:resource.ui.missing_portrait"), TEXT(""), E)) return false;
    if (StaminaMeter && Applied.Meters.Num() > 0) StaminaMeter->ApplyProgress(Applied.Meters[0].Percent);

    if (UGV2ListViewWidgetBase* ItemRep = ResolveItemRepeater())
    {
        const TSubclassOf<UGV2ImageWidgetBase> IconClass = ResolveIconWidgetClass();
        if (IconClass != nullptr)
        {
            ItemRep->ReconcileEntries<UGV2ImageWidgetBase, FString>(
                Applied.ItemIconResourceIds,
                [](const FString& ResourceId) { return FName(*ResourceId); },
                [this, IconClass]() -> UGV2ImageWidgetBase*
                {
                    return GetOwningPlayer()
                        ? CreateWidget<UGV2ImageWidgetBase>(GetOwningPlayer(), IconClass)
                        : CreateWidget<UGV2ImageWidgetBase>(GetWorld(), IconClass);
                },
                [](UGV2ImageWidgetBase& Icon, const FString& ResourceId) -> bool
                {
                    FString Error;
                    return Icon.ApplyOptionalImageResource(ResourceId, TEXT("textsystem:resource.ui.missing_icon"), Error);
                });
        }
    }

    if (UGV2ListViewWidgetBase* EffectRep = ResolveEffectRepeater())
    {
        const TSubclassOf<UGV2ImageWidgetBase> IconClass = ResolveIconWidgetClass();
        if (IconClass != nullptr)
        {
            EffectRep->ReconcileEntries<UGV2ImageWidgetBase, FString>(
                Applied.EffectIconResourceIds,
                [](const FString& ResourceId) { return FName(*ResourceId); },
                [this, IconClass]() -> UGV2ImageWidgetBase*
                {
                    return GetOwningPlayer()
                        ? CreateWidget<UGV2ImageWidgetBase>(GetOwningPlayer(), IconClass)
                        : CreateWidget<UGV2ImageWidgetBase>(GetWorld(), IconClass);
                },
                [](UGV2ImageWidgetBase& Icon, const FString& ResourceId) -> bool
                {
                    FString Error;
                    return Icon.ApplyOptionalImageResource(ResourceId, TEXT("textsystem:resource.ui.missing_icon"), Error);
                });
        }
    }

    return true;
}

bool UGV2LocationPlayerStatusWidgetBase::ResetScreenField_Implementation()
{
    Applied = {};
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
bool UGV2LocationSceneWidgetBase::CanApplyScreenField_Implementation(const FGV2ScreenFieldValue& V) const { return V.SchemaId==TEXT("textsystem:schema.ui_field.location_scene.v1"); }
bool UGV2LocationSceneWidgetBase::CaptureScreenField_Implementation(FGV2ScreenFieldValue& O) const { O=FGV2ScreenFieldValue::MakeLocationScene(TEXT("scene"),Applied); return true; }

UGV2ListViewWidgetBase* UGV2LocationSceneWidgetBase::ResolveCharacterRepeater()
{
    if (CharacterRepeater != nullptr) return CharacterRepeater;
    if (InternalCharacterRepeater == nullptr)
    {
        InternalCharacterRepeater = NewObject<UGV2ListViewWidgetBase>(this);
    }
    if (CharacterContainer != nullptr)
    {
        InternalCharacterRepeater->SetContainerPanel(CharacterContainer);
    }
    return InternalCharacterRepeater;
}

TSubclassOf<UGV2ImageWidgetBase> UGV2LocationSceneWidgetBase::ResolveCharacterWidgetClass() const
{
    if (CharacterWidgetClass != nullptr) return CharacterWidgetClass;
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
            CharRep->ReconcileEntries<UGV2ImageWidgetBase, FString>(
                Applied.CharacterResourceIds,
                [](const FString& ResourceId) { return FName(*ResourceId); },
                [this, CharClass]() -> UGV2ImageWidgetBase*
                {
                    return GetOwningPlayer()
                        ? CreateWidget<UGV2ImageWidgetBase>(GetOwningPlayer(), CharClass)
                        : CreateWidget<UGV2ImageWidgetBase>(GetWorld(), CharClass);
                },
                [](UGV2ImageWidgetBase& CharWidget, const FString& ResourceId) -> bool
                {
                    FString Error;
                    return CharWidget.ApplyOptionalImageResource(ResourceId, TEXT("textsystem:resource.ui.missing_character"), Error);
                });
        }
    }

    if (Character)
    {
        const FString CharacterResourceId = Applied.CharacterResourceIds.IsEmpty() ? FString() : Applied.CharacterResourceIds[0];
        if (!CharacterResourceId.IsEmpty())
        {
            Character->SetVisibility(ESlateVisibility::Visible);
            if (!Character->ApplyOptionalImageResource(CharacterResourceId, TEXT("textsystem:resource.ui.missing_character"), E)) return false;
        }
        else
        {
            Character->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    return true;
}

bool UGV2LocationSceneWidgetBase::ResetScreenField_Implementation()
{
    Applied = {};
    if (SceneContextText) SceneContextText->SetVisibility(ESlateVisibility::Collapsed);
    if (BackgroundTile) BackgroundTile->SetVisibility(ESlateVisibility::Collapsed);
    if (Background) Background->SetVisibility(ESlateVisibility::Collapsed);
    if (Character) Character->SetVisibility(ESlateVisibility::Collapsed);
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
    if (InternalRepeater == nullptr)
    {
        InternalRepeater = NewObject<UGV2ListViewWidgetBase>(this);
    }
    if (ButtonContainer != nullptr)
    {
        InternalRepeater->SetContainerPanel(ButtonContainer);
    }
    return InternalRepeater;
}

TSubclassOf<UGV2ButtonWidgetBase> UGV2LocationCommandPanelWidgetBase::ResolveButtonWidgetClass() const
{
    if (ButtonWidgetClass != nullptr) return ButtonWidgetClass;
    return LoadClass<UGV2ButtonWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Button.WBP_Button_C"));
}

bool UGV2LocationCommandPanelWidgetBase::CanApplyButtonModels(const TArray<FGV2ButtonViewModel>& Models) const
{
    if (!ResolveButtonWidgetClass()) return false;
    if (ButtonContainer == nullptr && ButtonRepeater == nullptr) return false;
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
    UGV2ListViewWidgetBase* Repeater = ResolveRepeater();
    if (Repeater == nullptr) return false;
    const TSubclassOf<UGV2ButtonWidgetBase> Class = ResolveButtonWidgetClass();

    const bool bSuccess = Repeater->ReconcileEntries<UGV2ButtonWidgetBase, FGV2ButtonViewModel>(
        Models,
        [](const FGV2ButtonViewModel& Model) { return Model.Key; },
        [this, Class]() -> UGV2ButtonWidgetBase*
        {
            return GetOwningPlayer()
                ? CreateWidget<UGV2ButtonWidgetBase>(GetOwningPlayer(), Class)
                : CreateWidget<UGV2ButtonWidgetBase>(GetWorld(), Class);
        },
        [this](UGV2ButtonWidgetBase& Button, const FGV2ButtonViewModel& Model) -> bool
        {
            Button.ApplyButtonModel(Model);
            Button.OnBindingInvoked.AddUniqueDynamic(this, &ThisClass::HandleButtonBindingInvoked);
            return true;
        });

    if (!bSuccess) return false;
    AppliedButtonModels = Models;
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
bool UGV2LocationCommandPanelWidgetBase::ResetScreenField_Implementation() { return ApplyButtonModels({}); }
