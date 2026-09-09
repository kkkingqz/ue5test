#include "UI/GV2LayeredUiReconciler.h"

#include "UI/GV2CentralStylePreparer.h"
#include "UI/GV2LegacyPresentationApplyAdapter.h"

#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "UI/GV2GameShellWidgetBase.h"
#include "UI/GV2KeyedCollection.h"
#include "UI/GV2ScreenWidgetBase.h"

namespace
{
// PAH-06B: FGV2KeyedCollection::ReconcilePrepared's own Prepare/Commit item lifecycle is
// unused here -- every widget is already fully resolved (reused or created by
// ScreenFactory) and its fields already committed by CommitReconcile's step 1, before
// this primitive is ever called for any layer. Each call exists purely for the
// primitive's keyed container-ordering and atomic-swap guarantee, so PreparedType
// carries no data.
struct FGV2LayerReconcilePrepared
{
};
}

bool FGV2LayeredUiReconciler::PrepareReconcile(
    UGV2GameShellWidgetBase* Shell,
    const FGV2UiDocumentViewModel& Document,
    FScreenFactory ScreenFactory,
    FPreparedReconciliationPlan& OutPlan,
    FString& OutError,
    const FGV2PresentationPrepareContext* PrepareContext) const
{
    OutPlan = {};
    OutError.Reset();
    const TArray<FGV2ScreenInstanceViewModel> IncomingInstances = Document.GetAllScreenInstances();

    // 1. Validation phase
    TSet<FScreenSlotKey> IncomingKeys;
    for (const FGV2ScreenInstanceViewModel& Instance : IncomingInstances)
    {
        if (!UGV2GameShellWidgetBase::IsValidLayerName(Instance.Layer))
        {
            OutError = FString::Printf(TEXT("Invalid layer name: '%s'"), *Instance.Layer.ToString());
            return false;
        }

        // GBH-01: AttachScreenToLayer's only *predictable* false-branches are a null
        // widget (already guarded above -- ScreenFactory failure returns false before
        // this instance could ever reach Commit) and a missing authored host for the
        // layer (Shell Blueprint misconfiguration). Both are knowable here, before any
        // live mutation, without calling Attach itself. Checked only when a real Shell
        // is given: many off-tree unit tests reconcile with Shell == nullptr, and Commit
        // itself skips Attach entirely in that case (see CommitReconcile step 3).
        if (Shell != nullptr && !Shell->HasHostForLayer(Instance.Layer))
        {
            OutError = FString::Printf(
                TEXT("core:diagnostic.ui_reconcile.missing_layer_host: layer '%s' has no authored host in Shell"),
                *Instance.Layer.ToString());
            return false;
        }

        const FScreenSlotKey Key{Instance.Layer, Instance.InstanceKey};
        if (IncomingKeys.Contains(Key))
        {
            OutError = FString::Printf(
                TEXT("Duplicate instance key '%s' in layer '%s'"),
                *Instance.InstanceKey.ToString(),
                *Instance.Layer.ToString());
            return false;
        }
        IncomingKeys.Add(Key);
    }

    // 2. Preparation phase: prepare mutation plan for each screen instance off-tree
    OutPlan.ScreensToUpdateOrAttach.Reserve(IncomingInstances.Num());

    for (const FGV2ScreenInstanceViewModel& Instance : IncomingInstances)
    {
        const FScreenSlotKey Key{Instance.Layer, Instance.InstanceKey};
        const FActiveScreenEntry* Existing = ActiveScreens.Find(Key);

        FPreparedScreenInstance PreparedInst;
        PreparedInst.Layer = Instance.Layer;
        PreparedInst.InstanceKey = Instance.InstanceKey;
        PreparedInst.ScreenId = Instance.ScreenId;

        if (Existing != nullptr && Existing->ScreenId == Instance.ScreenId && Existing->Widget != nullptr)
        {
            // Reuse existing widget instance (preserving UI-local state)
            PreparedInst.bIsReuse = true;
            PreparedInst.TargetWidget = Existing->Widget;
        }
        else
        {
            // Instantiate new screen widget. If this replaces an existing widget at the
            // same slot, the old widget needs no bookkeeping here: PAH-06B's per-layer
            // ReconcilePrepared commit (CommitReconcile) rebuilds each layer's container
            // from this round's incoming instances only, so a superseded widget --
            // simply absent from that rebuild -- is dropped atomically along with any
            // other removed screen, not detached by a separate step.
            PreparedInst.bIsReuse = false;
            PreparedInst.TargetWidget = ScreenFactory(Instance.ScreenId, Instance.Layer);
            if (PreparedInst.TargetWidget == nullptr)
            {
                OutError = FString::Printf(
                    TEXT("Failed to instantiate screen widget for screen_id '%s'"),
                    *Instance.ScreenId);
                return false;
            }
        }

        // Prepare screen fields (predicts any deep child failure across all field hosts).
        // DUC-11: seed the composition chain with this screen's own screen_id so a
        // nested tab (any depth below) resolving back to it -- directly or through
        // an intermediate screen -- is caught as a cycle rather than silently
        // accepted; see FGV2TabContainerTabsPropertyConsumer's own guard.
        const TArray<FString> RootCompositionChain{Instance.ScreenId};
        if (!PreparedInst.TargetWidget->PrepareScreenFields(Instance.Fields, PreparedInst.MutationPlan, OutError, &RootCompositionChain, PrepareContext))
        {
            if (OutError.IsEmpty())
            {
                OutError = FString::Printf(TEXT("Failed to prepare fields for screen '%s'"), *Instance.ScreenId);
            }
            return false;
        }

        // PSC-10B: the theme is read HERE, in Prepare, holding the session snapshot -- and
        // nowhere below. What reaches Commit is finished values.
        if (PrepareContext != nullptr)
        {
            GV2CentralStylePreparer::PrepareForSubtree(
                PreparedInst.TargetWidget, *PrepareContext, PreparedInst.CentralStyleTransaction);
        }

        OutPlan.NewActiveScreens.Add(Key, {Instance.ScreenId, PreparedInst.TargetWidget});
        if (Instance.Layer == UGV2GameShellWidgetBase::LayerModalStack)
        {
            OutPlan.Modals.Add(PreparedInst.TargetWidget);
        }

        OutPlan.ScreensToUpdateOrAttach.Add(MoveTemp(PreparedInst));
    }

    // PAH-06B: a screen active in a previous revision but absent from IncomingKeys needs
    // no bookkeeping here either -- CommitReconcile's per-layer ReconcilePrepared commit
    // rebuilds each layer's container from ScreensToUpdateOrAttach alone (this round's
    // incoming instances), so anything not in that list is dropped atomically by the same
    // per-layer ClearChildren-and-rebuild swap that handles reordering and replacement.

    OutPlan.bHasModals = Document.Modals.Num() > 0;
    return true;
}

// GBF-07: rollback_boundary=Document
bool FGV2LayeredUiReconciler::CommitReconcile(
    UGV2GameShellWidgetBase* Shell,
    const FPreparedReconciliationPlan& Plan,
    FString& OutError,
    TFunction<bool(const FString& ScreenId, const FString& PropertyPath)> ScreenCommitFailureInjector,
    TFunction<bool(const FString& ScreenId, const FString& PropertyPath)> ScreenRollbackFailureInjector)
{
    OutError.Reset();

    // 1. Commit every screen's mutation plan FIRST, before touching the Shell tree or
    // ActiveScreens at all. PCC-07: this is what makes the whole document commit atomic,
    // not just per-screen -- Commit only mutates a widget's own bound sub-widgets
    // (resolved by name), which does not require the widget to be attached to a parent
    // panel yet, so committing before attach/detach is safe and changes no normal-path
    // behavior. ADR-0040: an unexpected Commit-phase failure is an invariant violation,
    // not an ordinary `false` -- the failed screen must not be published, and because
    // nothing below (detach/attach/ActiveScreens/layer interactivity) has run yet, every
    // *other* layer's widget instances, bindings, and the previous ActiveScreens revision
    // are still exactly what they were before this call -- not merely equal in count.
    for (int32 InstIndex = 0; InstIndex < Plan.ScreensToUpdateOrAttach.Num(); ++InstIndex)
    {
        const FPreparedScreenInstance& Inst = Plan.ScreensToUpdateOrAttach[InstIndex];
        if (Inst.TargetWidget == nullptr)
        {
            continue;
        }
        TFunction<bool(const FString&)> PerScreenInjector = nullptr;
        TFunction<bool(const FString&)> PerScreenRollbackInjector = nullptr;
        if (ScreenCommitFailureInjector)
        {
            const FString ScreenId = Inst.ScreenId;
            PerScreenInjector = [ScreenCommitFailureInjector, ScreenId](const FString& PropertyPath)
            {
                return ScreenCommitFailureInjector(ScreenId, PropertyPath);
            };
        }
        if (ScreenRollbackFailureInjector)
        {
            const FString ScreenId = Inst.ScreenId;
            PerScreenRollbackInjector = [ScreenRollbackFailureInjector, ScreenId](const FString& PropertyPath)
            {
                return ScreenRollbackFailureInjector(ScreenId, PropertyPath);
            };
        }
        FString ScreenCommitError;
        const bool bScreenFieldsCommitted = Inst.TargetWidget->CommitScreenFields(
            Inst.MutationPlan, ScreenCommitError, PerScreenInjector, PerScreenRollbackInjector);

        // PSC-10B: central style is applied through the ordinary transaction Apply pair,
        // inside this same atomic step -- not as a separate lifecycle pass -- and strictly
        // AFTER the fields. The order is a requirement, not a preference: a keyed collection
        // creates its children's panel slots during field commit, and per-item slot padding
        // is one of the things a central-style operation writes, so styling first would
        // leave every newly created slot unstyled. Apply resolves nothing; every value in
        // the transaction was fixed during Prepare, so the only way this can fail is a
        // role/class mismatch, i.e. a bug in the preparer.
        bool bScreenStyled = true;
        if (bScreenFieldsCommitted)
        {
            bScreenStyled = GV2PresentationApply::Apply(Inst.CentralStyleTransaction, ScreenCommitError)
                && GV2LegacyPresentationApplyAdapter::Apply(Inst.CentralStyleTransaction, ScreenCommitError);
            if (!bScreenStyled)
            {
                // This screen's fields ARE committed, so unlike a CommitScreenFields failure
                // nothing has self-healed it yet; restore it here before the sibling loop
                // below restores the screens committed earlier in this same step.
                const FGV2UiRollbackResult SelfRollback = RollbackFieldPlans(
                    Inst.MutationPlan.FieldPlans, PerScreenRollbackInjector);
                if (!SelfRollback.bRestored && !ScreenCommitError.Contains(GGV2UiRollbackFailedDiagnosticCode))
                {
                    ScreenCommitError = FString::Printf(
                        TEXT("%s: %s"), GGV2UiRollbackFailedDiagnosticCode, *ScreenCommitError);
                }
            }
        }

        if (!bScreenFieldsCommitted || !bScreenStyled)
        {
            // GBH-10 (ADR-0041): CommitScreenFields already self-healed *this* screen
            // back to its own previous properties (and, per PAH-01, already folded
            // GGV2UiRollbackFailedDiagnosticCode into ScreenCommitError if that self-heal
            // itself failed). Screens committed earlier in this same step (indices
            // [0..InstIndex-1]; a null-widget entry never commits so it never needs
            // rollback either) still sit on their new revision and must be restored too --
            // ActiveScreens has not advanced yet, so the whole document commit must not
            // leave any screen physically on a revision it is not publishing.
            bool bSiblingRollbackFailed = false;
            for (int32 RollbackIndex = InstIndex - 1; RollbackIndex >= 0; --RollbackIndex)
            {
                const FGV2UiRollbackResult SiblingRollback = RollbackFieldPlans(
                    Plan.ScreensToUpdateOrAttach[RollbackIndex].MutationPlan.FieldPlans,
                    PerScreenRollbackInjector);
                bSiblingRollbackFailed |= !SiblingRollback.bRestored;
            }
            OutError = FString::Printf(
                TEXT("core:diagnostic.ui_reconcile.commit_failed: layer='%s' instance_key='%s' screen_id='%s': %s"),
                *Inst.Layer.ToString(), *Inst.InstanceKey.ToString(), *Inst.ScreenId, *ScreenCommitError);
            if (bSiblingRollbackFailed && !OutError.Contains(GGV2UiRollbackFailedDiagnosticCode))
            {
                OutError = FString::Printf(TEXT("%s: %s"), GGV2UiRollbackFailedDiagnosticCode, *OutError);
            }
            UE_LOG(LogTemp, Error, TEXT("CommitReconcile: %s"), *OutError);
            return false;
        }
    }

    // 2. PAH-06B (ADR-0042, INV-P3): every layer is reconciled atomically through the
    // shared FGV2KeyedCollection::ReconcilePrepared primitive, in the form PAH-06A proved
    // on modal_stack alone -- generalized here to all six layers, replacing the old
    // per-widget AttachScreenToLayer/DetachScreen loop entirely rather than leaving it
    // alongside. Desired order for a layer is that layer's own screen instances in
    // incoming-document order (the traversal order ScreensToUpdateOrAttach already
    // preserves, filtered per layer), not the traversal order of the whole multi-layer
    // plan. Every widget here already exists and its fields are already committed (step 1
    // above), so each layer's seed map always has every key its ReconcilePrepared call
    // could ask for -- CreateItem is unreachable by construction, and returns nullptr
    // (fail-closed) if that invariant is ever violated by a future change. A layer's own
    // ClearChildren-and-rebuild swap drops any widget not present in this round's
    // instances for it -- a removed screen, or the old widget at a slot whose screen_id
    // changed -- atomically, with no separate detach step required.
    struct FGV2LayerReconcileState
    {
        FName Layer;
        UPanelWidget* Host = nullptr;
        TArray<UGV2ScreenWidgetBase*> PreviousOrder;
        bool bCommitted = false;
    };
    TArray<FGV2LayerReconcileState> LayerStates;
    if (Shell != nullptr)
    {
        for (FName Layer : UGV2GameShellWidgetBase::GetApprovedLayers())
        {
            UPanelWidget* Host = Shell->GetHostForLayer(Layer);
            if (Host == nullptr)
            {
                // PrepareReconcile's own missing_layer_host check already rejected the
                // whole plan if this round has any instance for a hostless layer, so a
                // null Host here means this layer genuinely has nothing to reconcile.
                continue;
            }

            TArray<const FPreparedScreenInstance*> LayerInstances;
            for (const FPreparedScreenInstance& Inst : Plan.ScreensToUpdateOrAttach)
            {
                if (Inst.Layer == Layer && Inst.TargetWidget != nullptr)
                {
                    LayerInstances.Add(&Inst);
                }
            }

            TMap<FName, TObjectPtr<UGV2ScreenWidgetBase>> SeedByKey;
            for (const FPreparedScreenInstance* Inst : LayerInstances)
            {
                SeedByKey.Add(Inst->InstanceKey, Inst->TargetWidget);
            }

            FGV2LayerReconcileState& State = LayerStates.AddDefaulted_GetRef();
            State.Layer = Layer;
            State.Host = Host;

            TArray<UGV2ScreenWidgetBase*> OrderedOut;
            const bool bLayerReconciled = FGV2KeyedCollection::ReconcilePrepared<UGV2ScreenWidgetBase, const FPreparedScreenInstance*, FGV2LayerReconcilePrepared>(
                Host,
                LayerInstances,
                SeedByKey,
                [](const FPreparedScreenInstance* const& Inst) { return Inst->InstanceKey; },
                []() -> UGV2ScreenWidgetBase* { return nullptr; },
                [](UGV2ScreenWidgetBase&, const FPreparedScreenInstance* const&, FGV2LayerReconcilePrepared&) { return true; },
                [](UGV2ScreenWidgetBase&, const FGV2LayerReconcilePrepared&) {},
                OrderedOut,
                nullptr,
                &State.PreviousOrder);

            if (!bLayerReconciled)
            {
                // GBH-10 (ADR-0041): roll back every layer already committed earlier in
                // this same loop to its own exact prior order (via the
                // OutPreviousOrderedWidgets each of THEIR ReconcilePrepared calls
                // captured), then undo step 1's property commits for every screen in the
                // plan (all of them physically changed there, regardless of which layer's
                // reconcile fails) -- nothing else has mutated yet, so this leaves the
                // Shell tree, every screen's properties, and ActiveScreens (not yet
                // touched) exactly at the previous revision.
                bool bStructureRestoreFailed = false;
                for (const FGV2LayerReconcileState& CommittedLayer : LayerStates)
                {
                    if (!CommittedLayer.bCommitted)
                    {
                        continue;
                    }
                    CommittedLayer.Host->ClearChildren();
                    for (UGV2ScreenWidgetBase* PrevWidget : CommittedLayer.PreviousOrder)
                    {
                        if (PrevWidget != nullptr && CommittedLayer.Host->AddChild(PrevWidget) == nullptr)
                        {
                            bStructureRestoreFailed = true;
                            UE_LOG(LogTemp, Error,
                                TEXT("GBH-10: rollback failed restoring layer '%s' order after layer '%s' reconcile failure -- invariant violation"),
                                *CommittedLayer.Layer.ToString(), *Layer.ToString());
                        }
                    }
                }
                for (const FPreparedScreenInstance& CommittedInst : Plan.ScreensToUpdateOrAttach)
                {
                    const FGV2UiRollbackResult FieldRollback = RollbackFieldPlans(
                        CommittedInst.MutationPlan.FieldPlans,
                        ScreenRollbackFailureInjector
                            ? TFunction<bool(const FString&)>(
                                  [ScreenRollbackFailureInjector, ScreenId = CommittedInst.ScreenId](const FString& PropertyPath)
                                  { return ScreenRollbackFailureInjector(ScreenId, PropertyPath); })
                            : nullptr);
                    bStructureRestoreFailed |= !FieldRollback.bRestored;
                }

                OutError = FString::Printf(TEXT("core:diagnostic.ui_reconcile.layer_reconcile_failed: layer='%s'"), *Layer.ToString());
                if (bStructureRestoreFailed)
                {
                    OutError = FString::Printf(TEXT("%s: %s"), GGV2UiRollbackFailedDiagnosticCode, *OutError);
                }
                UE_LOG(LogTemp, Error, TEXT("CommitReconcile: %s"), *OutError);
                return false;
            }
            State.bCommitted = true;
        }
    }

    // 3. Commit active screens map -- only reached once every layer above committed
    // cleanly.
    ActiveScreens = Plan.NewActiveScreens;

    // 4. Layer Rules & Modal Interactivity (UIF-20)
    if (Shell != nullptr)
    {
        if (Plan.bHasModals)
        {
            Shell->SetLayerInteractive(UGV2GameShellWidgetBase::LayerBackground, false);
            Shell->SetLayerInteractive(UGV2GameShellWidgetBase::LayerLocationContent, false);
            Shell->SetLayerInteractive(UGV2GameShellWidgetBase::LayerCharacterPresentation, false);
            Shell->SetLayerInteractive(UGV2GameShellWidgetBase::LayerCoreInterface, false);
            Shell->SetLayerInteractive(UGV2GameShellWidgetBase::LayerOverlayStack, false);
            Shell->SetLayerInteractive(UGV2GameShellWidgetBase::LayerModalStack, true);

            // Only top modal in modal stack is interactive
            for (int32 i = 0; i < Plan.Modals.Num(); ++i)
            {
                const bool bIsTopModal = (i == Plan.Modals.Num() - 1);
                if (Plan.Modals[i] != nullptr)
                {
                    Plan.Modals[i]->SetIsEnabled(bIsTopModal);
                }
            }
        }
        else
        {
            for (const FName& Layer : UGV2GameShellWidgetBase::GetApprovedLayers())
            {
                Shell->SetLayerInteractive(Layer, true);
            }
        }
    }

    return true;
}

bool FGV2LayeredUiReconciler::Reconcile(
    UGV2GameShellWidgetBase* Shell,
    const FGV2UiDocumentViewModel& Document,
    FScreenFactory ScreenFactory,
    FString& OutError,
    TFunction<bool(const FString& ScreenId, const FString& PropertyPath)> ScreenCommitFailureInjector,
    TFunction<bool(const FString& ScreenId, const FString& PropertyPath)> ScreenRollbackFailureInjector,
    const FGV2PresentationPrepareContext* PrepareContext)
{
    FPreparedReconciliationPlan Plan;
    if (!PrepareReconcile(Shell, Document, ScreenFactory, Plan, OutError, PrepareContext))
    {
        return false;
    }
    if (CommitReconcile(Shell, Plan, OutError, ScreenCommitFailureInjector, ScreenRollbackFailureInjector))
    {
        // PAH-07 (ADR-0042, INV-P4): the only place a document is ever fully committed --
        // this is the data a catastrophic rebuild replays, not a copy of the panel tree
        // CommitReconcile happened to produce from it.
        LastCommittedDocument = Document;
        Health = EGV2PresentationHealth::Nominal;
        return true;
    }

    // PAH-07: a Commit failure whose own compensating rollback (GBH-10, ADR-0041) ALSO
    // failed leaves the physical tree's relationship to ActiveScreens undefined -- quick,
    // in-place inverse-mutation recovery already tried and failed inside CommitReconcile.
    // Fall back to catastrophic recovery. An ordinary Commit failure whose rollback
    // succeeded does NOT reach this branch (OutError has no rollback-failed marker) --
    // Health stays at whatever it already was, matching "the previous revision, physically
    // and accounting-wise untouched".
    if (OutError.Contains(GGV2UiRollbackFailedDiagnosticCode))
    {
        PerformCatastrophicRecovery(Shell, ScreenFactory);
    }
    return false;
}

void FGV2LayeredUiReconciler::PerformCatastrophicRecovery(UGV2GameShellWidgetBase* Shell, FScreenFactory ScreenFactory)
{
    if (Shell != nullptr)
    {
        Shell->ClearAllLayers();
    }
    ActiveScreens.Reset();

    if (!LastCommittedDocument.IsSet())
    {
        // No prior successful commit exists in this session to recover to (the very first
        // document a session ever applies failed its own rollback) -- the tree is now
        // genuinely, correctly empty rather than in an undefined state, but there is
        // nothing further to rebuild.
        Health = EGV2PresentationHealth::CatastrophicRecoveryFailed;
        return;
    }

    // Deliberately PrepareReconcile+CommitReconcile, not a recursive Reconcile() call: no
    // injector is forwarded (production behavior even when THIS recovery was triggered by
    // a test injector on the original candidate), and recursing into Reconcile() would
    // risk re-entering this same recovery path if the replay somehow failed the same way.
    FString RecoveryError;
    FPreparedReconciliationPlan RecoveryPlan;
    if (!PrepareReconcile(Shell, *LastCommittedDocument, ScreenFactory, RecoveryPlan, RecoveryError)
        || !CommitReconcile(Shell, RecoveryPlan, RecoveryError))
    {
        UE_LOG(LogTemp, Error,
            TEXT("PAH-07: catastrophic recovery failed reapplying the last committed document: %s"),
            *RecoveryError);
        Health = EGV2PresentationHealth::CatastrophicRecoveryFailed;
        return;
    }

    Health = EGV2PresentationHealth::RecoveredFromCatastrophicFailure;
}

UGV2ScreenWidgetBase* FGV2LayeredUiReconciler::GetActiveScreen(FName Layer, FName InstanceKey) const
{
    const FScreenSlotKey Key{Layer, InstanceKey};
    const FActiveScreenEntry* Found = ActiveScreens.Find(Key);
    return Found != nullptr ? Found->Widget.Get() : nullptr;
}

void FGV2LayeredUiReconciler::Reset()
{
    ActiveScreens.Reset();
    LastCommittedDocument.Reset();
    Health = EGV2PresentationHealth::Nominal;
}
