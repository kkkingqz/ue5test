#pragma once

#include "CommonUserWidget.h"
#include "Components/PanelWidget.h"
#include "UI/GV2KeyedCollection.h"
#include "UI/GV2UiStyleConsumer.h"
#include "GV2ListViewWidgetBase.generated.h"

class UPanelWidget;

/**
 * UGV2ListViewWidgetBase (UIF-13, UIH-01, ADR-0035)
 * Generalized list container supporting vertical, horizontal, or wrap layout.
 * Reconciles child widgets by unique non-empty keys using FGV2KeyedCollection.
 * Transactional: fails without mutating the live UI if keys are invalid,
 * widget creation fails, or item apply fails.
 */
UCLASS(Blueprintable)
class GV2_API UGV2ListViewWidgetBase
    : public UCommonUserWidget
    , public IGV2UiStyleConsumer
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "GV2|UI|ListView")
    EOrientation GetOrientation() const { return Orientation; }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|ListView")
    void SetOrientation(EOrientation InOrientation);

    UFUNCTION(BlueprintPure, Category = "GV2|UI|ListView")
    UPanelWidget* GetContainerPanel() const { return ContainerPanel; }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|ListView")
    void SetContainerPanel(UPanelWidget* InContainerPanel);

    UFUNCTION(BlueprintPure, Category = "GV2|UI|ListView")
    int32 GetEntryCount() const { return ActiveWidgetsByKey.Num(); }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|ListView")
    UWidget* GetEntryWidget(FName Key) const { return ActiveWidgetsByKey.FindRef(Key); }

    template <typename WidgetType = UWidget>
    WidgetType* GetEntry(FName Key) const
    {
        return Cast<WidgetType>(ActiveWidgetsByKey.FindRef(Key));
    }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|ListView")
    TArray<UWidget*> GetOrderedEntries() const;

    UFUNCTION(BlueprintPure, Category = "GV2|UI|ListView")
    TArray<FName> GetActiveKeys() const;

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|ListView")
    void ClearEntries();

    virtual bool ApplyCentralStyle_Implementation() override;

    /**
     * Transactionally reconciles entries into the container panel.
     * Validates all keys (non-empty, unique), reuses existing widgets,
     * applies models, and updates children only if every step succeeds.
     */
    template <typename WidgetType, typename ModelType>
    bool ReconcileEntries(
        TConstArrayView<ModelType> Models,
        TFunctionRef<FName(const ModelType&)> GetKey,
        TFunctionRef<WidgetType*()> CreateItem,
        TFunctionRef<bool(WidgetType&, const ModelType&)> ApplyItem)
    {
        if (ContainerPanel == nullptr)
        {
            return false;
        }

        // 1. Validation phase (keys must be valid and unique)
        TSet<FName> Keys;
        for (const ModelType& Model : Models)
        {
            const FName Key = GetKey(Model);
            if (Key.IsNone() || Keys.Contains(Key))
            {
                return false;
            }
            Keys.Add(Key);
        }

        // 2. Stage candidate widgets (reuse existing or create new)
        TMap<FName, TObjectPtr<UWidget>> CandidateByKey;
        TArray<WidgetType*> StagedWidgets;
        StagedWidgets.Reserve(Models.Num());

        for (const ModelType& Model : Models)
        {
            const FName Key = GetKey(Model);
            WidgetType* Widget = Cast<WidgetType>(ActiveWidgetsByKey.FindRef(Key));
            if (Widget == nullptr)
            {
                Widget = CreateItem();
                if (Widget == nullptr)
                {
                    return false;
                }
            }

            CandidateByKey.Add(Key, Widget);
            StagedWidgets.Add(Widget);
        }

        // 3. Apply items to widgets (transactional: abort if any apply returns false)
        for (int32 Index = 0; Index < Models.Num(); ++Index)
        {
            if (!ApplyItem(*StagedWidgets[Index], Models[Index]))
            {
                return false;
            }
        }

        // 4. Commit to container panel and active map
        ContainerPanel->ClearChildren();
        for (WidgetType* Widget : StagedWidgets)
        {
            if (ContainerPanel->AddChild(Widget) == nullptr)
            {
                return false;
            }
        }

        ActiveWidgetsByKey = MoveTemp(CandidateByKey);
        return true;
    }

protected:
    virtual void NativePreConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> ContainerPanel;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|ListView")
    TEnumAsByte<EOrientation> Orientation = Orient_Vertical;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UWidget>> ActiveWidgetsByKey;
};
