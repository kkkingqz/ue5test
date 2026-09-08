#include "Runtime/GV2RuntimeSubsystem.h"

#include "Application/GV2FilesystemContentSourceProvider.h"
#include "Application/GV2PackageClosure.h"
#include "Application/GV2RepositoryPublisher.h"
#include "Application/GV2SessionCoordinator.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Logging/LogMacros.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "UI/GV2GameShellWidgetBase.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2LayeredUiReconciler.h"
#include "UI/GV2RecoveryScreenWidget.h"
#include "UI/GV2ScreenRegistry.h"
#include "UI/GV2ScreenWidgetBase.h"
#include "UI/GV2TextPipeline.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

DEFINE_LOG_CATEGORY_STATIC(LogGV2Runtime, Log, All);

namespace
{
TOptional<GV2ContentHostSupport::FResolvedPackageSet> ToTOptional(
    std::optional<GV2ContentHostSupport::FResolvedPackageSet> Set)
{
    if (!Set.has_value())
    {
        return {};
    }
    return TOptional<GV2ContentHostSupport::FResolvedPackageSet>(MoveTemp(*Set));
}

// TSL-02/PSC-02 (ADR-0043 D1/D5): the SINGLE package-set resolution point for this
// GameInstance. Repository build, Screen Registry build, and Lua/schema source loading
// (FGV2SessionCoordinator::StartSession, called once per session replacement) all
// consume this ONE result; none of them re-discovers the package set independently
// (PAH-R3: a second, independent discovery of the same closure is a second authority,
// even when it returns the same order today).
// PAH-04: pre_ready_discovery -- only called from Initialize(), before any session exists.
TOptional<GV2ContentHostSupport::FResolvedPackageSet> ResolveSessionPackageSet()
{
    const FString GameDataDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData"));
    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;

#if WITH_DEV_AUTOMATION_TESTS
    if (FGV2SessionCoordinator::bTestForceIncludeSamplePackage)
    {
        // Mirrors the LoadPortableRuntimeSources() override: GameData/sample
        // and GameData/rh cannot coexist (both bind the shared
        // "textsystem:action.location.travel" action), so tests that opt in
        // get core+textsystem+sample instead of the default core+textsystem+rh.
        const std::vector<std::filesystem::path> SampleRoots = {
            std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, TEXT("core")))),
            std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, TEXT("textsystem")))),
            std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, TEXT("sample")))),
        };
        return ToTOptional(GV2ContentHostSupport::ResolvePackageSetFromDirectories(SampleRoots, Diagnostics));
    }
#endif

#if WITH_EDITOR
    const bool bIsInteractiveEditorSession = GIsEditor && !IsRunningCommandlet() && !FApp::IsUnattended();
    if (bIsInteractiveEditorSession)
    {
        const UGV2RuntimeSettings* Settings = GetDefault<UGV2RuntimeSettings>();
        if (Settings != nullptr && !Settings->EditorPackageRoots.IsEmpty())
        {
            std::vector<std::filesystem::path> Roots;
            Roots.reserve(Settings->EditorPackageRoots.Num());
            for (const FString& ConfiguredRoot : Settings->EditorPackageRoots)
            {
                FString Root = ConfiguredRoot;
                if (FPaths::IsRelative(Root))
                {
                    Root = FPaths::Combine(FPaths::ProjectDir(), Root);
                }
                Root = FPaths::ConvertRelativePathToFull(Root);
                FPaths::NormalizeDirectoryName(Root);
                Roots.emplace_back(TCHAR_TO_UTF8(*Root));
            }
            return ToTOptional(GV2ContentHostSupport::ResolvePackageSetFromDirectories(Roots, Diagnostics));
        }
    }
#endif

    return ToTOptional(GV2ContentHostSupport::ResolvePackageSetFromContainer(
        std::filesystem::path(TCHAR_TO_UTF8(*GameDataDir)), Diagnostics));
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

    // PSC-02 (ADR-0043 D1/D5): resolve the package set ONCE, before anything that used
    // to discover it independently -- Screen Registry build now runs AFTER this (it used
    // to run before ANY package resolution at all, PAH-R3), and repository build consumes
    // this same value's descriptors directly instead of re-discovering them.
    ResolvedPackageSet = ResolveSessionPackageSet();

    TArray<GV2PackageClosure::FEntry> ClosureEntries;
    if (ResolvedPackageSet.IsSet())
    {
        ClosureEntries = GV2PackageClosure::FromResolvedPackageSet(*ResolvedPackageSet);
    }
    LoadScreenRegistry(ClosureEntries);

    RepositoryPublisher = MakePimpl<FGV2RepositoryPublisher>();
    bRepositoryReady = ResolvedPackageSet.IsSet()
        && RepositoryPublisher->PublishCandidate(
            BuildGV2RepositoryFromResolvedPackageSet(*ResolvedPackageSet));
    if (!bRepositoryReady)
    {
        RepositoryBuildError = TEXT("failed to build the initial GameDataRepository");
        UE_LOG(LogGV2Runtime, Error, TEXT("%s"), *RepositoryBuildError);
    }

    Reconciler = MakePimpl<FGV2LayeredUiReconciler>();

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
    Reconciler->Reset();
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
        Coordinator->ClearDocumentSink();
        Coordinator.Reset();
    }
    bScreenRegistryReady = false;
    ScreenRegistry = nullptr;
    RepositoryPublisher.Reset();
    ResolvedPackageSet.Reset();
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

    if (!bScreenRegistryReady)
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("StartSession rejected: Screen Registry is not ready"));
        Coordinator->FailBootstrap(TEXT("ScreenRegistryNotReady"), TEXT("Screen Registry is not ready"));
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
    if (ActiveGameShell != nullptr)
    {
        ActiveGameShell->RemoveFromParent();
        ActiveGameShell = nullptr;
    }
    Reconciler->Reset();

    const UGV2ScreenRegistrySettings* RegistrySettings = GetDefault<UGV2ScreenRegistrySettings>();
    UClass* GameShellClass = RegistrySettings != nullptr
        ? RegistrySettings->GameShellClass.LoadSynchronous()
        : nullptr;
    if (GameShellClass != nullptr && GetWorld() != nullptr)
    {
        ActiveGameShell = CreateWidget<UGV2GameShellWidgetBase>(GetWorld(), GameShellClass);
        if (ActiveGameShell != nullptr && bActiveScreenAddedToViewport)
        {
            ActiveGameShell->AddToViewport();
        }
    }

    if (!Coordinator->StartSession(
            RepositoryPublisher->GetCurrent(),
            RepositoryPublisher->GetVersion(),
            ResolvedPackageSet.GetPtrOrNull()))
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
                FGV2TextViewModel ResolvedTitle;
                FGV2TextViewModel ResolvedDesc;
                FString Error;
                const FString TitleText = UGV2TextPipeline::Resolve(TEXT("core:text.screen.recovery.title"), {}, FName("title"), ResolvedTitle, Error)
                    ? ResolvedTitle.Text.ToString()
                    : TEXT("Recovery");
                const FString MessageText = UGV2TextPipeline::Resolve(TEXT("core:text.screen.error.description"), {}, FName("default"), ResolvedDesc, Error)
                    ? ResolvedDesc.Text.ToString()
                    : TEXT("Session initialization rejected by host lifecycle");

                if (RecoveryScreen->InitializeRecoveryScreen(TitleText, MessageText))
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
    Reconciler->Reset();
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
    UGV2ScreenWidgetBase* RouteScreen = Reconciler->GetActiveScreen(UGV2GameShellWidgetBase::LayerLocationContent, FName("main"));
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
    return Reconciler->GetActiveScreen(Layer, InstanceKey);
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

bool UGV2RuntimeSubsystem::LoadScreenRegistry(const TArray<GV2PackageClosure::FEntry>& ClosureEntries)
{
    bScreenRegistryReady = false;
    const UGV2ScreenRegistrySettings* Settings = GetDefault<UGV2ScreenRegistrySettings>();
    ScreenRegistry = Settings != nullptr ? Settings->RegistryAsset.LoadSynchronous() : nullptr;
    if (ScreenRegistry == nullptr)
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("Screen Registry asset is not configured or could not be loaded"));
        return false;
    }

    FString BuildError;
    if (!ScreenRegistry->Build(ClosureEntries, BuildError))
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("Screen Registry failed to build: %s"), *BuildError);
        ScreenRegistry = nullptr;
        return false;
    }

    bScreenRegistryReady = true;
    return true;
}

// PAH-08: phase=prepare -- the screen factory the reconciler calls from
// PrepareReconcile to obtain a candidate widget class; no caller is on the
// application path.
UClass* UGV2RuntimeSubsystem::ResolveScreenClass(const FString& ScreenId, const FGV2ScreenPlacement& Placement) const
{
    if (ScreenRegistry == nullptr)
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("Unknown screen_id '%s': Screen Registry is not ready"), *ScreenId);
        return nullptr;
    }
    FGV2ResolvedScreenDescriptor Descriptor;
    FGV2ScreenResolutionRejection Rejection;
    if (!ScreenRegistry->Resolve(ScreenId, Placement, Descriptor, Rejection))
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("%s"), *Rejection.Message);
        return nullptr;
    }
    return Descriptor.WidgetClass;
}

UGV2ScreenWidgetBase* UGV2RuntimeSubsystem::InstantiateScreenWidget(const FString& ScreenId, const FGV2ScreenPlacement& Placement)
{
    UClass* ScreenClass = ResolveScreenClass(ScreenId, Placement);
    if (ScreenClass == nullptr || GetGameInstance() == nullptr)
    {
        return nullptr;
    }
    return CreateWidget<UGV2ScreenWidgetBase>(GetGameInstance(), ScreenClass);
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
    auto ScreenFactory = [this](const FString& ScreenId, FName Layer) -> UGV2ScreenWidgetBase*
    {
        return InstantiateScreenWidget(ScreenId, FGV2ScreenPlacement::TopLevel(Layer));
    };

    if (!Reconciler->Reconcile(ActiveGameShell, Document, ScreenFactory, ReconcileError))
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("UI Document reconciliation failed: %s"), *ReconcileError);
        return false;
    }

    if (ActiveGameShell == nullptr && Document.bHasRoute)
    {
        ActiveScreen = Reconciler->GetActiveScreen(Document.Route.Layer, Document.Route.InstanceKey);
        if (bActiveScreenAddedToViewport && ActiveScreen != nullptr && ActiveScreen->GetParent() == nullptr && !ActiveScreen->IsInViewport())
        {
            ActiveScreen->AddToViewport();
        }
    }
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
