#pragma once

#include "CoreMinimal.h"
#include "Bridge/GV2BridgeTypes.h"

#include "UI/GV2ScreenWidgetBase.h"

class UGV2GameShellWidgetBase;
class UGV2ScreenWidgetBase;

/**
 * FGV2LayeredUiReconciler (UIF-19, UIF-20, UPP-28)
 * Reconciles active UI widgets across Game Shell layers based on document envelopes
 * using a strict two-phase atomic Prepare / Commit transaction.
 */
class GV2_API FGV2LayeredUiReconciler
{
public:
    struct FScreenSlotKey
    {
        FName Layer;
        FName InstanceKey;

        bool operator==(const FScreenSlotKey& Other) const
        {
            return Layer == Other.Layer && InstanceKey == Other.InstanceKey;
        }

        friend uint32 GetTypeHash(const FScreenSlotKey& Key)
        {
            return HashCombine(GetTypeHash(Key.Layer), GetTypeHash(Key.InstanceKey));
        }
    };

    struct FActiveScreenEntry
    {
        FString ScreenId;
        TObjectPtr<UGV2ScreenWidgetBase> Widget;
    };

    struct FPreparedScreenInstance
    {
        FName Layer;
        FName InstanceKey;
        FString ScreenId;
        TObjectPtr<UGV2ScreenWidgetBase> TargetWidget;
        TObjectPtr<UGV2ScreenWidgetBase> ReplacedOldWidget; // non-null if replacing an existing screen in this slot
        FGV2ScreenMutationPlan MutationPlan;
        bool bIsReuse = false;
    };

    // PAH-06A: a screen no longer present in the incoming document, paired with the layer
    // it was attached to -- CommitReconcile needs Layer to skip an entry the modal_stack
    // ReconcilePrepared commit already removed atomically (its own ClearChildren already
    // dropped it), so a redundant DetachScreen there doesn't log a spurious "no parent" warning.
    struct FDetachEntry
    {
        FName Layer;
        TObjectPtr<UGV2ScreenWidgetBase> Widget;
    };

    struct FPreparedReconciliationPlan
    {
        TArray<FPreparedScreenInstance> ScreensToUpdateOrAttach;
        TArray<FDetachEntry> ScreensToDetach;
        TMap<FScreenSlotKey, FActiveScreenEntry> NewActiveScreens;
        bool bHasModals = false;
        TArray<TObjectPtr<UGV2ScreenWidgetBase>> Modals; // In modal_stack order
    };

    // PAH-02: Layer is the requested top-level Placement for ScreenId (every screen this
    // factory instantiates is a top-level route/overlay/modal instance -- embedded/tab
    // screens resolve through FGV2TabContainerTabsPropertyConsumer instead), so an
    // implementation backed by UGV2ScreenRegistry::Resolve can reject a screen registered
    // for a different layer instead of handing out its class regardless.
    using FScreenFactory = TFunctionRef<UGV2ScreenWidgetBase*(const FString& ScreenId, FName Layer)>;

    // UPP-28: Prepares the complete reconciliation plan for every layer and screen
    // before touching any widget or mutating Game Shell. If any screen fails preparation,
    // this returns false, leaving all active screens, widgets, and shell untouched.
    // PCC-06: [[nodiscard]] -- a discarded result here is the exact swallowed-failure
    // shape this task exists to make impossible.
    [[nodiscard]] bool PrepareReconcile(
        UGV2GameShellWidgetBase* Shell,
        const FGV2UiDocumentViewModel& Document,
        FScreenFactory ScreenFactory,
        FPreparedReconciliationPlan& OutPlan,
        FString& OutError) const;

    // UPP-28 / PCC-06: Commits a cleanly prepared reconciliation plan to the Game Shell
    // and active widgets. Attach/Commit are checked per screen; the first failure stops
    // the traversal (ADR-0040: the failed screen is not published, ActiveScreens keeps
    // its previous revision) instead of continuing on to the remaining screens.
    // ScreenCommitFailureInjector mirrors CommitScreenFields' own injector, keyed by
    // (screen_id, property_path); PCC-06/07 fault-injection tests only, production omits it.
    // PAH-01: OutError carries GGV2UiRollbackFailedDiagnosticCode (GV2UiMutationPlan.h)
    // when a Commit or Attach failure's compensating rollback -- per-screen self-heal,
    // sibling-screen restoration, or the ShellAttach detach/reattach undo -- itself fails.
    // ScreenRollbackFailureInjector is test-only (production always omits it), mirroring
    // ScreenCommitFailureInjector but for the rollback/undo replay specifically.
    [[nodiscard]] bool CommitReconcile(
        UGV2GameShellWidgetBase* Shell,
        const FPreparedReconciliationPlan& Plan,
        FString& OutError,
        TFunction<bool(const FString& ScreenId, const FString& PropertyPath)> ScreenCommitFailureInjector = nullptr,
        TFunction<bool(const FString& ScreenId, const FString& PropertyPath)> ScreenRollbackFailureInjector = nullptr);

    // Full atomic reconciliation: Prepare + Commit.
    [[nodiscard]] bool Reconcile(
        UGV2GameShellWidgetBase* Shell,
        const FGV2UiDocumentViewModel& Document,
        FScreenFactory ScreenFactory,
        FString& OutError,
        TFunction<bool(const FString& ScreenId, const FString& PropertyPath)> ScreenCommitFailureInjector = nullptr,
        TFunction<bool(const FString& ScreenId, const FString& PropertyPath)> ScreenRollbackFailureInjector = nullptr);

    UGV2ScreenWidgetBase* GetActiveScreen(FName Layer, FName InstanceKey) const;
    const TMap<FScreenSlotKey, FActiveScreenEntry>& GetActiveScreens() const { return ActiveScreens; }
    void Reset();

private:
    TMap<FScreenSlotKey, FActiveScreenEntry> ActiveScreens;
};
