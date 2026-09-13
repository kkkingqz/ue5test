#include "Runtime/GV2RuntimeSubsystem.h"

#include "Application/GV2FilesystemContentSourceProvider.h"
#include "Application/GV2PackageClosure.h"
#include "Application/GV2RepositoryPublisher.h"
#include "Application/GV2SessionCoordinator.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Logging/LogMacros.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "UI/GV2GameShellWidgetBase.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2LayeredUiReconciler.h"
#include "UI/GV2RecoveryScreenWidget.h"
#include "UI/GV2ScreenRegistry.h"
#include "UI/GV2ScreenWidgetBase.h"
#include "UI/GV2UiTheme.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"
#include "UnrealClient.h"

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

FString ResolveRecoveryText(const FString& TextId, const FString& Fallback)
{
    const UGV2UiTheme* Theme = UGV2UiTheme::GetCoreMinimalTheme();
    const FText* Text = Theme != nullptr ? Theme->TextCatalog.Find(TextId) : nullptr;
    if (Text == nullptr && Theme != nullptr)
    {
        Text = Theme->FallbackTextCatalog.Find(TextId);
    }
    return Text != nullptr ? Text->ToString() : Fallback;
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

    // PSC-14: physical geometry changes are not Lua document publications. Route the
    // engine event into one prepared viewport-refresh transaction over the committed tree.
    ViewportResizedHandle = FViewport::ViewportResizedEvent.AddUObject(
        this,
        &ThisClass::HandleViewportResized);

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
    Coordinator->SetProjectionTeardownSink([this]()
    {
        TeardownActiveProjection();
    });
    Coordinator->SetDocumentSink([this](const FGV2UiDocumentViewModel& Document, const FGV2PresentationPrepareContext& PrepareContext)
    {
        return HandleDocumentRequested(Document, PrepareContext);
    });
    Coordinator->SetProjectionPublishSink([this]()
    {
        PublishActiveProjection();
    });
#if !UE_BUILD_SHIPPING
    StartGameInstanceHandle = FWorldDelegates::OnStartGameInstance.AddUObject(
        this,
        &ThisClass::HandleStartGameInstance);
#endif
}

void UGV2RuntimeSubsystem::Deinitialize()
{
    if (ViewportResizedHandle.IsValid())
    {
        FViewport::ViewportResizedEvent.Remove(ViewportResizedHandle);
        ViewportResizedHandle.Reset();
    }
    if (StartGameInstanceHandle.IsValid())
    {
        FWorldDelegates::OnStartGameInstance.Remove(StartGameInstanceHandle);
        StartGameInstanceHandle.Reset();
    }
    TeardownActiveProjection();
    if (Coordinator)
    {
        Coordinator->EndSession(EGV2SessionState::Destroyed);
        Coordinator->ClearInteractionSink();
        Coordinator->ClearDocumentSink();
        Coordinator->ClearProjectionTeardownSink();
        Coordinator->ClearProjectionPublishSink();
        Coordinator.Reset();
    }
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

int64 UGV2RuntimeSubsystem::RequestSession(const FSessionStartDescriptor& Descriptor)
{
    check(IsInGameThread());
    check(Coordinator);

    FString ValidateError;
    if (!Descriptor.IsValid(&ValidateError))
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("RequestSession rejected descriptor: %s"), *ValidateError);
        Coordinator->FailBootstrap(TEXT("InvalidSessionDescriptor"), ValidateError);
        return 0;
    }

    if (!bRepositoryReady || !RepositoryPublisher->HasCurrent())
    {
        UE_LOG(
            LogGV2Runtime,
            Error,
            TEXT("RequestSession rejected: GameDataRepository is not ready: %s"),
            *RepositoryBuildError);
        Coordinator->FailBootstrap(
            TEXT("RepositoryNotReady"),
            RepositoryBuildError.IsEmpty() ? TEXT("No published GameDataRepository to pin.") : RepositoryBuildError);
        return 0;
    }

    const uint64 OpId = Coordinator->RequestSession(
        Descriptor,
        RepositoryPublisher->GetCurrent(),
        RepositoryPublisher->GetVersion(),
        *ResolvedPackageSet);

    const TOptional<ESessionOperationOutcome> Outcome = Coordinator->GetSessionOperationOutcome(OpId);
    if (Outcome.IsSet() && *Outcome == ESessionOperationOutcome::Failed)
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("Failed to start GV2 session"));
        if (Coordinator->GetStatus().ApplicationState == EGV2ApplicationState::Failed && GetGameInstance() != nullptr)
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("Showing UE-native recovery surface: session bootstrap failed"));
        if (PendingGameShell != nullptr)
        {
            PendingGameShell->RemoveFromParent();
            PendingGameShell = nullptr;
        }
        PendingScreen = nullptr;
        UGV2RecoveryScreenWidget* RecoveryScreen = CreateWidget<UGV2RecoveryScreenWidget>(
            GetGameInstance(),
            UGV2RecoveryScreenWidget::StaticClass());
        if (RecoveryScreen != nullptr)
        {
            const FString TitleText = ResolveRecoveryText(
                TEXT("core:text.screen.recovery.title"),
                TEXT("Recovery"));
            const FString MessageText = ResolveRecoveryText(
                TEXT("core:text.screen.error.description"),
                TEXT("Session initialization rejected by host lifecycle"));

            if (RecoveryScreen->InitializeRecoveryScreen(TitleText, MessageText))
            {
                ReplaceActiveScreen(RecoveryScreen);
            }
        }
    }
    }

    return static_cast<int64>(OpId);
}

ESessionCancellationResult UGV2RuntimeSubsystem::CancelSessionRequest(const int64 OperationId)
{
    check(IsInGameThread());
    return Coordinator ? Coordinator->CancelSessionRequest(static_cast<uint64>(OperationId)) : ESessionCancellationResult::Stale;
}

bool UGV2RuntimeSubsystem::GetSessionOperationOutcome(const int64 OperationId, ESessionOperationOutcome& OutOutcome) const
{
    if (!Coordinator)
    {
        return false;
    }
    const TOptional<ESessionOperationOutcome> Outcome = Coordinator->GetSessionOperationOutcome(static_cast<uint64>(OperationId));
    if (Outcome.IsSet())
    {
        OutOutcome = *Outcome;
        return true;
    }
    return false;
}

TOptional<ESessionOperationOutcome> UGV2RuntimeSubsystem::GetSessionOperationOutcome(const uint64 OperationId) const
{
    return Coordinator ? Coordinator->GetSessionOperationOutcome(OperationId) : TOptional<ESessionOperationOutcome>();
}

void UGV2RuntimeSubsystem::StartSession()
{
    check(IsInGameThread());
    check(Coordinator);

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

    FSessionStartDescriptor Descriptor;
    Descriptor.Mode = ESessionStartMode::NewGame;
    Descriptor.RepositoryVersion = FString::Printf(TEXT("%lld"), RepositoryPublisher->GetVersion());
    Descriptor.RepositoryContentHash = UTF8_TO_TCHAR(RepositoryPublisher->GetCurrent().GetContentHash().c_str());
    Descriptor.SeedHex = FSessionStartDescriptor::GenerateFreshSeedHex();

    const int64 OpId = RequestSession(Descriptor);
    if (OpId > 0)
    {
        ESessionOperationOutcome Outcome;
        if (GetSessionOperationOutcome(OpId, Outcome) && Outcome == ESessionOperationOutcome::Completed)
        {
            UE_LOG(
                LogGV2Runtime,
                Display,
                TEXT("Started session generation %d with repository version %lld"),
                Coordinator->GetStatus().SessionGeneration,
                Coordinator->GetStatus().RepositoryVersion);
        }
    }
}

FString UGV2RuntimeSubsystem::GetActiveSeedHex() const
{
    return Coordinator ? Coordinator->GetActiveSeedHex() : FString();
}

void UGV2RuntimeSubsystem::EndSession()
{
    check(IsInGameThread());
    check(Coordinator);
    bActiveScreenAddedToViewport = false;
    Coordinator->EndSession(EGV2SessionState::Destroyed);
}

void UGV2RuntimeSubsystem::TeardownActiveProjection()
{
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
    if (PendingGameShell != nullptr)
    {
        PendingGameShell->RemoveFromParent();
        PendingGameShell = nullptr;
    }
    PendingScreen = nullptr;
    if (Reconciler.IsValid())
    {
        Reconciler->Reset();
    }
}

void UGV2RuntimeSubsystem::PublishActiveProjection()
{
    if (PendingGameShell != nullptr)
    {
        ActiveGameShell = PendingGameShell;
        PendingGameShell = nullptr;
        if (bActiveScreenAddedToViewport && ActiveGameShell != nullptr && !ActiveGameShell->IsInViewport())
        {
            ActiveGameShell->AddToViewport();
        }
    }
    if (PendingScreen != nullptr)
    {
        ActiveScreen = PendingScreen;
        PendingScreen = nullptr;
        if (bActiveScreenAddedToViewport && ActiveScreen != nullptr && !ActiveScreen->IsInViewport())
        {
            ActiveScreen->AddToViewport();
        }
    }
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

UClass* UGV2RuntimeSubsystem::ResolveScreenClass(
    const FString& ScreenId,
    const FGV2ScreenPlacement& Placement,
    const FGV2PresentationPrepareContext& PrepareContext) const
{
    FGV2ResolvedScreenDescriptor Descriptor;
    FGV2ScreenResolutionRejection Rejection;
    if (!PrepareContext.ResolveScreen(ScreenId, Placement, Descriptor, Rejection))
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("%s"), *Rejection.Message);
        return nullptr;
    }
    return Descriptor.WidgetClass;
}

UGV2ScreenWidgetBase* UGV2RuntimeSubsystem::InstantiateScreenWidget(
    const FString& ScreenId,
    const FGV2ScreenPlacement& Placement,
    const FGV2PresentationPrepareContext& PrepareContext)
{
    UClass* ScreenClass = ResolveScreenClass(ScreenId, Placement, PrepareContext);
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

void UGV2RuntimeSubsystem::HandleViewportResized(FViewport* Viewport, uint32 /*Unused*/)
{
    check(IsInGameThread());
    if (Viewport == nullptr || !Reconciler.IsValid() || GetWorld() == nullptr)
    {
        return;
    }

    UGameViewportClient* GameViewport = GetWorld()->GetGameViewport();
    if (GameViewport == nullptr || GameViewport->Viewport != Viewport)
    {
        // The delegate is process-wide and also reports editor/other PIE viewports. A
        // GameInstance may refresh only the viewport that owns its committed presentation.
        return;
    }

    const float ViewportHeight = static_cast<float>(Viewport->GetSizeXY().Y);
    if (ViewportHeight <= 0.0f)
    {
        return;
    }

    FString RefreshError;
    if (!Reconciler->RefreshViewportPresentation(ViewportHeight, RefreshError))
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("Viewport presentation refresh failed: %s"), *RefreshError);
    }
}

bool UGV2RuntimeSubsystem::HandleDocumentRequested(
    const FGV2UiDocumentViewModel& Document,
    const FGV2PresentationPrepareContext& PrepareContext)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (bTestForceDocumentSinkFailure)
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("UI Document reconciliation failed (forced by automation test)"));
        return false;
    }
#endif
    FString ReconcileError;
    auto ScreenFactory = [this, &PrepareContext](const FString& ScreenId, FName Layer) -> UGV2ScreenWidgetBase*
    {
        return InstantiateScreenWidget(ScreenId, FGV2ScreenPlacement::TopLevel(Layer), PrepareContext);
    };

    UGV2GameShellWidgetBase* TargetShell = ActiveGameShell;
    if (TargetShell == nullptr)
    {
        if (PendingGameShell != nullptr)
        {
            TargetShell = PendingGameShell;
        }
        else
        {
            UClass* GameShellClass = PrepareContext.GetGameShellClass();
            if (GameShellClass != nullptr && GetWorld() != nullptr)
            {
                PendingGameShell = CreateWidget<UGV2GameShellWidgetBase>(GetWorld(), GameShellClass);
                TargetShell = PendingGameShell;
            }
        }
    }

    if (!Reconciler->Reconcile(
            TargetShell,
            Document,
            ScreenFactory,
            ReconcileError,
            PrepareContext))
    {
        UE_LOG(LogGV2Runtime, Error, TEXT("UI Document reconciliation failed: %s"), *ReconcileError);
        return false;
    }

    if (TargetShell == nullptr && Document.bHasRoute)
    {
        UGV2ScreenWidgetBase* ReconciledScreen = Reconciler->GetActiveScreen(Document.Route.Layer, Document.Route.InstanceKey);
        if (ActiveGameShell != nullptr || ActiveScreen != nullptr)
        {
            ActiveScreen = ReconciledScreen;
            if (bActiveScreenAddedToViewport && ActiveScreen != nullptr && ActiveScreen->GetParent() == nullptr && !ActiveScreen->IsInViewport())
            {
                ActiveScreen->AddToViewport();
            }
        }
        else
        {
            PendingScreen = ReconciledScreen;
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

#if WITH_DEV_AUTOMATION_TESTS
bool UGV2RuntimeSubsystem::bTestForceDocumentSinkFailure = false;

const FGV2SessionContentSnapshot* UGV2RuntimeSubsystem::GetContentSnapshotForAutomationTest() const
{
    return Coordinator ? Coordinator->GetContentSnapshot() : nullptr;
}
#endif
