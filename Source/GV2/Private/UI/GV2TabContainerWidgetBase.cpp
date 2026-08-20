#include "UI/GV2TabContainerWidgetBase.h"

#include "UI/GV2ScreenRegistry.h"
#include "UI/GV2ScreenWidgetBase.h"
#include "UI/GV2UiTheme.h"

namespace
{
constexpr TCHAR TabContainerDeclaredSchemaId[] = TEXT("core:schema.ui_field.tab_container.v1");
}

UGV2TabContainerWidgetBase::UGV2TabContainerWidgetBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

FName UGV2TabContainerWidgetBase::GetConfiguredScreenFieldId_Implementation() const
{
    return ConfiguredScreenFieldId;
}

FString UGV2TabContainerWidgetBase::GetDeclaredSchemaId_Implementation() const
{
    return TabContainerDeclaredSchemaId;
}

bool UGV2TabContainerWidgetBase::CanApplyScreenField_Implementation(const FGV2ScreenFieldValue& FieldValue) const
{
    if (FieldValue.SchemaId != TabContainerDeclaredSchemaId || !FieldValue.TabContainerValue.IsValid())
    {
        return false;
    }
    return CanApplyTabContainerModel(*FieldValue.TabContainerValue);
}

bool UGV2TabContainerWidgetBase::ApplyScreenField_Implementation(const FGV2ScreenFieldValue& FieldValue)
{
    if (!CanApplyScreenField_Implementation(FieldValue))
    {
        return false;
    }
    return ApplyTabContainerModel(*FieldValue.TabContainerValue);
}

void UGV2TabContainerWidgetBase::ResetScreenField_Implementation()
{
    ResetTabContainerModel();
}

void UGV2TabContainerWidgetBase::ApplyUiTheme_Implementation(UGV2UiTheme* Theme)
{
    if (Theme == nullptr)
    {
        return;
    }

    for (const auto& Pair : TabScreenWidgets)
    {
        if (Pair.Value != nullptr && Pair.Value->GetClass()->ImplementsInterface(UGV2UiStyleConsumer::StaticClass()))
        {
            IGV2UiStyleConsumer::Execute_ApplyUiTheme(Pair.Value, Theme);
        }
    }
}

bool UGV2TabContainerWidgetBase::CanApplyTabContainerModel(const FGV2TabContainerViewModel& InModel) const
{
    if (InModel.Tabs.IsEmpty())
    {
        return false;
    }

    TSet<FName> SeenKeys;
    for (const FGV2TabItemViewModel& Tab : InModel.Tabs)
    {
        if (Tab.Key.IsNone() || SeenKeys.Contains(Tab.Key))
        {
            return false;
        }
        SeenKeys.Add(Tab.Key);

        if (Tab.ScreenId.IsEmpty())
        {
            return false;
        }
    }

    return true;
}

bool UGV2TabContainerWidgetBase::ApplyTabContainerModel(const FGV2TabContainerViewModel& InModel)
{
    if (!CanApplyTabContainerModel(InModel))
    {
        return false;
    }

    Model = InModel;

    // Determine active tab key
    const bool bPrevKeyValid = !ActiveTabKey.IsNone() && Model.Tabs.ContainsByPredicate([this](const FGV2TabItemViewModel& Tab)
    {
        return Tab.Key == ActiveTabKey;
    });

    FName NewActiveKey = NAME_None;
    if (bPrevKeyValid)
    {
        NewActiveKey = ActiveTabKey;
    }
    else if (!Model.DefaultTabKey.IsNone() && Model.Tabs.ContainsByPredicate([this](const FGV2TabItemViewModel& Tab)
    {
        return Tab.Key == Model.DefaultTabKey;
    }))
    {
        NewActiveKey = Model.DefaultTabKey;
    }
    else
    {
        NewActiveKey = Model.Tabs[0].Key;
    }

    ActiveTabKey = NewActiveKey;
    ActiveTabIndex = Model.Tabs.IndexOfByPredicate([this](const FGV2TabItemViewModel& Tab)
    {
        return Tab.Key == ActiveTabKey;
    });

    // Reconcile child screen widgets for tabs
    UWorld* World = GetWorld();
    TSet<FName> ActiveKeys;

    for (const FGV2TabItemViewModel& Tab : Model.Tabs)
    {
        ActiveKeys.Add(Tab.Key);
        TObjectPtr<UGV2ScreenWidgetBase>* ExistingWidgetPtr = TabScreenWidgets.Find(Tab.Key);
        UGV2ScreenWidgetBase* ChildWidget = ExistingWidgetPtr != nullptr ? ExistingWidgetPtr->Get() : nullptr;

        // If widget already exists, apply fields
        if (ChildWidget != nullptr)
        {
            ChildWidget->ApplyScreenFields(Tab.Fields);
        }
        else if (World != nullptr)
        {
            // For headless / simulation tests or dynamic templates, instantiate generic ScreenWidget if no specific class
            ChildWidget = CreateWidget<UGV2ScreenWidgetBase>(World, UGV2ScreenWidgetBase::StaticClass());
            if (ChildWidget != nullptr)
            {
                ChildWidget->ApplyScreenFields(Tab.Fields);
                TabScreenWidgets.Add(Tab.Key, ChildWidget);
            }
        }
    }

    // Clean up detached tabs
    TArray<FName> StaleKeys;
    for (const auto& Pair : TabScreenWidgets)
    {
        if (!ActiveKeys.Contains(Pair.Key))
        {
            StaleKeys.Add(Pair.Key);
        }
    }
    for (const FName StaleKey : StaleKeys)
    {
        if (TObjectPtr<UGV2ScreenWidgetBase>* StaleWidget = TabScreenWidgets.Find(StaleKey))
        {
            if (*StaleWidget != nullptr)
            {
                (*StaleWidget)->RemoveFromParent();
            }
            TabScreenWidgets.Remove(StaleKey);
        }
    }

    UpdateActiveTabDisplay();
    OnTabModelApplied();
    OnTabSelectionUpdated(ActiveTabKey, ActiveTabIndex);
    OnTabChanged.Broadcast(ActiveTabKey, ActiveTabIndex);

    return true;
}

void UGV2TabContainerWidgetBase::ResetTabContainerModel()
{
    for (const auto& Pair : TabScreenWidgets)
    {
        if (Pair.Value != nullptr)
        {
            Pair.Value->RemoveFromParent();
        }
    }
    TabScreenWidgets.Empty();
    Model = {};
    ActiveTabKey = NAME_None;
    ActiveTabIndex = INDEX_NONE;
}

bool UGV2TabContainerWidgetBase::SelectTabByKey(FName InTabKey)
{
    if (InTabKey == ActiveTabKey)
    {
        return true;
    }

    const int32 FoundIndex = Model.Tabs.IndexOfByPredicate([InTabKey](const FGV2TabItemViewModel& Tab)
    {
        return Tab.Key == InTabKey;
    });

    if (FoundIndex == INDEX_NONE)
    {
        return false;
    }

    ActiveTabKey = InTabKey;
    ActiveTabIndex = FoundIndex;

    UpdateActiveTabDisplay();
    OnTabSelectionUpdated(ActiveTabKey, ActiveTabIndex);
    OnTabChanged.Broadcast(ActiveTabKey, ActiveTabIndex);
    return true;
}

bool UGV2TabContainerWidgetBase::SelectTabByIndex(int32 InIndex)
{
    if (!Model.Tabs.IsValidIndex(InIndex))
    {
        return false;
    }
    return SelectTabByKey(Model.Tabs[InIndex].Key);
}

UGV2ScreenWidgetBase* UGV2TabContainerWidgetBase::GetActiveScreenWidget() const
{
    if (ActiveTabKey.IsNone())
    {
        return nullptr;
    }
    return TabScreenWidgets.FindRef(ActiveTabKey);
}

UGV2ScreenWidgetBase* UGV2TabContainerWidgetBase::GetScreenWidgetForTab(FName InTabKey) const
{
    return TabScreenWidgets.FindRef(InTabKey);
}

void UGV2TabContainerWidgetBase::UpdateActiveTabDisplay()
{
    for (const auto& Pair : TabScreenWidgets)
    {
        if (Pair.Value != nullptr)
        {
            const bool bIsActive = (Pair.Key == ActiveTabKey);
            Pair.Value->SetVisibility(bIsActive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
            Pair.Value->SetIsEnabled(bIsActive);
        }
    }
}
