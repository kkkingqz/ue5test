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
        TFunctionRef<void(WidgetType&, const ModelType&)> ApplyItem,
        TArray<WidgetType*>& OutOrderedWidgets)
    {
        if (Container == nullptr) return false;

#if !UE_BUILD_SHIPPING
        const int32 PreviousWidgetCount = InOutWidgetsByKey.Num();
        int32 ReusedInRevision = 0;
        int32 CreatedInRevision = 0;
#endif

        TMap<FName, TObjectPtr<WidgetType>> CandidateByKey;
        OutOrderedWidgets.Reset(Models.Num());
        for (const ModelType& Model : Models)
        {
            const FName Key = GetKey(Model);
            if (Key.IsNone() || CandidateByKey.Contains(Key)) return false;
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
            OutOrderedWidgets.Add(Widget);
        }

        for (int32 Index = 0; Index < Models.Num(); ++Index)
        {
            ApplyItem(*OutOrderedWidgets[Index], Models[Index]);
        }
        Container->ClearChildren();
        for (WidgetType* Widget : OutOrderedWidgets)
        {
            if (Container->AddChild(Widget) == nullptr) return false;
        }
        InOutWidgetsByKey = MoveTemp(CandidateByKey);

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
