#pragma once

#include "CoreMinimal.h"
#include "Bridge/GV2BridgeTypes.h"
#include "UI/GV2DynamicScreenElement.h"
#include "UI/GV2UiStyleConsumer.h"
#include "UI/GV2WidgetBase.h"
#include "GV2TabContainerWidgetBase.generated.h"

class UGV2ScreenWidgetBase;
class UGV2ScreenRegistry;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGV2OnTabChanged, FName, NewTabKey, int32, NewTabIndex);

UCLASS(BlueprintType, Blueprintable)
class GV2_API UGV2TabContainerWidgetBase : public UGV2WidgetBase, public IGV2DynamicScreenElement, public IGV2UiStyleConsumer
{
    GENERATED_BODY()

public:
    UGV2TabContainerWidgetBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    // IGV2DynamicScreenElement
    virtual FName GetConfiguredScreenFieldId_Implementation() const override;
    virtual FString GetDeclaredSchemaId_Implementation() const override;
    virtual bool CanApplyScreenField_Implementation(const FGV2ScreenFieldValue& FieldValue) const override;
    virtual bool ApplyScreenField_Implementation(const FGV2ScreenFieldValue& FieldValue) override;
    virtual void ResetScreenField_Implementation() override;

    // IGV2UiStyleConsumer
    virtual void ApplyUiTheme_Implementation(UGV2UiTheme* Theme) override;

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Tabs")
    bool CanApplyTabContainerModel(const FGV2TabContainerViewModel& InModel) const;

    UFUNCTION(BlueprintCallable, Category = "GV2|UI|Tabs")
    bool ApplyTabContainerModel(const FGV2TabContainerViewModel& InModel);

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
    const FGV2TabContainerViewModel& GetTabContainerModel() const { return Model; }

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Tabs")
    UGV2ScreenWidgetBase* GetActiveScreenWidget() const;

    UFUNCTION(BlueprintPure, Category = "GV2|UI|Tabs")
    UGV2ScreenWidgetBase* GetScreenWidgetForTab(FName InTabKey) const;

    UPROPERTY(BlueprintAssignable, Category = "GV2|UI|Tabs")
    FGV2OnTabChanged OnTabChanged;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|ScreenField")
    FName ConfiguredScreenFieldId;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Tabs")
    FGV2TabContainerViewModel Model;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Tabs")
    FName ActiveTabKey;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Tabs")
    int32 ActiveTabIndex = INDEX_NONE;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UGV2ScreenWidgetBase>> TabScreenWidgets;

    UFUNCTION(BlueprintImplementableEvent, Category = "GV2|UI|Tabs")
    void OnTabSelectionUpdated(FName NewTabKey, int32 NewTabIndex);

    UFUNCTION(BlueprintImplementableEvent, Category = "GV2|UI|Tabs")
    void OnTabModelApplied();

private:
    void UpdateActiveTabDisplay();
};
