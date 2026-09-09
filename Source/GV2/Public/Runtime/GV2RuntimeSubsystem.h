#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/PimplPtr.h"
#include "GV2RuntimeSubsystem.generated.h"

namespace GV2PackageClosure { struct FEntry; }

class FGV2LayeredUiReconciler;
class FGV2RepositoryPublisher;
class FGV2ScreenPlacement;
class FGV2SessionContentSnapshot;
class FGV2SessionCoordinator;
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
class GV2_API UGV2RuntimeSubsystem : public UGameInstanceSubsystem
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

    UFUNCTION(BlueprintCallable, Category = "GV2|Runtime")
    void StartSession();

    UFUNCTION(BlueprintCallable, Category = "GV2|Runtime")
    void EndSession();

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    UUserWidget* GetActiveScreen() const;

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    UGV2GameShellWidgetBase* GetActiveGameShell() const;

    UGV2ScreenWidgetBase* GetActiveScreenInLayer(FName Layer, FName InstanceKey) const;

    UFUNCTION(BlueprintCallable, Category = "GV2|UI")
    void SetActiveTab(const FString& ContainerPath, const FString& TabKey);

    UFUNCTION(BlueprintPure, Category = "GV2|UI")
    FString GetActiveTab(const FString& ContainerPath) const;

private:
    // PSC-06 (ADR-0043 D1): resolves via the coordinator's current session content
    // snapshot (FGV2PresentationPrepareContext) -- UGV2RuntimeSubsystem no longer owns a
    // separate, GameInstance-lifetime Screen Registry authority of its own (PAH-R3's
    // original defect: this used to be built once in Initialize(), before any session's
    // actual package set was even known).
    UClass* ResolveScreenClass(const FString& ScreenId, const FGV2ScreenPlacement& Placement) const;
    UGV2ScreenWidgetBase* InstantiateScreenWidget(const FString& ScreenId, const FGV2ScreenPlacement& Placement);
    void HandleStartGameInstance(UGameInstance* StartedGameInstance);
    bool HandleDocumentRequested(const FGV2UiDocumentViewModel& Document);
    void ReplaceActiveScreen(UUserWidget* NewScreen);

public:
#if WITH_DEV_AUTOMATION_TESTS
    // PSC-10C: the session's image catalog now lives only in its content snapshot -- the
    // process-global one a test could inspect is gone. This exposes the snapshot itself so a
    // test can still assert the observable property ("a failed session publishes none; a
    // recovered one publishes a catalog that resolves real content") instead of asserting it
    // through a global that production no longer has.
    const FGV2SessionContentSnapshot* GetContentSnapshotForAutomationTest() const;
#endif

private:
    TPimplPtr<FGV2SessionCoordinator> Coordinator;
    TPimplPtr<FGV2RepositoryPublisher> RepositoryPublisher;

    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> ActiveScreen;

    UPROPERTY(Transient)
    TObjectPtr<UGV2GameShellWidgetBase> ActiveGameShell;

    TPimplPtr<FGV2LayeredUiReconciler> Reconciler;

    FDelegateHandle StartGameInstanceHandle;
    FString RepositoryBuildError;

    // PSC-02 (ADR-0043 D1/D5): resolved exactly once in Initialize() -- repository build,
    // Screen Registry build, and every FGV2SessionCoordinator::StartSession() call (one
    // per session replacement, potentially many per GameInstance lifetime) all consume
    // THIS value; none of them re-discovers the package set independently.
    TOptional<GV2ContentHostSupport::FResolvedPackageSet> ResolvedPackageSet;
    bool bRepositoryReady = false;
    bool bActiveScreenAddedToViewport = false;
};
