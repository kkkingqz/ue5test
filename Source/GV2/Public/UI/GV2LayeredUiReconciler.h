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

    struct FPreparedReconciliationPlan
    {
        TArray<FPreparedScreenInstance> ScreensToUpdateOrAttach;
        TArray<TObjectPtr<UGV2ScreenWidgetBase>> ScreensToDetach;
        TMap<FScreenSlotKey, FActiveScreenEntry> NewActiveScreens;
        bool bHasModals = false;
        TArray<TObjectPtr<UGV2ScreenWidgetBase>> Modals; // In modal_stack order
    };

    using FScreenFactory = TFunctionRef<UGV2ScreenWidgetBase*(const FString& ScreenId)>;

    // UPP-28: Prepares the complete reconciliation plan for every layer and screen
    // before touching any widget or mutating Game Shell. If any screen fails preparation,
    // this returns false, leaving all active screens, widgets, and shell untouched.
    bool PrepareReconcile(
        UGV2GameShellWidgetBase* Shell,
        const FGV2UiDocumentViewModel& Document,
        FScreenFactory ScreenFactory,
        FPreparedReconciliationPlan& OutPlan,
        FString& OutError) const;

    // UPP-28: Commits a cleanly prepared reconciliation plan to the Game Shell and active widgets.
    bool CommitReconcile(
        UGV2GameShellWidgetBase* Shell,
        const FPreparedReconciliationPlan& Plan);

    // Full atomic reconciliation: Prepare + Commit.
    bool Reconcile(
        UGV2GameShellWidgetBase* Shell,
        const FGV2UiDocumentViewModel& Document,
        FScreenFactory ScreenFactory,
        FString& OutError);

    UGV2ScreenWidgetBase* GetActiveScreen(FName Layer, FName InstanceKey) const;
    const TMap<FScreenSlotKey, FActiveScreenEntry>& GetActiveScreens() const { return ActiveScreens; }
    void Reset();

private:
    TMap<FScreenSlotKey, FActiveScreenEntry> ActiveScreens;
};
