#pragma once

#include "Components/PanelWidget.h"
#include "Logging/LogMacros.h"

#if !UE_BUILD_SHIPPING
struct FGV2ContainerReuseStats
{
    int32 TotalReused = 0;
    int32 TotalCreated = 0;
    int32 ConsecutiveZeroReuseRevisions = 0;
};
#endif

/**
 * FGV2KeyedCollection
 *
 * PSC-11 (ADR-0043 D2): moved into GV2PresentationApply. Keyed reconciliation is physical
 * application -- it matches existing child widgets to incoming keys and rebuilds a
 * UPanelWidget's children -- and it never depended on a GV2 type to do it. Living above the
 * boundary only meant the upper module still owned a piece of the physical layer.
 * Reconciles collection elements into a UPanelWidget container matching elements by stable FName keys.
 *
 * Atomicity and rollback guarantees:
 * 1. Key validation and CanApplyItem preflight: Fully atomic. Rejects before creating any widgets
 *    or preparing any existing widget.
 * 2. Widget creation: If CreateItem fails (returns nullptr), reconciliation aborts before calling PrepareItem.
 * 3. Two-phase Prepare/Commit: PrepareItem prepares all element models off-tree without mutating widgets.
 *    If any individual item prepare fails, reconciliation aborts immediately with NO widget mutated.
 * 4. Container hierarchy: Committed atomically. Child additions/removals only occur after all items are committed.
 */
class FGV2KeyedCollection
{
public:
    /**
     * PSC-11 (ADR-0043 D2): physical projection recovery -- put a panel's children back into
     * a known order. It is the same rebuild the container swap below performs for itself when
     * an AddChild fails inside one call; a caller whose wider transaction spans SEVERAL
     * ReconcilePrepared calls (one per Game Shell layer) has to undo an already-committed
     * call when a later sibling fails, and doing that with its own ClearChildren/AddChild
     * loop would leave one piece of physical projection in the upper module.
     *
     * Returns false if any child could not be re-attached -- an invariant violation the
     * caller reports; there is nothing further this level can do about it.
     */
    template <typename WidgetType>
    static bool RestoreOrder(
        UPanelWidget* Container,
        const TArray<WidgetType*>& OrderedChildren,
        TFunction<void(UPanelSlot&)> ConfigureSlot = nullptr)
    {
        if (Container == nullptr)
        {
            return false;
        }
        Container->ClearChildren();
        bool bRestored = true;
        for (WidgetType* Child : OrderedChildren)
        {
            if (Child == nullptr)
            {
                continue;
            }
            UPanelSlot* RestoredSlot = Container->AddChild(Child);
            if (RestoredSlot == nullptr)
            {
                bRestored = false;
            }
            else if (ConfigureSlot)
            {
                ConfigureSlot(*RestoredSlot);
            }
        }
        return bRestored;
    }

    template <typename WidgetType, typename ModelType, typename PreparedType>
    static bool ReconcilePrepared(
        UPanelWidget* Container,
        TConstArrayView<ModelType> Models,
        TMap<FName, TObjectPtr<WidgetType>>& InOutWidgetsByKey,
        TFunctionRef<FName(const ModelType&)> GetKey,
        TFunctionRef<WidgetType*()> CreateItem,
        TFunctionRef<bool(WidgetType&, const ModelType&, PreparedType&)> PrepareItem,
        TFunctionRef<void(WidgetType&, const PreparedType&)> CommitItem,
        TArray<WidgetType*>& OutOrderedWidgets,
        TFunction<bool(const ModelType&)> CanApplyItem = nullptr,
        // PAH-06A (ADR-0042, INV-P3): the container swap below already restores this
        // panel to its exact prior children on an AddChild failure *within this call*
        // (see the restore loop a few lines down) -- but once this call RETURNS true,
        // that prior order is gone from its scope. A caller whose own wider transaction
        // spans multiple such calls (e.g. one call per Game Shell layer) and needs to
        // undo THIS call's already-committed reorder because a LATER, sibling call in
        // that same transaction failed has no way to recover what this call replaced.
        // Optional and additive: every existing call site is unaffected.
        TArray<WidgetType*>* OutPreviousOrderedWidgets = nullptr,
        // PSC-14 / PSC-AF-02: AddChild creates a fresh panel slot. A keyed container
        // rebuild must reapply caller-owned layout policy to that new slot in the same
        // physical commit; otherwise an Overlay silently falls back to Left/Top even
        // when the widget was Fill before ClearChildren. The callback is also used by
        // rollback restoration below, so success and recovery have the same projection.
        TFunction<void(UPanelSlot&)> ConfigureSlot = nullptr)
    {
        if (Container == nullptr) return false;

        // 1. Validation phase (keys non-empty, unique, can apply)
        TSet<FName> Keys;
        for (const ModelType& Model : Models)
        {
            const FName Key = GetKey(Model);
            if (Key.IsNone() || Keys.Contains(Key)) return false;
            if (CanApplyItem && !CanApplyItem(Model)) return false;
            Keys.Add(Key);
        }

#if !UE_BUILD_SHIPPING
        const int32 PreviousWidgetCount = InOutWidgetsByKey.Num();
        int32 ReusedInRevision = 0;
        int32 CreatedInRevision = 0;
#endif

        TMap<FName, TObjectPtr<WidgetType>> CandidateByKey;
        TArray<WidgetType*> TempOrderedWidgets;
        TempOrderedWidgets.Reset(Models.Num());
        for (const ModelType& Model : Models)
        {
            const FName Key = GetKey(Model);
            WidgetType* Widget = InOutWidgetsByKey.FindRef(Key);
            if (Widget == nullptr)
            {
                Widget = CreateItem();
#if !UE_BUILD_SHIPPING
                ++CreatedInRevision;
#endif
            }
            else
            {
#if !UE_BUILD_SHIPPING
                ++ReusedInRevision;
#endif
            }
            if (Widget == nullptr) return false;
            CandidateByKey.Add(Key, Widget);
            TempOrderedWidgets.Add(Widget);
        }

        // 2. Prepare items into intermediate storage without mutating widgets
        TArray<PreparedType> PreparedData;
        PreparedData.SetNum(Models.Num());
        for (int32 Index = 0; Index < Models.Num(); ++Index)
        {
            if (!PrepareItem(*TempOrderedWidgets[Index], Models[Index], PreparedData[Index]))
            {
                return false;
            }
        }

        // 3. Infallible Commit items
        for (int32 Index = 0; Index < Models.Num(); ++Index)
        {
            CommitItem(*TempOrderedWidgets[Index], PreparedData[Index]);
        }

        // Commit to Container atomically
        const TArray<UWidget*> PreviousChildren = Container->GetAllChildren();
        if (OutPreviousOrderedWidgets != nullptr)
        {
            OutPreviousOrderedWidgets->Reset(PreviousChildren.Num());
            for (UWidget* Prev : PreviousChildren)
            {
                if (WidgetType* TypedPrev = Cast<WidgetType>(Prev))
                {
                    OutPreviousOrderedWidgets->Add(TypedPrev);
                }
            }
        }
        Container->ClearChildren();
        for (WidgetType* Widget : TempOrderedWidgets)
        {
            UPanelSlot* NewSlot = Container->AddChild(Widget);
            if (NewSlot == nullptr)
            {
                RestoreOrder(Container, PreviousChildren, ConfigureSlot);
                return false;
            }
            if (ConfigureSlot)
            {
                ConfigureSlot(*NewSlot);
            }
        }
        InOutWidgetsByKey = MoveTemp(CandidateByKey);
        OutOrderedWidgets = MoveTemp(TempOrderedWidgets);

#if !UE_BUILD_SHIPPING
        if (Models.Num() > 0 && PreviousWidgetCount > 0)
        {
            static TMap<TWeakObjectPtr<UPanelWidget>, FGV2ContainerReuseStats> ContainerStats;
            FGV2ContainerReuseStats& Stats = ContainerStats.FindOrAdd(Container);
            Stats.TotalReused += ReusedInRevision;
            Stats.TotalCreated += CreatedInRevision;
            if (ReusedInRevision == 0)
            {
                ++Stats.ConsecutiveZeroReuseRevisions;
                constexpr int32 WarnThreshold = 3;
                if (Stats.ConsecutiveZeroReuseRevisions >= WarnThreshold)
                {
                    UE_LOG(
                        LogTemp,
                        Warning,
                        TEXT("Container '%s' has performed %d consecutive reconciliations without reusing any children (created %d, reused 0)"),
                        *Container->GetPathName(),
                        Stats.ConsecutiveZeroReuseRevisions,
                        CreatedInRevision);
                }
            }
            else
            {
                Stats.ConsecutiveZeroReuseRevisions = 0;
            }
        }
#endif

        return true;
    }
    template <typename WidgetType, typename ModelType>
    static bool Reconcile(
        UPanelWidget* Container,
        TConstArrayView<ModelType> Models,
        TMap<FName, TObjectPtr<WidgetType>>& InOutWidgetsByKey,
        TFunctionRef<FName(const ModelType&)> GetKey,
        TFunctionRef<WidgetType*()> CreateItem,
        TFunctionRef<bool(WidgetType&, const ModelType&)> ApplyItem,
        TArray<WidgetType*>& OutOrderedWidgets,
        TFunction<bool(const ModelType&)> CanApplyItem = nullptr,
        TFunction<void(UPanelSlot&)> ConfigureSlot = nullptr)
    {
        if (Container == nullptr) return false;

        // 1. Validation phase (keys non-empty, unique, can apply)
        TSet<FName> Keys;
        for (const ModelType& Model : Models)
        {
            const FName Key = GetKey(Model);
            if (Key.IsNone() || Keys.Contains(Key)) return false;
            if (CanApplyItem && !CanApplyItem(Model)) return false;
            Keys.Add(Key);
        }

#if !UE_BUILD_SHIPPING
        const int32 PreviousWidgetCount = InOutWidgetsByKey.Num();
        int32 ReusedInRevision = 0;
        int32 CreatedInRevision = 0;
#endif

        TMap<FName, TObjectPtr<WidgetType>> CandidateByKey;
        TArray<WidgetType*> TempOrderedWidgets;
        TempOrderedWidgets.Reset(Models.Num());
        for (const ModelType& Model : Models)
        {
            const FName Key = GetKey(Model);
            WidgetType* Widget = InOutWidgetsByKey.FindRef(Key);
            if (Widget == nullptr)
            {
                Widget = CreateItem();
#if !UE_BUILD_SHIPPING
                ++CreatedInRevision;
#endif
            }
            else
            {
#if !UE_BUILD_SHIPPING
                ++ReusedInRevision;
#endif
            }
            if (Widget == nullptr) return false;
            CandidateByKey.Add(Key, Widget);
            TempOrderedWidgets.Add(Widget);
        }

        // Apply items to widgets
        for (int32 Index = 0; Index < Models.Num(); ++Index)
        {
            if (!ApplyItem(*TempOrderedWidgets[Index], Models[Index]))
            {
                return false;
            }
        }

        // Commit to Container atomically
        const TArray<UWidget*> PreviousChildren = Container->GetAllChildren();
        Container->ClearChildren();
        for (WidgetType* Widget : TempOrderedWidgets)
        {
            UPanelSlot* NewSlot = Container->AddChild(Widget);
            if (NewSlot == nullptr)
            {
                RestoreOrder(Container, PreviousChildren, ConfigureSlot);
                return false;
            }
            if (ConfigureSlot)
            {
                ConfigureSlot(*NewSlot);
            }
        }
        InOutWidgetsByKey = MoveTemp(CandidateByKey);
        OutOrderedWidgets = MoveTemp(TempOrderedWidgets);

#if !UE_BUILD_SHIPPING
        if (Models.Num() > 0 && PreviousWidgetCount > 0)
        {
            static TMap<TWeakObjectPtr<UPanelWidget>, FGV2ContainerReuseStats> ContainerStats;
            FGV2ContainerReuseStats& Stats = ContainerStats.FindOrAdd(Container);
            Stats.TotalReused += ReusedInRevision;
            Stats.TotalCreated += CreatedInRevision;
            if (ReusedInRevision == 0)
            {
                ++Stats.ConsecutiveZeroReuseRevisions;
                constexpr int32 WarnThreshold = 3;
                if (Stats.ConsecutiveZeroReuseRevisions >= WarnThreshold)
                {
                    UE_LOG(
                        LogTemp,
                        Warning,
                        TEXT("Container '%s' has performed %d consecutive reconciliations without reusing any children (created %d, reused 0)"),
                        *Container->GetPathName(),
                        Stats.ConsecutiveZeroReuseRevisions,
                        CreatedInRevision);
                }
            }
            else
            {
                Stats.ConsecutiveZeroReuseRevisions = 0;
            }
        }
#endif

        return true;
    }
};
