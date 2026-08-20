#include "UI/GV2LayeredUiReconciler.h"

#include "UI/GV2GameShellWidgetBase.h"
#include "UI/GV2ScreenWidgetBase.h"

bool FGV2LayeredUiReconciler::Reconcile(
    UGV2GameShellWidgetBase* Shell,
    const FGV2UiDocumentViewModel& Document,
    FScreenFactory ScreenFactory,
    FString& OutError)
{
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
            OutError = FString::Printf(TEXT("Duplicate instance key '%s' in layer '%s'"), *Instance.InstanceKey.ToString(), *Instance.Layer.ToString());
            return false;
        }
        IncomingKeys.Add(Key);
    }

    // 2. Application phase
    TMap<FScreenSlotKey, FActiveScreenEntry> NewActiveScreens;

    for (const FGV2ScreenInstanceViewModel& Instance : IncomingInstances)
    {
        const FScreenSlotKey Key{Instance.Layer, Instance.InstanceKey};
        FActiveScreenEntry* Existing = ActiveScreens.Find(Key);

        UGV2ScreenWidgetBase* TargetWidget = nullptr;
        if (Existing != nullptr && Existing->ScreenId == Instance.ScreenId && Existing->Widget != nullptr)
        {
            // Reuse existing widget instance (preserving UI-local state)
            TargetWidget = Existing->Widget;
        }
        else
        {
            if (Existing != nullptr && Existing->Widget != nullptr && Shell != nullptr)
            {
                Shell->DetachScreen(Existing->Widget);
            }
            TargetWidget = ScreenFactory(Instance.ScreenId);
            if (TargetWidget == nullptr)
            {
                OutError = FString::Printf(TEXT("Failed to instantiate screen widget for screen_id '%s'"), *Instance.ScreenId);
                return false;
            }
            if (Shell != nullptr)
            {
                Shell->AttachScreenToLayer(Instance.Layer, TargetWidget);
            }
        }

        if (!TargetWidget->ApplyScreenFields(Instance.Fields))
        {
            OutError = FString::Printf(TEXT("Failed to apply fields to screen '%s'"), *Instance.ScreenId);
            return false;
        }

        NewActiveScreens.Add(Key, {Instance.ScreenId, TargetWidget});
    }

    // Detach screens that are no longer present
    for (auto& Pair : ActiveScreens)
    {
        if (!IncomingKeys.Contains(Pair.Key))
        {
            if (Shell != nullptr && Pair.Value.Widget != nullptr)
            {
                Shell->DetachScreen(Pair.Value.Widget);
            }
        }
    }

    ActiveScreens = MoveTemp(NewActiveScreens);

    // 3. Layer Rules & Modal Interactivity (UIF-20)
    if (Shell != nullptr)
    {
        const bool bHasModals = Document.Modals.Num() > 0;
        if (bHasModals)
        {
            Shell->SetLayerInteractive(UGV2GameShellWidgetBase::LayerBackground, false);
            Shell->SetLayerInteractive(UGV2GameShellWidgetBase::LayerLocationContent, false);
            Shell->SetLayerInteractive(UGV2GameShellWidgetBase::LayerCharacterPresentation, false);
            Shell->SetLayerInteractive(UGV2GameShellWidgetBase::LayerCoreInterface, false);
            Shell->SetLayerInteractive(UGV2GameShellWidgetBase::LayerOverlayStack, false);
            Shell->SetLayerInteractive(UGV2GameShellWidgetBase::LayerModalStack, true);

            // Only top modal in modal stack is interactive
            for (int32 i = 0; i < Document.Modals.Num(); ++i)
            {
                const FGV2ScreenInstanceViewModel& ModalInst = Document.Modals[i];
                const FScreenSlotKey Key{ModalInst.Layer, ModalInst.InstanceKey};
                if (const FActiveScreenEntry* Entry = ActiveScreens.Find(Key))
                {
                    const bool bIsTopModal = (i == Document.Modals.Num() - 1);
                    if (Entry->Widget != nullptr)
                    {
                        Entry->Widget->SetIsEnabled(bIsTopModal);
                    }
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
