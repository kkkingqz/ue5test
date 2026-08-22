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

class FGV2KeyedCollection
{
public:
    template <typename WidgetType, typename ModelType>
    static bool Reconcile(
        UPanelWidget* Container,
        TConstArrayView<ModelType> Models,
        TMap<FName, TObjectPtr<WidgetType>>& InOutWidgetsByKey,
        TFunctionRef<FName(const ModelType&)> GetKey,
        TFunctionRef<WidgetType*()> CreateItem,
        TFunctionRef<bool(WidgetType&, const ModelType&)> ApplyItem,
        TArray<WidgetType*>& OutOrderedWidgets,
        TFunction<bool(const ModelType&)> CanApplyItem = nullptr)
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
            if (Container->AddChild(Widget) == nullptr)
            {
                Container->ClearChildren();
                for (UWidget* Prev : PreviousChildren)
                {
                    Container->AddChild(Prev);
                }
                return false;
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
