#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/PimplPtr.h"
#include "GV2RuntimeSubsystem.generated.h"

class FGV2RepositoryPublisher;
class FGV2SessionCoordinator;
class UGV2GameShellWidgetBase;
class UGV2ScreenRegistry;
class UGV2ScreenWidgetBase;
class UUserWidget;

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
    bool LoadScreenRegistry();
    UClass* ResolveScreenClass(const FString& ScreenId) const;
    UGV2ScreenWidgetBase* CreateRegisteredScreen(
        const FGV2ScreenViewModel& Model,
        bool bAddToViewport);
    UGV2ScreenWidgetBase* InstantiateScreenWidget(const FString& ScreenId);
    void HandleStartGameInstance(UGameInstance* StartedGameInstance);
    bool HandleScreenRequested(const FGV2ScreenViewModel& Model);
    bool HandleDocumentRequested(const FGV2UiDocumentViewModel& Document);
    void ReplaceActiveScreen(UUserWidget* NewScreen);

    TPimplPtr<FGV2SessionCoordinator> Coordinator;
    TPimplPtr<FGV2RepositoryPublisher> RepositoryPublisher;

    UPROPERTY(Transient)
    TObjectPtr<UGV2ScreenRegistry> ScreenRegistry;

    UPROPERTY(Transient)
    TMap<FString, TObjectPtr<UClass>> RegisteredScreenClasses;

    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> ActiveScreen;

    UPROPERTY(Transient)
    TObjectPtr<UGV2GameShellWidgetBase> ActiveGameShell;

    FGV2LayeredUiReconciler Reconciler;

    FDelegateHandle StartGameInstanceHandle;
    FString ImageCatalogBuildError;
    bool bImageCatalogReady = false;
    FString RepositoryBuildError;
    bool bRepositoryReady = false;
    bool bActiveScreenAddedToViewport = false;
};
