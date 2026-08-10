#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/PimplPtr.h"
#include "GV2RuntimeSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FGV2TestInteractionAccepted,
    const FString&, TestAction,
    FGV2UiBindingHandle, BindingHandle,
    int64, Sequence);

class FGV2SessionCoordinator;

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

    UFUNCTION(BlueprintCallable, Category = "GV2|Testing", meta = (DevelopmentOnly))
    void StartTestSession();

    UFUNCTION(BlueprintCallable, Category = "GV2|Testing", meta = (DevelopmentOnly))
    void EndTestSession();

    UFUNCTION(BlueprintCallable, Category = "GV2|Testing", meta = (DevelopmentOnly))
    FGV2TestScreenViewModel CreateTestScreenModel(
        const FText& DescriptionText,
        const TArray<FGV2TestButtonSpec>& Buttons);

    UPROPERTY(BlueprintAssignable, Category = "GV2|Testing")
    FGV2TestInteractionAccepted OnTestInteractionAccepted;

private:
    TPimplPtr<FGV2SessionCoordinator> Coordinator;
};
