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
class FGV2SessionCoordinator;
class UGV2GameShellWidgetBase;
class UGV2ScreenRegistry;
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
    // PSC-02 (ADR-0043 D1/D5): ClosureEntries is this GameInstance's single resolved
    // package set (see ResolvedPackageSet below), already computed by Initialize() --
    // LoadScreenRegistry() threads it into UGV2ScreenRegistry::Build() instead of the
    // registry discovering its own package closure (PAH-R3).
    bool LoadScreenRegistry(const TArray<GV2PackageClosure::FEntry>& ClosureEntries);
    UClass* ResolveScreenClass(const FString& ScreenId, const FGV2ScreenPlacement& Placement) const;
    UGV2ScreenWidgetBase* InstantiateScreenWidget(const FString& ScreenId, const FGV2ScreenPlacement& Placement);
    void HandleStartGameInstance(UGameInstance* StartedGameInstance);
    bool HandleDocumentRequested(const FGV2UiDocumentViewModel& Document);
    void ReplaceActiveScreen(UUserWidget* NewScreen);

    TPimplPtr<FGV2SessionCoordinator> Coordinator;
    TPimplPtr<FGV2RepositoryPublisher> RepositoryPublisher;

    UPROPERTY(Transient)
    TObjectPtr<UGV2ScreenRegistry> ScreenRegistry;

    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> ActiveScreen;

    UPROPERTY(Transient)
    TObjectPtr<UGV2GameShellWidgetBase> ActiveGameShell;

    TPimplPtr<FGV2LayeredUiReconciler> Reconciler;

    FDelegateHandle StartGameInstanceHandle;
    bool bScreenRegistryReady = false;
    FString RepositoryBuildError;

    // PSC-02 (ADR-0043 D1/D5): resolved exactly once in Initialize() -- repository build,
    // Screen Registry build, and every FGV2SessionCoordinator::StartSession() call (one
    // per session replacement, potentially many per GameInstance lifetime) all consume
    // THIS value; none of them re-discovers the package set independently.
    TOptional<GV2ContentHostSupport::FResolvedPackageSet> ResolvedPackageSet;
    bool bRepositoryReady = false;
    bool bActiveScreenAddedToViewport = false;
};
