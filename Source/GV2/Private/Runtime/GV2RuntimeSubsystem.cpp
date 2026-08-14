#include "Runtime/GV2RuntimeSubsystem.h"

#include "Application/GV2SessionCoordinator.h"
#include "Bridge/GV2StableIdUE.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Logging/LogMacros.h"
#include "UI/GV2DebugStartScreenWidget.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2ScreenRegistry.h"
#include "UI/GV2ScreenWidgetBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogGV2Runtime, Log, All);

namespace
{
bool IsCanonicalScreenId(const FString& Value)
{
    return GV2StableIdUE::IsOfKind(Value, "screen");
}
}

UGV2RuntimeSubsystem::UGV2RuntimeSubsystem(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

UGV2RuntimeSubsystem::~UGV2RuntimeSubsystem() = default;

void UGV2RuntimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    ImageCatalogBuildError.Reset();
    bImageCatalogReady = UGV2ImageResourceCatalogSettings::RebuildConfiguredCatalog(
        ImageCatalogBuildError);
    if (!bImageCatalogReady)
    {
        UE_LOG(
            LogGV2Runtime,
            Error,
            TEXT("Image Resource Catalog build failed: %s"),
            *ImageCatalogBuildError);
    }
    LoadScreenRegistry();
    Coordinator = MakePimpl<FGV2SessionCoordinator>();
    Coordinator->SetInteractionSink([this](const FGV2UiIngressItem& Item)
    {
        UE_LOG(
            LogGV2Runtime,
            Display,
            TEXT("Dispatched UI ingress: sequence=%lld binding=%s command=%s"),
            Item.Sequence,
            *Item.BindingHandle.ToString(),
            *Item.Binding.CommandId);

    });
    Coordinator->SetScreenSink([this](const FGV2ScreenViewModel& Model)
    {
        return HandleScreenRequested(Model);
    });
#if !UE_BUILD_SHIPPING
    StartGameInstanceHandle = FWorldDelegates::OnStartGameInstance.AddUObject(
        this,
        &ThisClass::HandleStartGameInstance);
#endif
}

void UGV2RuntimeSubsystem::Deinitialize()
{
    if (StartGameInstanceHandle.IsValid())
    {
        FWorldDelegates::OnStartGameInstance.Remove(StartGameInstanceHandle);
        StartGameInstanceHandle.Reset();
    }
    if (ActiveScreen != nullptr)
    {
        ActiveScreen->RemoveFromParent();
        ActiveScreen = nullptr;
    }
    if (Coordinator)
    {
        Coordinator->EndSession(EGV2SessionState::Destroyed);
        Coordinator->ClearInteractionSink();
        Coordinator->ClearScreenSink();
        Coordinator.Reset();
    }
    RegisteredScreenClasses.Reset();
    ScreenRegistry = nullptr;
    bImageCatalogReady = false;
    ImageCatalogBuildError.Reset();

    Super::Deinitialize();
}

FGV2SessionStatus UGV2RuntimeSubsystem::GetSessionState() const
{
    return Coordinator ? Coordinator->GetStatus() : FGV2SessionStatus();
}

EGV2SubmitUiInteractionResult UGV2RuntimeSubsystem::SubmitUiInteraction(
    const FGV2UiBindingHandle BindingHandle,
    const TArray<FGV2UiControlValue>& InputValues)
{
    return Coordinator
        ? Coordinator->SubmitUiInteraction(BindingHandle, InputValues)
        : EGV2SubmitUiInteractionResult::RuntimeNotReady;
}

void UGV2RuntimeSubsystem::StartSession()
{
    check(IsInGameThread());
    check(Coordinator);

    if (RegisteredScreenClasses.IsEmpty())
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("StartSession rejected: Screen Registry is not ready"));
        return;
    }
    if (!bImageCatalogReady)
    {
        UE_LOG(
            LogGV2Runtime,
            Error,
            TEXT("StartSession rejected: required Image Resource Catalog is not ready: %s"),
            *ImageCatalogBuildError);
        return;
    }

    if (ActiveScreen != nullptr)
    {
        ActiveScreen->RemoveFromParent();
        ActiveScreen = nullptr;
    }

    if (!Coordinator->StartSession())
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("Failed to start GV2 session"));
        return;
    }
    UE_LOG(
        LogGV2Runtime,
        Display,
        TEXT("Started session generation %d"),
        Coordinator->GetStatus().SessionGeneration);
}

void UGV2RuntimeSubsystem::EndSession()
{
    check(IsInGameThread());
    check(Coordinator);
    if (ActiveScreen != nullptr)
    {
        ActiveScreen->RemoveFromParent();
        ActiveScreen = nullptr;
    }
    bActiveScreenAddedToViewport = false;
    Coordinator->EndSession(EGV2SessionState::Destroyed);
}

UGV2DebugStartScreenWidget* UGV2RuntimeSubsystem::ShowDebugStartScreen(
    const bool bAddToViewport)
{
    check(IsInGameThread());
    if (Coordinator == nullptr || GetGameInstance() == nullptr)
    {
        return nullptr;
    }
    if (!Coordinator->GetStatus().bIsReady)
    {
        StartSession();
    }

    FGV2ButtonViewModel StartButtonModel;
    if (!Coordinator->BuildDebugStartButtonModel(StartButtonModel))
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("Failed to build the debug start binding"));
        return nullptr;
    }

    UGV2DebugStartScreenWidget* Screen = CreateWidget<UGV2DebugStartScreenWidget>(
        GetGameInstance(),
        UGV2DebugStartScreenWidget::StaticClass());
    if (Screen == nullptr || !Screen->InitializeStartScreen(StartButtonModel))
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("Failed to create the debug start screen"));
        return nullptr;
    }

    bActiveScreenAddedToViewport = bAddToViewport;
    ReplaceActiveScreen(Screen);
    return Screen;
}

UUserWidget* UGV2RuntimeSubsystem::GetActiveScreen() const
{
    return ActiveScreen;
}

bool UGV2RuntimeSubsystem::LoadScreenRegistry()
{
    RegisteredScreenClasses.Reset();
    const UGV2ScreenRegistrySettings* Settings = GetDefault<UGV2ScreenRegistrySettings>();
    ScreenRegistry = Settings != nullptr ? Settings->RegistryAsset.LoadSynchronous() : nullptr;
    if (ScreenRegistry == nullptr)
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("Screen Registry asset is not configured or could not be loaded"));
        return false;
    }

    for (const FGV2ScreenRegistryEntry& Entry : ScreenRegistry->GetEntries())
    {
        if (!IsCanonicalScreenId(Entry.ScreenId)
            || Entry.Layer.IsNone()
            || Entry.WidgetClass.IsNull()
            || RegisteredScreenClasses.Contains(Entry.ScreenId))
        {
            UE_LOG(
                LogGV2Runtime,
                Error,
                TEXT("Invalid or duplicate Screen Registry entry: screen_id='%s' layer='%s' class='%s'"),
                *Entry.ScreenId,
                *Entry.Layer.ToString(),
                *Entry.WidgetClass.ToSoftObjectPath().ToString());
            RegisteredScreenClasses.Reset();
            ScreenRegistry = nullptr;
            return false;
        }

        UClass* WidgetClass = Entry.WidgetClass.LoadSynchronous();
        if (WidgetClass == nullptr
            || !WidgetClass->IsChildOf(UGV2ScreenWidgetBase::StaticClass())
            || WidgetClass->HasAnyClassFlags(CLASS_Abstract))
        {
            UE_LOG(
                LogGV2Runtime,
                Error,
                TEXT("Screen Registry class is missing, abstract, or has the wrong parent: screen_id='%s' class='%s'"),
                *Entry.ScreenId,
                *Entry.WidgetClass.ToSoftObjectPath().ToString());
            RegisteredScreenClasses.Reset();
            ScreenRegistry = nullptr;
            return false;
        }
        RegisteredScreenClasses.Add(Entry.ScreenId, WidgetClass);
    }

    if (RegisteredScreenClasses.IsEmpty())
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("Screen Registry contains no entries"));
        ScreenRegistry = nullptr;
        return false;
    }
    return true;
}

UClass* UGV2RuntimeSubsystem::ResolveScreenClass(const FString& ScreenId) const
{
    const TObjectPtr<UClass>* Found = RegisteredScreenClasses.Find(ScreenId);
    if (Found == nullptr)
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("Unknown screen_id '%s'"), *ScreenId);
        return nullptr;
    }
    return Found->Get();
}

UGV2ScreenWidgetBase* UGV2RuntimeSubsystem::CreateRegisteredScreen(
    const FGV2ScreenViewModel& Model,
    const bool bAddToViewport)
{
    UClass* ScreenClass = ResolveScreenClass(Model.ScreenId);
    if (ScreenClass == nullptr || GetGameInstance() == nullptr)
    {
        return nullptr;
    }

    UGV2ScreenWidgetBase* Screen = CreateWidget<UGV2ScreenWidgetBase>(GetGameInstance(), ScreenClass);
    if (Screen == nullptr)
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("Failed to instantiate registered screen '%s'"), *Model.ScreenId);
        return nullptr;
    }
    if (!Screen->ApplyScreenFields(Model.Fields))
    {
        UE_LOG(
            LogGV2Runtime,
            Error,
            TEXT("Failed to apply screen '%s' with %d fields"),
            *Model.ScreenId,
            Model.Fields.Num());
        return nullptr;
    }
    if (bAddToViewport)
    {
        Screen->AddToViewport();
    }
    return Screen;
}

void UGV2RuntimeSubsystem::HandleStartGameInstance(UGameInstance* StartedGameInstance)
{
    if (StartedGameInstance != nullptr
        && StartedGameInstance == GetGameInstance()
        && StartedGameInstance->GetWorld() != nullptr
        && StartedGameInstance->GetWorld()->IsGameWorld())
    {
        ShowDebugStartScreen(true);
    }
}

bool UGV2RuntimeSubsystem::HandleScreenRequested(
    const FGV2ScreenViewModel& Model)
{
    UGV2ScreenWidgetBase* Screen = CreateRegisteredScreen(Model, false);
    if (Screen == nullptr)
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("Unable to create requested screen '%s'"), *Model.ScreenId);
        return false;
    }
    ReplaceActiveScreen(Screen);
    return true;
}

void UGV2RuntimeSubsystem::ReplaceActiveScreen(UUserWidget* NewScreen)
{
    if (ActiveScreen != nullptr)
    {
        ActiveScreen->RemoveFromParent();
    }
    ActiveScreen = NewScreen;
    if (bActiveScreenAddedToViewport && ActiveScreen != nullptr)
    {
        ActiveScreen->AddToViewport();
    }
}
