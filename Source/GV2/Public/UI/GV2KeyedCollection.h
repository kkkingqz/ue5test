#pragma once

#include "Components/PanelWidget.h"

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

        TMap<FName, TObjectPtr<WidgetType>> CandidateByKey;
        OutOrderedWidgets.Reset(Models.Num());
        for (const ModelType& Model : Models)
        {
            const FName Key = GetKey(Model);
            if (Key.IsNone() || CandidateByKey.Contains(Key)) return false;
            WidgetType* Widget = InOutWidgetsByKey.FindRef(Key);
            if (Widget == nullptr) Widget = CreateItem();
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
        return true;
    }
};
