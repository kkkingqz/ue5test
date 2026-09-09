#pragma once

#include "GV2PresentationApply/PreparedApplyTargets.h"
#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Bridge/GV2BridgeTypes.h"
#include "UI/GV2UiPropertyHost.h"
#include "GV2TabContainerWidgetBase.generated.h"

class UGV2ScreenWidgetBase;
class UPanelWidget;

USTRUCT(BlueprintType)
struct GV2_API FGV2TabItemEntry
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Tabs")
    FName Key;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Tabs")
    FGV2TextViewModel Title;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Tabs")
    FString ScreenId;
};

UCLASS(BlueprintType, Blueprintable)
class GV2_API UGV2TabContainerWidgetBase : public UCommonUserWidget, public IGV2UiPropertyHost
    , public IGV2PreparedTabContainerTarget
{
    GENERATED_BODY()

public:
    // PSC-11: value sinks for the prepared tab-container operation.
    virtual void ApplyPreparedTabs(const TArray<GV2PresentationApply::FPreparedTabEntry>& Entries) override;
    virtual void ResetPreparedTabs() override;

    // PSC-11: this host routes a NAMED key capability of its own before the generic
    // identity write; see the implementation.
    virtual bool ApplyPreparedKey(FName PropertyName, FName Value) override;

    UGV2TabContainerWidgetBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    // IGV2UiPropertyHost
    virtual void DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const override;
    virtual FGV2UiPropertyHostState& GetPropertyHostState() override { return PropertyHostState; }
    virtual const FGV2UiPropertyHostState& GetPropertyHostState() const override { return PropertyHostState; }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Tabs")
    void ApplyDefaultTabKey(FName InKey);

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Tabs")
    FName GetDefaultTabKey() const { return DefaultTabKey; }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Tabs")
    void ApplyTabEntries(
        const TArray<FGV2TabItemEntry>& InEntries,
        const TMap<FName, UGV2ScreenWidgetBase*>& InWidgets);

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Tabs")
    const TArray<FGV2TabItemEntry>& GetTabEntries() const { return TabEntries; }

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Tabs")
    void ResetTabContainerModel();

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Tabs")
    bool SelectTabByKey(FName InTabKey);

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Tabs")
    bool SelectTabByIndex(int32 InIndex);

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Tabs")
    FName GetActiveTabKey() const { return ActiveTabKey; }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Tabs")
    int32 GetActiveTabIndex() const { return ActiveTabIndex; }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Tabs")
    UGV2ScreenWidgetBase* GetActiveScreenWidget() const;

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Tabs")
    UGV2ScreenWidgetBase* GetScreenWidgetForTab(FName InTabKey) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|ScreenField")
    FName ConfiguredScreenFieldId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Tabs")
    FString ContainerPath;

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Tabs")
    void SetContainerPath(const FString& InPath) { ContainerPath = InPath; }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Tabs")
    FString GetContainerPath() const { return ContainerPath; }

    UPanelWidget* GetTabContentPanel() const { return TabContentPanel; }

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Tabs")
    FName DefaultTabKey;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Tabs")
    TArray<FGV2TabItemEntry> TabEntries;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Tabs")
    FName ActiveTabKey;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Tabs")
    int32 ActiveTabIndex = INDEX_NONE;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UGV2ScreenWidgetBase>> TabScreenWidgets;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> TabContentPanel;

    UPROPERTY(EditAnywhere, Category = "GV2|UI|Identity", meta = (ShowOnlyInnerProperties))
    FGV2UiPropertyHostState PropertyHostState;

private:
    void UpdateActiveTabDisplay();
};
