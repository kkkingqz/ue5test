#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "GV2PresentationApply/GV2PresentationInteractionSink.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GV2RuntimeCore/GV2HostServices.h"
#include <memory>
#include "GV2RuntimeSubsystem.generated.h"

namespace GV2PackageClosure { struct FEntry; }

class FGV2LayeredUiReconciler;
class FGV2PresentationPrepareContext;
class FGV2RepositoryPublisher;
class FGV2ScreenPlacement;
class FGV2SessionContentSnapshot;
class FGV2SessionCoordinator;
class FViewport;
class UGV2GameShellWidgetBase;
class UGV2ScreenWidgetBase;
class UUserWidget;

UCLASS(Config = Game, DefaultConfig)
class GV2_API UGV2RuntimeSettings : public UObject
{
    GENERATED_BODY()

public:
    // Interactive Editor sessions may use a content profile different from
    // the canonical mods.lock package set. Relative paths are resolved from
    // the project directory. Commandlets, automation and Shipping ignore it.
    UPROPERTY(Config, EditAnywhere, Category = "GV2|Runtime|Development")
    TArray<FString> EditorPackageRoots;
};

UCLASS()
class GV2_API UGV2RuntimeSubsystem : public UGV2PresentationInteractionSink
{
    GENERATED_BODY()

public:
    explicit UGV2RuntimeSubsystem(const FObjectInitializer& ObjectInitializer);
    virtual ~UGV2RuntimeSubsystem() override;

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintPure, Category = "GV2|Runtime")
    FGV2SessionStatus GetSessionState() const;

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    EGV2SubmitUiInteractionResult SubmitUiInteraction(
        FGV2UiBindingHandle BindingHandle,
        const TArray<FGV2UiControlValue>& InputValues);

    virtual EGV2SubmitUiInteractionResult SubmitPresentationInteraction(
        FGV2UiBindingHandle BindingHandle,
        const TArray<FGV2UiControlValue>& InputValues) override
    {
        return SubmitUiInteraction(MoveTemp(BindingHandle), InputValues);
    }

    UFUNCTION(BlueprintCallable, Category = "GV2|Runtime")
    int64 RequestSession(const FSessionStartDescriptor& Descriptor);

    UFUNCTION(BlueprintCallable, Category = "GV2|Runtime")
    int64 RequestSave(const FString& SlotId);

    UFUNCTION(BlueprintCallable, Category = "GV2|Runtime")
    int64 RequestLoad(const FString& SlotId, EGV2SaveSlotRevision Revision = EGV2SaveSlotRevision::Current);

    UFUNCTION(BlueprintCallable, Category = "GV2|Runtime")
    ESessionCancellationResult CancelSessionRequest(int64 OperationId);

    UFUNCTION(BlueprintCallable, Category = "GV2|Runtime")
    bool GetSessionOperationOutcome(int64 OperationId, ESessionOperationOutcome& OutOutcome, FGV2OperationFault& OutFault) const;

    TOptional<FGV2SessionOperationResult> GetSessionOperationOutcome(uint64 OperationId) const;

    UFUNCTION(BlueprintCallable, Category = "GV2|Runtime")
    bool IsSessionOperationEvicted(int64 OperationId) const;

    UFUNCTION(BlueprintCallable, Category = "GV2|Runtime")
    ESessionOperationQueryStatus QuerySessionOperation(int64 OperationId, FGV2SessionOperationResult& OutResult) const;

    UFUNCTION(BlueprintCallable, Category = "GV2|Runtime")
    void StartSession();

    UFUNCTION(BlueprintCallable, Category = "GV2|Runtime")
    void EndSession();

    UFUNCTION(BlueprintPure, Category = "GV2|Runtime")
    FString GetActiveSeedHex() const;

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    UUserWidget* GetActiveScreen() const;

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    UGV2GameShellWidgetBase* GetActiveGameShell() const;

    UGV2ScreenWidgetBase* GetActiveScreenInLayer(FName Layer, FName InstanceKey) const;

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    void SetActiveTab(const FString& ContainerPath, const FString& TabKey);

    virtual void NotifyActiveTab(const FString& ContainerPath, const FString& TabKey) override
    {
        SetActiveTab(ContainerPath, TabKey);
    }

    // PEP-06B: the runtime-side half of the sink boundary UGV2PresentationInteractionSink
    // declares -- GV2PresentationApply cannot name a layer or reach ActiveGameShell/
    // Reconciler itself (PSC-09A), so this override is the only place that decides which
    // layer a hover overlay lands in and forwards to the two calls PEP-06 already built for
    // exactly this purpose.
    virtual bool OpenHoverOverlay(UUserWidget* Widget, FName& OutInstanceKey, FString& OutError) override;
    virtual void CloseHoverOverlay(FName InstanceKey) override;

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    FString GetActiveTab(const FString& ContainerPath) const;

private:
    // PSC-06 (ADR-0043 D1): resolves via the coordinator's current session content
    // snapshot (FGV2PresentationPrepareContext) -- UGV2RuntimeSubsystem no longer owns a
    // separate, GameInstance-lifetime Screen Registry authority of its own (PAH-R3's
    // original defect: this used to be built once in Initialize(), before any session's
    // actual package set was even known).
    UClass* ResolveScreenClass(
        const FString& ScreenId,
        const FGV2ScreenPlacement& Placement,
        const FGV2PresentationPrepareContext& PrepareContext) const;
    UGV2ScreenWidgetBase* InstantiateScreenWidget(
        const FString& ScreenId,
        const FGV2ScreenPlacement& Placement,
        const FGV2PresentationPrepareContext& PrepareContext);
    void HandleStartGameInstance(UGameInstance* StartedGameInstance);
    void HandleViewportResized(FViewport* Viewport, uint32 Unused);
    bool HandleDocumentRequested(
        const FGV2UiDocumentViewModel& Document,
        const FGV2PresentationPrepareContext& PrepareContext);
    void ReplaceActiveScreen(UUserWidget* NewScreen);
    void TeardownActiveProjection();
    void PublishActiveProjection();

public:
#if WITH_DEV_AUTOMATION_TESTS
    // PSC-10C: the session's image catalog now lives only in its content snapshot -- the
    // process-global one a test could inspect is gone. This exposes the snapshot itself so a
    // test can still assert the observable property ("a failed session publishes none; a
    // recovered one publishes a catalog that resolves real content") instead of asserting it
    // through a global that production no longer has.
    const FGV2SessionContentSnapshot* GetContentSnapshotForAutomationTest() const;
    FGV2SessionCoordinator* GetCoordinatorForAutomationTest() const { return Coordinator.Get(); }

    static bool bTestForceDocumentSinkFailure;
    static bool bTestForceRepositoryNotReady;
#endif

private:
    TPimplPtr<FGV2SessionCoordinator> Coordinator;
    TPimplPtr<FGV2RepositoryPublisher> RepositoryPublisher;
    std::unique_ptr<GV2RuntimeCore::FFilesystemSaveSlotStorage> SaveSlotStorage;

    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> ActiveScreen;

    UPROPERTY(Transient)
    TObjectPtr<UGV2GameShellWidgetBase> ActiveGameShell;

    UPROPERTY(Transient)
    TObjectPtr<UGV2GameShellWidgetBase> PendingGameShell;

    UPROPERTY(Transient)
    TObjectPtr<UGV2ScreenWidgetBase> PendingScreen;

    TPimplPtr<FGV2LayeredUiReconciler> Reconciler;

    FDelegateHandle StartGameInstanceHandle;
    FDelegateHandle ViewportResizedHandle;
    FString RepositoryBuildError;

    // PSC-02 (ADR-0043 D1/D5): resolved exactly once in Initialize() -- repository build,
    // Screen Registry build, and every FGV2SessionCoordinator::StartSession() call (one
    // per session replacement, potentially many per GameInstance lifetime) all consume
    // THIS value; none of them re-discovers the package set independently.
    TOptional<GV2ContentHostSupport::FResolvedPackageSet> ResolvedPackageSet;
    bool bRepositoryReady = false;
    bool bActiveScreenAddedToViewport = false;
};
