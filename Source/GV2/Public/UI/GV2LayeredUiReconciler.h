#pragma once

#include "CoreMinimal.h"
#include "Bridge/GV2BridgeTypes.h"

class UGV2GameShellWidgetBase;
class UGV2ScreenWidgetBase;

/**
 * FGV2LayeredUiReconciler (UIF-19, UIF-20)
 * Reconciles active UI widgets across Game Shell layers based on document envelopes.
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

    using FScreenFactory = TFunctionRef<UGV2ScreenWidgetBase*(const FString& ScreenId)>;

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
