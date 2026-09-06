#include "UI/GV2LayeredUiReconciler.h"

#include "Components/Widget.h"
#include "UI/GV2GameShellWidgetBase.h"
#include "UI/GV2ScreenWidgetBase.h"

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
            PreparedInst.TargetWidget = ScreenFactory(Instance.ScreenId);
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
                OutPlan.ScreensToDetach.Add(Pair.Value.Widget);
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

    // 3. Attach new (already fully committed) screens to their layers in Shell.
    // GBH-01: every *predictable* cause of AttachScreenToLayer returning false (null
    // widget, invalid layer name, missing authored host) has already been rejected in
    // PrepareReconcile, before any mutation above -- on a plan that reached this point,
    // Attach is an invariant-level operation. A false here is therefore necessarily an
    // unpredictable engine-level failure (e.g. AddChild rejecting the child for a reason
    // Prepare cannot dry-run). GBH-10 (ADR-0041): recovery is no longer merely "delegated"
    // -- a failure here undoes every new screen already attached earlier in this same
    // step, re-attaches (best-effort) any old widget step 2 detached to make room for a
    // replacement, and rolls back step 1's property commits for every screen in the plan
    // (all of them physically changed in step 1, regardless of where attach fails), so
    // CommitReconcile returns false with the Shell tree, every screen's properties and
    // ActiveScreens (not yet touched) exactly at the previous revision.
    for (int32 InstIndex = 0; InstIndex < Plan.ScreensToUpdateOrAttach.Num(); ++InstIndex)
    {
        const FPreparedScreenInstance& Inst = Plan.ScreensToUpdateOrAttach[InstIndex];
        if (!Inst.bIsReuse && Shell != nullptr)
        {
            if (!Shell->AttachScreenToLayer(Inst.Layer, Inst.TargetWidget.Get()))
            {
                bool bStructureRestoreFailed = false;
                for (int32 RollbackIndex = InstIndex - 1; RollbackIndex >= 0; --RollbackIndex)
                {
                    const FPreparedScreenInstance& AttachedInst = Plan.ScreensToUpdateOrAttach[RollbackIndex];
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

    // 4. Detach removed screens that are no longer present in document (best-effort
    // cleanup, same reasoning as step 2).
    for (const TObjectPtr<UGV2ScreenWidgetBase>& RemovedWidget : Plan.ScreensToDetach)
    {
        if (RemovedWidget != nullptr && Shell != nullptr)
        {
            if (!Shell->DetachScreen(RemovedWidget.Get()))
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("CommitReconcile: DetachScreen(removed) returned false -- widget had no parent"));
            }
        }
    }

    // 5. Commit active screens map -- only reached once every screen above committed
    // and attached cleanly.
    ActiveScreens = Plan.NewActiveScreens;

    // 6. Layer Rules & Modal Interactivity (UIF-20)
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
