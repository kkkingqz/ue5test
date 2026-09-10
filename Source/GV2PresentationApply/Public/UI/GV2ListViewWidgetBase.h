#pragma once

#include "GV2PresentationApply/PreparedApplyTargets.h"
#include "CommonUserWidget.h"
#include "Components/PanelWidget.h"
#include "GV2PresentationApply/PreparedKeyedCollection.h"
#include "UI/GV2UiPropertyHost.h"
#include "GV2ListViewWidgetBase.generated.h"

class UPanelWidget;

/**
 * UGV2ListViewWidgetBase (UIF-13, UIH-01, ADR-0035, ADR-0040)
 * Generalized list container supporting vertical, horizontal, or wrap layout.
 * Reconciles child widgets by unique non-empty keys using FGV2KeyedCollection.
 * Container hierarchy and item mutations are fully atomic via two-phase Prepare/Commit:
 * if any item fails during prepare, no widget is mutated and container children remain unchanged.
 */
UCLASS(Blueprintable)
class GV2PRESENTATIONAPPLY_API UGV2ListViewWidgetBase
    : public UCommonUserWidget
    , public IGV2UiPropertyHost
    , public IGV2PreparedKeyedCollectionTarget
{
    GENERATED_BODY()

public:
    // PSC-11: value sinks for the prepared keyed-collection operation.
    virtual UPanelWidget* GetPreparedCollectionPanel() const override;
    virtual void ResetPreparedCollection() override;
    virtual void OnPreparedCollectionSettled(
        const TArray<GV2PresentationApply::FPreparedKeyedCollectionEntry>& Entries) override;

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

    const TMap<FName, TObjectPtr<UWidget>>& GetActiveWidgetsMap() const { return ActiveWidgetsByKey; }
    void SetActiveWidgetsMap(const TMap<FName, TObjectPtr<UWidget>>& InMap) { ActiveWidgetsByKey = InMap; }

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

    // IGV2UiPropertyHost
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

    /**
     * Reconciles entries into the container panel in two phases (Prepare / Commit) using FGV2KeyedCollection.
     * All items are prepared off-tree without mutating widgets. If ANY PrepareItem fails,
     * reconciliation returns false and NO widget is mutated.
     */
    template <typename WidgetType, typename ModelType, typename PreparedType>
    bool ReconcilePreparedEntries(
        TConstArrayView<ModelType> Models,
        TFunctionRef<FName(const ModelType&)> GetKey,
        TFunctionRef<WidgetType*()> CreateItem,
        TFunctionRef<bool(WidgetType&, const ModelType&, PreparedType&)> PrepareItem,
        TFunctionRef<void(WidgetType&, const PreparedType&)> CommitItem,
        TFunction<bool(const ModelType&)> CanApplyItem = nullptr)
    {
        if (ContainerPanel == nullptr)
        {
            return false;
        }

        TMap<FName, TObjectPtr<WidgetType>> TypedActiveByKey;
        for (const auto& Pair : ActiveWidgetsByKey)
        {
            if (WidgetType* TypedWidget = Cast<WidgetType>(Pair.Value))
            {
                TypedActiveByKey.Add(Pair.Key, TypedWidget);
            }
        }

        TArray<WidgetType*> OutWidgets;
        const bool bSuccess = FGV2KeyedCollection::ReconcilePrepared<WidgetType, ModelType, PreparedType>(
            ContainerPanel,
            Models,
            TypedActiveByKey,
            GetKey,
            CreateItem,
            PrepareItem,
            CommitItem,
            OutWidgets,
            CanApplyItem);

        if (bSuccess)
        {
            ActiveWidgetsByKey.Reset();
            for (const auto& Pair : TypedActiveByKey)
            {
                ActiveWidgetsByKey.Add(Pair.Key, Pair.Value);
            }
            return true;
        }
        return false;
    }

    /**
     * Reconciles entries into the container panel using FGV2KeyedCollection.
     * Preflight validates keys and models, reuses existing widgets by key,
     * applies models, and commits child hierarchy to the container panel only on full success.
     */
    template <typename WidgetType, typename ModelType>
    bool ReconcileEntries(
        TConstArrayView<ModelType> Models,
        TFunctionRef<FName(const ModelType&)> GetKey,
        TFunctionRef<WidgetType*()> CreateItem,
        TFunctionRef<bool(WidgetType&, const ModelType&)> ApplyItem,
        TFunction<bool(const ModelType&)> CanApplyItem = nullptr)
    {
        if (ContainerPanel == nullptr)
        {
            return false;
        }

        TMap<FName, TObjectPtr<WidgetType>> TypedActiveByKey;
        for (const auto& Pair : ActiveWidgetsByKey)
        {
            if (WidgetType* TypedWidget = Cast<WidgetType>(Pair.Value))
            {
                TypedActiveByKey.Add(Pair.Key, TypedWidget);
            }
        }

        TArray<WidgetType*> OutWidgets;
        const bool bSuccess = FGV2KeyedCollection::Reconcile<WidgetType, ModelType>(
            ContainerPanel,
            Models,
            TypedActiveByKey,
            GetKey,
            CreateItem,
            [&ApplyItem](WidgetType& Item, const ModelType& Model) -> bool
            {
                return ApplyItem(Item, Model);
            },
            OutWidgets,
            CanApplyItem);

        if (bSuccess)
        {
            ActiveWidgetsByKey.Reset();
            for (const auto& Pair : TypedActiveByKey)
            {
                ActiveWidgetsByKey.Add(Pair.Key, Pair.Value);
            }
            return true;
        }
        return false;
    }

protected:
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> ContainerPanel;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|ListView")
    TEnumAsByte<EOrientation> Orientation = Orient_Vertical;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UWidget>> ActiveWidgetsByKey;

    UPROPERTY(EditAnywhere, Category = "GV2|UI|Identity", meta = (ShowOnlyInnerProperties))
    FGV2UiPropertyHostState PropertyHostState;
};
