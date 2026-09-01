#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "CommonUserWidget.h"
#include "UI/GV2UiMutationPlan.h"
#include "GV2ScreenWidgetBase.generated.h"

struct FGV2ScreenFieldPlan
{
    TObjectPtr<UUserWidget> HostWidget;
    FGV2UiHostMutationPlan MutationPlan;
    TSharedPtr<const FGV2PreparedUiObject> CommittedValue;
};

struct FGV2ScreenMutationPlan
{
    TArray<FGV2ScreenFieldPlan> FieldPlans;
};

UCLASS(Blueprintable)
class GV2_API UGV2ScreenWidgetBase : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    // UPP-27 / UPP-28: Prepares every field's full mutation plan without modifying any widget.
    // Predicts deep child failures (STATUS-004) before commit.
    // PCC-06: [[nodiscard]] -- a discarded result is exactly the swallowed-failure shape
    // this task exists to make impossible; see GV2LayeredUiReconciler.cpp for why.
    // DUC-11: ActiveCompositionChain is the ordered list of screen_ids already being
    // prepared on this call stack (root screen first). A nested screen (DUC-09's
    // FGV2TabContainerTabsPropertyConsumer) forwards its own chain plus the tab's
    // screen_id into this same parameter on the recursive call, so a screen_id
    // reappearing on its own composition path -- direct (a tab pointing at its own
    // screen) or indirect (through an intermediate screen) -- is caught as a cycle.
    // nullptr (the default, used by ApplyScreenFields/CanApplyScreenFields below and
    // by any standalone caller) simply means "not tracking a chain here": screen_id
    // is a runtime string lookup via Screen Registry, not a Blueprint class
    // reference, so UMG's own circular-dependency detection has no visibility into
    // it -- this is the only guard against it.
    [[nodiscard]] bool PrepareScreenFields(
        const TArray<FGV2ScreenFieldValue>& ScreenFields,
        FGV2ScreenMutationPlan& OutPlan,
        FString& OutError,
        const TArray<FString>* ActiveCompositionChain = nullptr) const;

    // Commits a prepared mutation plan. FailureInjector mirrors CommitUiHostProperties'
    // own injector (PCC-06/07 fault-injection tests only; production always omits it).
    [[nodiscard]] bool CommitScreenFields(
        const FGV2ScreenMutationPlan& Plan,
        TFunction<bool(const FString& PropertyPath)> FailureInjector = nullptr);

    // One-shot Prepare + Commit for standalone screen usage.
    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Screen")
    [[nodiscard]] bool ApplyScreenFields(const TArray<FGV2ScreenFieldValue>& ScreenFields);

    // Public preflight: runs Prepare pass and discards the plan.
    UFUNCTION(BlueprintPure, Category = "GV2|UI|Screen")
    [[nodiscard]] bool CanApplyScreenFields(const TArray<FGV2ScreenFieldValue>& ScreenFields) const;

    // Screen Field ids this screen's tree currently declares (via
    // IGV2ScreenFieldHost), for contract introspection/tests.
    UFUNCTION(BlueprintPure, Category = "GV2|UI|Screen")
    TArray<FName> GetScreenFieldIds() const;

protected:
    UFUNCTION(BlueprintImplementableEvent, Category = "GV2|UI|Screen", meta = (DisplayName = "On Screen Fields Applied"))
    void OnScreenFieldsApplied();
};
