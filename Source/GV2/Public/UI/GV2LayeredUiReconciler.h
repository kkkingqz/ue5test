#pragma once

#include "CoreMinimal.h"
#include "Bridge/GV2BridgeTypes.h"

#include "GV2PresentationApply/PreparedPresentationTransaction.h"
#include "UI/GV2ScreenWidgetBase.h"
#include "UObject/StrongObjectPtr.h"

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
        TWeakObjectPtr<UGV2ScreenWidgetBase> Widget;
    };

    struct FPreparedScreenInstance
    {
        FName Layer;
        FName InstanceKey;
        FString ScreenId;
        TWeakObjectPtr<UGV2ScreenWidgetBase> TargetWidget;
        TStrongObjectPtr<UGV2ScreenWidgetBase> CandidateWidget;
        FGV2ScreenMutationPlan MutationPlan;
        // PSC-10B: central style resolved for this screen's whole subtree during Prepare and
        // applied in CommitReconcile. It sits beside MutationPlan rather than inside it
        // because it is not keyed by declared field -- every styled widget below the screen
        // gets an operation whether or not it hosts a field.
        GV2PresentationApply::FGV2PreparedPresentationTransaction CentralStyleTransaction;
        bool bIsReuse = false;
    };

    struct FPreparedReconciliationPlan
    {
        TArray<FPreparedScreenInstance> ScreensToUpdateOrAttach;
        TMap<FScreenSlotKey, FActiveScreenEntry> NewActiveScreens;
        bool bHasModals = false;
        TArray<TWeakObjectPtr<UGV2ScreenWidgetBase>> Modals; // In modal_stack order
    };

    // PEP-06 (ADR-0042): a layer participant not sourced from the desired document --
    // registered and unregistered by direct host-local calls (hover open/close today),
    // never by PrepareReconcile/CommitReconcile's own document diff. Widget is a borrowed
    // reference: once AttachHostLocalScreen parents it into the layer's panel, the panel
    // owns it the same way it owns every document-tier child (PSC-11) -- this entry is a
    // lookup index, not an owning pointer, mirroring FActiveScreenEntry's own shape.
    struct FHostLocalScreenEntry
    {
        TWeakObjectPtr<UGV2ScreenWidgetBase> Widget;
        int64 CreationOrder = 0;
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
    // PSC-06 (ADR-0043 D1): PrepareContext is forwarded unchanged, down to
    // PrepareUiHostProperties -- see its own doc comment.
    // PSC-10B: it is REQUIRED, not an optional pointer. Since no widget resolves a Theme of
    // its own any more, a Prepare without the session snapshot produces a physically correct
    // and entirely unstyled tree; making the parameter mandatory is what stops that being
    // expressible. The catastrophic-recovery replay -- the call site the old default existed
    // for -- now forwards the context of the reconcile that triggered it.
    [[nodiscard]] bool PrepareReconcile(
        UGV2GameShellWidgetBase* Shell,
        const FGV2UiDocumentViewModel& Document,
        FScreenFactory ScreenFactory,
        FPreparedReconciliationPlan& OutPlan,
        FString& OutError,
        const FGV2PresentationPrepareContext& PrepareContext) const;

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
        const FGV2PresentationPrepareContext& PrepareContext,
        TFunction<bool(const FString& ScreenId, const FString& PropertyPath)> ScreenCommitFailureInjector = nullptr,
        TFunction<bool(const FString& ScreenId, const FString& PropertyPath)> ScreenRollbackFailureInjector = nullptr);

    UGV2ScreenWidgetBase* GetActiveScreen(FName Layer, FName InstanceKey) const;
    const TMap<FScreenSlotKey, FActiveScreenEntry>& GetActiveScreens() const { return ActiveScreens; }

    // PEP-09: read-only lookup into the host-local registry (PEP-06), for a caller (the
    // runtime sink override) that only has the synthetic instance key AttachHostLocalScreen
    // returned and needs the widget back to gate its input on departure -- never a second
    // index, the same registry AttachHostLocalScreen/DetachHostLocalScreen already own.
    UGV2ScreenWidgetBase* GetHostLocalScreen(FName Layer, FName InstanceKey) const;

    // PEP-06: every host-local instance key this reconciler ever issues carries this
    // prefix, which no document-authored instance_key can (Lua identifiers never contain
    // ':' -- the same reservation Stable IDs already rely on). This is the single source
    // both AttachHostLocalScreen and the ownership ergate rely on to tell the two
    // enumerators apart; a document instance never collides with a host-local one by
    // construction, not by convention.
    static bool IsHostLocalInstanceKey(FName InstanceKey);

    // PEP-06 (ADR-0042): registers Widget as a host-local participant of Layer and
    // reconciles that one layer's panel immediately -- above every document-tier
    // participant, in creation order among other host-local participants. Widget must
    // already be fully prepared and styled by the caller (its own PrepareScreenFields/
    // GV2CentralStylePreparer pass); this call performs no Prepare of its own; PSC-10B is
    // exactly this: whoever opens a host-local participant resolves nothing here. Returns
    // the synthetic instance key the caller must keep and pass to DetachHostLocalScreen.
    [[nodiscard]] bool AttachHostLocalScreen(
        UGV2GameShellWidgetBase* Shell,
        FName Layer,
        UGV2ScreenWidgetBase* Widget,
        FName& OutInstanceKey,
        FString& OutError);

    // PEP-06: unregisters a host-local participant and reconciles its layer's panel
    // without it. Dropping it from the panel's rebuilt child list is what releases the
    // Shell's ownership (PSC-11) -- this call does not call RemoveFromParent itself.
    [[nodiscard]] bool DetachHostLocalScreen(
        UGV2GameShellWidgetBase* Shell,
        FName Layer,
        FName InstanceKey,
        FString& OutError);

    // PSC-14: refreshes only viewport-derived physical values on the already-committed
    // screen instances. It does not Prepare/reconcile a document and therefore preserves
    // widget identity and UI-local state.
    [[nodiscard]] bool RefreshViewportPresentation(float ViewportHeight, FString& OutError) const;

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
    void PerformCatastrophicRecovery(
        UGV2GameShellWidgetBase* Shell,
        FScreenFactory ScreenFactory,
        const FGV2PresentationPrepareContext& PrepareContext);

    // PEP-06: one layer participant, from either tier -- document or host-local -- reduced
    // to exactly what FGV2KeyedCollection::ReconcilePrepared needs. Building this uniformly
    // for both tiers is what lets CommitLayerParticipants run one rebuild over their union
    // instead of two.
    struct FGV2LayerParticipant
    {
        FName Key;
        // A transient, per-call reduction of an already-tracked TWeakObjectPtr
        // (FActiveScreenEntry::Widget or FHostLocalScreenEntry::Widget) -- never itself
        // the owning or lookup reference the ownership gate's untraced-pointer check
        // exists for, but typed the same way regardless (validate_session_snapshot_
        // ownership.py checks every member of every configured type unconditionally).
        TWeakObjectPtr<UGV2ScreenWidgetBase> Widget;
    };

    // PEP-06: reconciles Layer's panel from Participants (the document-tier participants
    // the caller already knows about -- freshly prepared for CommitReconcile's own
    // per-layer step, or recovered from the panel's current children for the standalone
    // Attach/DetachHostLocalScreen calls) plus this reconciler's own current
    // HostLocalScreens registry for that layer, appended after in creation order --
    // document tier below, host-local tier above. Shared by every commit path so they can
    // never drift into different rebuild logic. OutPreviousOrder mirrors
    // FGV2KeyedCollection::ReconcilePrepared's own parameter of the same purpose (only
    // CommitReconcile's multi-layer transaction needs it, for cross-layer rollback).
    bool CommitLayerParticipants(
        UGV2GameShellWidgetBase* Shell,
        FName Layer,
        TArray<FGV2LayerParticipant> Participants,
        FString& OutError,
        TArray<UGV2ScreenWidgetBase*>* OutPreviousOrder = nullptr);

    TMap<FScreenSlotKey, FActiveScreenEntry> ActiveScreens;
    // PEP-06: keyed the same way as ActiveScreens, but never touched by PrepareReconcile/
    // CommitReconcile's own document diff -- see FHostLocalScreenEntry's own doc comment.
    TMap<FScreenSlotKey, FHostLocalScreenEntry> HostLocalScreens;
    int64 NextHostLocalCreationOrder = 1;
    EGV2PresentationHealth Health = EGV2PresentationHealth::Nominal;
    TOptional<FGV2UiDocumentViewModel> LastCommittedDocument;
};
