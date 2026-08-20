#include "Runtime/GV2RuntimeSubsystem.h"

#include "Application/GV2FilesystemContentSourceProvider.h"
#include "Application/GV2RepositoryPublisher.h"
#include "Application/GV2SessionCoordinator.h"
#include "Bridge/GV2StableIdUE.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Logging/LogMacros.h"
#include "Misc/Paths.h"
#include "UI/GV2GameShellWidgetBase.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2RecoveryScreenWidget.h"
#include "UI/GV2ScreenRegistry.h"
#include "UI/GV2ScreenWidgetBase.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

DEFINE_LOG_CATEGORY_STATIC(LogGV2Runtime, Log, All);

namespace
{
bool IsCanonicalScreenId(const FString& Value)
{
    return GV2StableIdUE::IsOfKind(Value, "screen");
}

// TSL-02: default package roots discovered dynamically from GameData container.
TArray<FString> DefaultRepositoryPackageRoots()
{
    const FString GameDataDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData"));
    const std::string GameDataDirUtf8 = TCHAR_TO_UTF8(*GameDataDir);
    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    std::vector<std::filesystem::path> OrderedRoots;
    if (GV2ContentHostSupport::DiscoverPackagesFromContainer(
            std::filesystem::path(GameDataDirUtf8),
            Diagnostics,
            &OrderedRoots))
    {
        TArray<FString> Roots;
        for (const auto& Root : OrderedRoots)
        {
            Roots.Add(UTF8_TO_TCHAR(Root.string().c_str()));
        }
        return Roots;
    }
    return {};
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

    RepositoryPublisher = MakePimpl<FGV2RepositoryPublisher>();
    bRepositoryReady = RepositoryPublisher->PublishCandidate(
        BuildGV2RepositoryFromDirectories(DefaultRepositoryPackageRoots()));
    if (!bRepositoryReady)
    {
        RepositoryBuildError = TEXT("failed to build the initial GameDataRepository");
        UE_LOG(LogGV2Runtime, Error, TEXT("%s"), *RepositoryBuildError);
    }

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
    Coordinator->SetDocumentSink([this](const FGV2UiDocumentViewModel& Document)
    {
        return HandleDocumentRequested(Document);
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
    Reconciler.Reset();
    if (ActiveGameShell != nullptr)
    {
        ActiveGameShell->RemoveFromParent();
        ActiveGameShell = nullptr;
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
        Coordinator->ClearDocumentSink();
        Coordinator.Reset();
    }
    RegisteredScreenClasses.Reset();
    ScreenRegistry = nullptr;
    bImageCatalogReady = false;
    ImageCatalogBuildError.Reset();
    RepositoryPublisher.Reset();
    bRepositoryReady = false;
    RepositoryBuildError.Reset();

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
        Coordinator->FailBootstrap(TEXT("ScreenRegistryNotReady"), TEXT("Screen Registry is not ready"));
        return;
    }
    if (!bImageCatalogReady)
    {
        UE_LOG(
            LogGV2Runtime,
            Error,
            TEXT("StartSession rejected: required Image Resource Catalog is not ready: %s"),
            *ImageCatalogBuildError);
        Coordinator->FailBootstrap(
            TEXT("ImageCatalogNotReady"),
            ImageCatalogBuildError.IsEmpty() ? TEXT("Image Resource Catalog is not ready") : ImageCatalogBuildError);
        return;
    }
    if (!bRepositoryReady || !RepositoryPublisher->HasCurrent())
    {
        UE_LOG(
            LogGV2Runtime,
            Error,
            TEXT("StartSession rejected: GameDataRepository is not ready: %s"),
            *RepositoryBuildError);
        Coordinator->FailBootstrap(
            TEXT("RepositoryNotReady"),
            RepositoryBuildError.IsEmpty() ? TEXT("No published GameDataRepository to pin.") : RepositoryBuildError);
        return;
    }

    if (ActiveScreen != nullptr)
    {
        ActiveScreen->RemoveFromParent();
        ActiveScreen = nullptr;
    }

    if (!Coordinator->StartSession(RepositoryPublisher->GetCurrent(), RepositoryPublisher->GetVersion()))
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("Failed to start GV2 session"));
        if (Coordinator->GetStatus().ApplicationState == EGV2ApplicationState::Failed && GetGameInstance() != nullptr)
        {
            UE_LOG(LogGV2Runtime, Error, TEXT("Showing UE-native recovery surface: session bootstrap failed"));
            UGV2RecoveryScreenWidget* RecoveryScreen = CreateWidget<UGV2RecoveryScreenWidget>(
                GetGameInstance(),
                UGV2RecoveryScreenWidget::StaticClass());
            if (RecoveryScreen != nullptr)
            {
                if (RecoveryScreen->InitializeRecoveryScreen(
                        TEXT("Bootstrap Failed"),
                        TEXT("Session initialization rejected by host lifecycle")))
                {
                    ReplaceActiveScreen(RecoveryScreen);
                }
            }
        }
        return;
    }
    UE_LOG(
        LogGV2Runtime,
        Display,
        TEXT("Started session generation %d with repository version %lld"),
        Coordinator->GetStatus().SessionGeneration,
        Coordinator->GetStatus().RepositoryVersion);
}

void UGV2RuntimeSubsystem::EndSession()
{
    check(IsInGameThread());
    check(Coordinator);
    Reconciler.Reset();
    if (ActiveGameShell != nullptr)
    {
        ActiveGameShell->RemoveFromParent();
        ActiveGameShell = nullptr;
    }
    if (ActiveScreen != nullptr)
    {
        ActiveScreen->RemoveFromParent();
        ActiveScreen = nullptr;
    }
    bActiveScreenAddedToViewport = false;
    Coordinator->EndSession(EGV2SessionState::Destroyed);
}

UUserWidget* UGV2RuntimeSubsystem::GetActiveScreen() const
{
    UGV2ScreenWidgetBase* RouteScreen = Reconciler.GetActiveScreen(UGV2GameShellWidgetBase::LayerLocationContent, FName("main"));
    if (RouteScreen != nullptr)
    {
        return RouteScreen;
    }
    return ActiveScreen;
}

UGV2GameShellWidgetBase* UGV2RuntimeSubsystem::GetActiveGameShell() const
{
    return ActiveGameShell;
}

UGV2ScreenWidgetBase* UGV2RuntimeSubsystem::GetActiveScreenInLayer(FName Layer, FName InstanceKey) const
{
    return Reconciler.GetActiveScreen(Layer, InstanceKey);
}

void UGV2RuntimeSubsystem::SetActiveTab(const FString& ContainerPath, const FString& TabKey)
{
    if (Coordinator.IsValid())
    {
        Coordinator->SetActiveTab(ContainerPath, TabKey);
    }
}

FString UGV2RuntimeSubsystem::GetActiveTab(const FString& ContainerPath) const
{
    if (Coordinator.IsValid())
    {
        return Coordinator->GetActiveTab(ContainerPath);
    }
    return FString();
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
            || !UGV2ScreenRegistry::IsValidLayer(Entry.Layer)
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

UGV2ScreenWidgetBase* UGV2RuntimeSubsystem::InstantiateScreenWidget(const FString& ScreenId)
{
    UClass* ScreenClass = ResolveScreenClass(ScreenId);
    if (ScreenClass == nullptr || GetGameInstance() == nullptr)
    {
        return nullptr;
    }
    return CreateWidget<UGV2ScreenWidgetBase>(GetGameInstance(), ScreenClass);
}

UGV2ScreenWidgetBase* UGV2RuntimeSubsystem::CreateRegisteredScreen(
    const FGV2ScreenViewModel& Model,
    const bool bAddToViewport)
{
    UGV2ScreenWidgetBase* Screen = InstantiateScreenWidget(Model.ScreenId);
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
        bActiveScreenAddedToViewport = true;
        StartSession();
    }
}

bool UGV2RuntimeSubsystem::HandleDocumentRequested(
    const FGV2UiDocumentViewModel& Document)
{
    FString ReconcileError;
    auto ScreenFactory = [this](const FString& ScreenId) -> UGV2ScreenWidgetBase*
    {
        return InstantiateScreenWidget(ScreenId);
    };

    if (!Reconciler.Reconcile(ActiveGameShell, Document, ScreenFactory, ReconcileError))
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("UI Document reconciliation failed: %s"), *ReconcileError);
        return false;
    }

    if (ActiveGameShell == nullptr && Document.bHasRoute)
    {
        ActiveScreen = Reconciler.GetActiveScreen(Document.Route.Layer, Document.Route.InstanceKey);
        if (bActiveScreenAddedToViewport && ActiveScreen != nullptr && ActiveScreen->GetParent() == nullptr && !ActiveScreen->IsInViewport())
        {
            ActiveScreen->AddToViewport();
        }
    }
    return true;
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
