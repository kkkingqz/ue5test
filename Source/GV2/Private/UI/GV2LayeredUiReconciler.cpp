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

        // Prepare screen fields (predicts any deep child failure across all field hosts)
        if (!PreparedInst.TargetWidget->PrepareScreenFields(Instance.Fields, PreparedInst.MutationPlan, OutError))
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

bool FGV2LayeredUiReconciler::CommitReconcile(
    UGV2GameShellWidgetBase* Shell,
    const FPreparedReconciliationPlan& Plan,
    FString& OutError,
    TFunction<bool(const FString& ScreenId, const FString& PropertyPath)> ScreenCommitFailureInjector)
{
    OutError.Reset();

    // 1. Detach old screens that were replaced by a new widget instance. Best-effort
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

    // 2+3. Attach then commit each screen. ADR-0040: an unexpected Commit-phase failure
    // is an invariant violation, not an ordinary `false` -- the failed screen must not be
    // published and the previous revision (ActiveScreens, unchanged below) stays active.
    // PCC-06: stop the traversal on the first failure instead of continuing to the
    // remaining screens (steps 4/5/6 are skipped entirely on this path).
    for (const FPreparedScreenInstance& Inst : Plan.ScreensToUpdateOrAttach)
    {
        if (!Inst.bIsReuse && Shell != nullptr)
        {
            if (!Shell->AttachScreenToLayer(Inst.Layer, Inst.TargetWidget.Get()))
            {
                OutError = FString::Printf(
                    TEXT("core:diagnostic.ui_reconcile.attach_failed: layer='%s' instance_key='%s' screen_id='%s'"),
                    *Inst.Layer.ToString(), *Inst.InstanceKey.ToString(), *Inst.ScreenId);
                UE_LOG(LogTemp, Error, TEXT("CommitReconcile: %s"), *OutError);
                return false;
            }
        }

        if (Inst.TargetWidget != nullptr)
        {
            TFunction<bool(const FString&)> PerScreenInjector = nullptr;
            if (ScreenCommitFailureInjector)
            {
                const FString ScreenId = Inst.ScreenId;
                PerScreenInjector = [ScreenCommitFailureInjector, ScreenId](const FString& PropertyPath)
                {
                    return ScreenCommitFailureInjector(ScreenId, PropertyPath);
                };
            }
            if (!Inst.TargetWidget->CommitScreenFields(Inst.MutationPlan, PerScreenInjector))
            {
                OutError = FString::Printf(
                    TEXT("core:diagnostic.ui_reconcile.commit_failed: layer='%s' instance_key='%s' screen_id='%s'"),
                    *Inst.Layer.ToString(), *Inst.InstanceKey.ToString(), *Inst.ScreenId);
                UE_LOG(LogTemp, Error, TEXT("CommitReconcile: %s"), *OutError);
                return false;
            }
        }
    }

    // 4. Detach removed screens that are no longer present in document (best-effort
    // cleanup, same reasoning as step 1).
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

    // 5. Commit active screens map -- only reached once every screen above committed cleanly.
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
    TFunction<bool(const FString& ScreenId, const FString& PropertyPath)> ScreenCommitFailureInjector)
{
    FPreparedReconciliationPlan Plan;
    if (!PrepareReconcile(Shell, Document, ScreenFactory, Plan, OutError))
    {
        return false;
    }
    return CommitReconcile(Shell, Plan, OutError, ScreenCommitFailureInjector);
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
