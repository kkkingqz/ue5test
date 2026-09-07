#include "UI/GV2LayeredUiReconciler.h"

#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "UI/GV2GameShellWidgetBase.h"
#include "UI/GV2KeyedCollection.h"
#include "UI/GV2ScreenWidgetBase.h"

namespace
{
// PAH-06A: FGV2KeyedCollection::ReconcilePrepared's own Prepare/Commit item lifecycle is
// unused for modal_stack -- every widget here is already fully resolved (reused or created
// by ScreenFactory) and its fields already committed by CommitReconcile's step 1, before
// this primitive is ever called. This call exists purely for the primitive's keyed
// container-ordering and atomic-swap guarantee, so PreparedType carries no data.
struct FGV2ModalStackReconcilePrepared
{
};
}

bool FGV2LayeredUiReconciler::PrepareReconcile(
    UGV2GameShellWidgetBase* Shell,
    const FGV2UiDocumentViewModel& Document,
    FScreenFactory ScreenFactory,
    FPreparedReconciliationPlan& OutPlan,
    FString& OutError) const
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
            // Instantiate new screen widget
            PreparedInst.bIsReuse = false;
            PreparedInst.TargetWidget = ScreenFactory(Instance.ScreenId, Instance.Layer);
            if (PreparedInst.TargetWidget == nullptr)
            {
                OutError = FString::Printf(
                    TEXT("Failed to instantiate screen widget for screen_id '%s'"),
                    *Instance.ScreenId);
                return false;
            }
            if (Existing != nullptr && Existing->Widget != nullptr)
            {
                PreparedInst.ReplacedOldWidget = Existing->Widget;
            }
        }

        // Prepare screen fields (predicts any deep child failure across all field hosts).
        // DUC-11: seed the composition chain with this screen's own screen_id so a
        // nested tab (any depth below) resolving back to it -- directly or through
        // an intermediate screen -- is caught as a cycle rather than silently
        // accepted; see FGV2TabContainerTabsPropertyConsumer's own guard.
        const TArray<FString> RootCompositionChain{Instance.ScreenId};
        if (!PreparedInst.TargetWidget->PrepareScreenFields(Instance.Fields, PreparedInst.MutationPlan, OutError, &RootCompositionChain))
        {
            if (OutError.IsEmpty())
            {
                OutError = FString::Printf(TEXT("Failed to prepare fields for screen '%s'"), *Instance.ScreenId);
            }
            return false;
        }

        OutPlan.NewActiveScreens.Add(Key, {Instance.ScreenId, PreparedInst.TargetWidget});
        if (Instance.Layer == UGV2GameShellWidgetBase::LayerModalStack)
        {
            OutPlan.Modals.Add(PreparedInst.TargetWidget);
        }

        OutPlan.ScreensToUpdateOrAttach.Add(MoveTemp(PreparedInst));
    }

    // 3. Identify screens to detach (active screens not in incoming document)
    for (const auto& Pair : ActiveScreens)
    {
        if (!IncomingKeys.Contains(Pair.Key))
        {
            if (Pair.Value.Widget != nullptr)
            {
                OutPlan.ScreensToDetach.Add(FDetachEntry{Pair.Key.Layer, Pair.Value.Widget});
            }
        }
    }

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
        if (!Inst.TargetWidget->CommitScreenFields(Inst.MutationPlan, ScreenCommitError, PerScreenInjector, PerScreenRollbackInjector))
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

    // 2. Detach old screens that were replaced by a new widget instance. Best-effort
    // cleanup of a widget already being superseded, not a publish step -- a false here
    // (e.g. it somehow had no parent already) is logged, not treated as an invariant
    // violation that must halt the commit of the *new* widget replacing it.
    for (const FPreparedScreenInstance& Inst : Plan.ScreensToUpdateOrAttach)
    {
        if (!Inst.bIsReuse && Inst.ReplacedOldWidget != nullptr && Shell != nullptr)
        {
            if (!Shell->DetachScreen(Inst.ReplacedOldWidget.Get()))
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("CommitReconcile: DetachScreen(replaced) returned false for layer '%s' instance '%s' -- widget had no parent"),
                    *Inst.Layer.ToString(), *Inst.InstanceKey.ToString());
            }
        }
    }

    // 3. PAH-06A (ADR-0042, INV-P3 proof slice): modal_stack is reconciled atomically
    // through the shared FGV2KeyedCollection::ReconcilePrepared primitive instead of the
    // per-widget AddChild loop below -- desired order is the layer's own screen instances
    // in incoming-document order (Document.Modals, preserved by ScreensToUpdateOrAttach's
    // own traversal order), not the traversal order of the whole multi-layer plan. Every
    // widget here already exists and its fields are already committed (step 1 above), so
    // the seed map below always has every key the primitive could ask for -- CreateItem
    // is unreachable by construction, and returns nullptr (fail-closed) if that invariant
    // is ever violated by a future change.
    UPanelWidget* ModalStackHost = Shell != nullptr ? Shell->GetHostForLayer(UGV2GameShellWidgetBase::LayerModalStack) : nullptr;
    TArray<UGV2ScreenWidgetBase*> PreviousModalOrder;
    bool bModalStackCommitted = false;
    if (ModalStackHost != nullptr)
    {
        TArray<const FPreparedScreenInstance*> ModalInstances;
        for (const FPreparedScreenInstance& Inst : Plan.ScreensToUpdateOrAttach)
        {
            if (Inst.Layer == UGV2GameShellWidgetBase::LayerModalStack && Inst.TargetWidget != nullptr)
            {
                ModalInstances.Add(&Inst);
            }
        }

        TMap<FName, TObjectPtr<UGV2ScreenWidgetBase>> ModalSeedByKey;
        for (const FPreparedScreenInstance* Inst : ModalInstances)
        {
            ModalSeedByKey.Add(Inst->InstanceKey, Inst->TargetWidget);
        }

        TArray<UGV2ScreenWidgetBase*> ModalOrderedOut;
        const bool bModalReconciled = FGV2KeyedCollection::ReconcilePrepared<UGV2ScreenWidgetBase, const FPreparedScreenInstance*, FGV2ModalStackReconcilePrepared>(
            ModalStackHost,
            ModalInstances,
            ModalSeedByKey,
            [](const FPreparedScreenInstance* const& Inst) { return Inst->InstanceKey; },
            []() -> UGV2ScreenWidgetBase* { return nullptr; },
            [](UGV2ScreenWidgetBase&, const FPreparedScreenInstance* const&, FGV2ModalStackReconcilePrepared&) { return true; },
            [](UGV2ScreenWidgetBase&, const FGV2ModalStackReconcilePrepared&) {},
            ModalOrderedOut,
            nullptr,
            &PreviousModalOrder);

        if (!bModalReconciled)
        {
            // Same recovery shape as the attach-failure branch in step 4 below (GBH-10),
            // simplified because nothing in that step's loop has attached anything yet at
            // this point: only step 1's field commits and step 2's replaced-widget
            // detaches (both already applied for every layer, modal_stack included) need
            // undoing.
            bool bStructureRestoreFailed = false;
            for (const FPreparedScreenInstance& ReplacedInst : Plan.ScreensToUpdateOrAttach)
            {
                if (!ReplacedInst.bIsReuse && ReplacedInst.ReplacedOldWidget != nullptr)
                {
                    if (!Shell->AttachScreenToLayer(ReplacedInst.Layer, ReplacedInst.ReplacedOldWidget.Get()))
                    {
                        bStructureRestoreFailed = true;
                        UE_LOG(LogTemp, Error,
                            TEXT("GBH-10: rollback failed re-attaching replaced screen (layer='%s' instance_key='%s') after modal_stack reconcile failure -- invariant violation"),
                            *ReplacedInst.Layer.ToString(), *ReplacedInst.InstanceKey.ToString());
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

            OutError = TEXT("core:diagnostic.ui_reconcile.modal_stack_reconcile_failed");
            if (bStructureRestoreFailed)
            {
                OutError = FString::Printf(TEXT("%s: %s"), GGV2UiRollbackFailedDiagnosticCode, *OutError);
            }
            UE_LOG(LogTemp, Error, TEXT("CommitReconcile: %s"), *OutError);
            return false;
        }
        bModalStackCommitted = true;
    }

    // 4. Attach new (already fully committed) screens to every OTHER layer in Shell.
    // modal_stack is excluded -- step 3 above already reconciled it atomically.
    // GBH-01: every *predictable* cause of AttachScreenToLayer returning false (null
    // widget, invalid layer name, missing authored host) has already been rejected in
    // PrepareReconcile, before any mutation above -- on a plan that reached this point,
    // Attach is an invariant-level operation. A false here is therefore necessarily an
    // unpredictable engine-level failure (e.g. AddChild rejecting the child for a reason
    // Prepare cannot dry-run). GBH-10 (ADR-0041): recovery is no longer merely "delegated"
    // -- a failure here undoes every new screen already attached earlier in this same
    // step, restores modal_stack's order from step 3 if that step already committed one
    // (PAH-06A), re-attaches (best-effort) any old widget step 2 detached to make room for
    // a replacement, and rolls back step 1's property commits for every screen in the plan
    // (all of them physically changed in step 1, regardless of where attach fails), so
    // CommitReconcile returns false with the Shell tree, every screen's properties and
    // ActiveScreens (not yet touched) exactly at the previous revision.
    for (int32 InstIndex = 0; InstIndex < Plan.ScreensToUpdateOrAttach.Num(); ++InstIndex)
    {
        const FPreparedScreenInstance& Inst = Plan.ScreensToUpdateOrAttach[InstIndex];
        if (Inst.Layer == UGV2GameShellWidgetBase::LayerModalStack)
        {
            continue;
        }
        if (!Inst.bIsReuse && Shell != nullptr)
        {
            if (!Shell->AttachScreenToLayer(Inst.Layer, Inst.TargetWidget.Get()))
            {
                bool bStructureRestoreFailed = false;
                for (int32 RollbackIndex = InstIndex - 1; RollbackIndex >= 0; --RollbackIndex)
                {
                    const FPreparedScreenInstance& AttachedInst = Plan.ScreensToUpdateOrAttach[RollbackIndex];
                    if (AttachedInst.Layer == UGV2GameShellWidgetBase::LayerModalStack)
                    {
                        continue;
                    }
                    if (!AttachedInst.bIsReuse)
                    {
                        if (!Shell->DetachScreen(AttachedInst.TargetWidget.Get()))
                        {
                            bStructureRestoreFailed = true;
                            UE_LOG(LogTemp, Error,
                                TEXT("GBH-10: rollback failed detaching newly-attached screen '%s' (layer='%s' instance_key='%s') -- invariant violation"),
                                *AttachedInst.ScreenId, *AttachedInst.Layer.ToString(), *AttachedInst.InstanceKey.ToString());
                        }
                    }
                }
                if (bModalStackCommitted && ModalStackHost != nullptr)
                {
                    ModalStackHost->ClearChildren();
                    for (UGV2ScreenWidgetBase* PrevModalWidget : PreviousModalOrder)
                    {
                        if (PrevModalWidget != nullptr && ModalStackHost->AddChild(PrevModalWidget) == nullptr)
                        {
                            bStructureRestoreFailed = true;
                            UE_LOG(LogTemp, Error,
                                TEXT("GBH-10: rollback failed restoring modal_stack order after a later layer's attach failure -- invariant violation"));
                        }
                    }
                }
                for (const FPreparedScreenInstance& ReplacedInst : Plan.ScreensToUpdateOrAttach)
                {
                    if (!ReplacedInst.bIsReuse && ReplacedInst.ReplacedOldWidget != nullptr)
                    {
                        if (!Shell->AttachScreenToLayer(ReplacedInst.Layer, ReplacedInst.ReplacedOldWidget.Get()))
                        {
                            bStructureRestoreFailed = true;
                            UE_LOG(LogTemp, Error,
                                TEXT("GBH-10: rollback failed re-attaching replaced screen (layer='%s' instance_key='%s') detached in step 2 -- invariant violation"),
                                *ReplacedInst.Layer.ToString(), *ReplacedInst.InstanceKey.ToString());
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

                OutError = FString::Printf(
                    TEXT("core:diagnostic.ui_reconcile.attach_failed: layer='%s' instance_key='%s' screen_id='%s'"),
                    *Inst.Layer.ToString(), *Inst.InstanceKey.ToString(), *Inst.ScreenId);
                if (bStructureRestoreFailed)
                {
                    OutError = FString::Printf(TEXT("%s: %s"), GGV2UiRollbackFailedDiagnosticCode, *OutError);
                }
                UE_LOG(LogTemp, Error, TEXT("CommitReconcile: %s"), *OutError);
                return false;
            }
        }
    }

    // 5. Detach removed screens that are no longer present in document (best-effort
    // cleanup, same reasoning as step 2). modal_stack entries are skipped: step 3's
    // ReconcilePrepared commit already dropped them from the container atomically via its
    // own ClearChildren, so calling DetachScreen on them here would just log a spurious
    // "no parent" warning.
    for (const FDetachEntry& RemovedEntry : Plan.ScreensToDetach)
    {
        if (RemovedEntry.Layer == UGV2GameShellWidgetBase::LayerModalStack)
        {
            continue;
        }
        if (RemovedEntry.Widget != nullptr && Shell != nullptr)
        {
            if (!Shell->DetachScreen(RemovedEntry.Widget.Get()))
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("CommitReconcile: DetachScreen(removed) returned false -- widget had no parent"));
            }
        }
    }

    // 6. Commit active screens map -- only reached once every screen above committed
    // and attached cleanly.
    ActiveScreens = Plan.NewActiveScreens;

    // 7. Layer Rules & Modal Interactivity (UIF-20)
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
    TFunction<bool(const FString& ScreenId, const FString& PropertyPath)> ScreenRollbackFailureInjector)
{
    FPreparedReconciliationPlan Plan;
    if (!PrepareReconcile(Shell, Document, ScreenFactory, Plan, OutError))
    {
        return false;
    }
    return CommitReconcile(Shell, Plan, OutError, ScreenCommitFailureInjector, ScreenRollbackFailureInjector);
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
}
