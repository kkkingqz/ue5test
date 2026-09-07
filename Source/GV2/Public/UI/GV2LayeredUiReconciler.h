#pragma once

#include "CoreMinimal.h"
#include "Bridge/GV2BridgeTypes.h"

#include "UI/GV2ScreenWidgetBase.h"

class UGV2GameShellWidgetBase;
class UGV2ScreenWidgetBase;

// PAH-07 (ADR-0042, INV-P4): presentation has exactly one committed logical state; this is
// its observable health, distinct from a log line nobody reads (DCA-20's `bFatal`). A
// Commit failure whose OWN compensating rollback (GBH-10, ADR-0041) also failed leaves the
// physical tree's relationship to the committed document undefined -- quick, in-place
// inverse-mutation recovery already tried and failed. `RecoveredFromCatastrophicFailure`
// means the physical tree was discarded and rebuilt from the last successfully committed
// document instead; it is distinct from an ordinary Commit failure whose own rollback
// succeeded (which leaves Health at `Nominal`, the previous revision intact in place) --
// the two are different observable outcomes, not the same "false" collapsed together.
enum class EGV2PresentationHealth : uint8
{
    Nominal,
    RecoveredFromCatastrophicFailure,
    // Even the last known-good document failed to reapply against an emptied Shell --
    // no further fallback exists. Structurally possible (defensive), not exercised by a
    // dedicated test: it requires a second, independent failure on top of the first.
    CatastrophicRecoveryFailed,
};

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
        FGV2ScreenMutationPlan MutationPlan;
        bool bIsReuse = false;
    };

    struct FPreparedReconciliationPlan
    {
        TArray<FPreparedScreenInstance> ScreensToUpdateOrAttach;
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

    // Full atomic reconciliation: Prepare + Commit. PAH-07: on success, Document becomes
    // the new committed state (LastCommittedDocument) other calls recover FROM, not a
    // snapshot of what CommitReconcile happened to produce physically. On a failure whose
    // OutError carries GGV2UiRollbackFailedDiagnosticCode, performs catastrophic recovery
    // (see GetHealth()) before returning false -- the caller still learns THIS candidate
    // was rejected the same way as any other Commit failure; GetHealth() is how it learns
    // recovery was catastrophic rather than an ordinary in-place rollback.
    [[nodiscard]] bool Reconcile(
        UGV2GameShellWidgetBase* Shell,
        const FGV2UiDocumentViewModel& Document,
        FScreenFactory ScreenFactory,
        FString& OutError,
        TFunction<bool(const FString& ScreenId, const FString& PropertyPath)> ScreenCommitFailureInjector = nullptr,
        TFunction<bool(const FString& ScreenId, const FString& PropertyPath)> ScreenRollbackFailureInjector = nullptr);

    UGV2ScreenWidgetBase* GetActiveScreen(FName Layer, FName InstanceKey) const;
    const TMap<FScreenSlotKey, FActiveScreenEntry>& GetActiveScreens() const { return ActiveScreens; }

    // PAH-07 (ADR-0042, INV-P4).
    EGV2PresentationHealth GetHealth() const { return Health; }
    const FGV2UiDocumentViewModel* GetLastCommittedDocument() const { return LastCommittedDocument.GetPtrOrNull(); }

    // Session-boundary reset: clears active screens AND the committed presentation state
    // (Health, LastCommittedDocument) -- a new session has no prior document to recover
    // to. Distinct from the internal, mid-session ActiveScreens-only clear catastrophic
    // recovery performs, which must preserve LastCommittedDocument to replay it.
    void Reset();

private:
    // PAH-07: discards the physical tree (Shell->ClearAllLayers(), ActiveScreens cleared)
    // and rebuilds it from LastCommittedDocument via a fresh, uninjected Prepare/Commit --
    // "обычный свежий Prepare/Apply против пустого GameShell", not a replay of physical
    // widget state. Sets Health to the outcome; never recurses into Reconcile().
    void PerformCatastrophicRecovery(UGV2GameShellWidgetBase* Shell, FScreenFactory ScreenFactory);

    TMap<FScreenSlotKey, FActiveScreenEntry> ActiveScreens;
    EGV2PresentationHealth Health = EGV2PresentationHealth::Nominal;
    TOptional<FGV2UiDocumentViewModel> LastCommittedDocument;
};
