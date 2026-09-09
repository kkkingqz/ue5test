#include "UI/GV2TabContainerWidgetBase.h"

#include "Components/PanelWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Runtime/GV2RuntimeSubsystem.h"
#include "UI/GV2ScreenRegistry.h"
#include "UI/GV2ScreenWidgetBase.h"
#include "UI/GV2UiTheme.h"

UGV2TabContainerWidgetBase::UGV2TabContainerWidgetBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UGV2TabContainerWidgetBase::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    OutBuilder.AddKey(TEXT("default_tab_key"), NAME_None);
    OutBuilder.AddNestedScreenCollection(TEXT("tabs"), FName(TEXT("TabContentPanel")), TEXT("key"));
    OutBuilder.AddKey(TEXT("key"), NAME_None);
}

void UGV2TabContainerWidgetBase::ApplyDefaultTabKey(FName InKey)
{
    DefaultTabKey = InKey;
}

void UGV2TabContainerWidgetBase::ApplyTabEntries(
    const TArray<FGV2TabItemEntry>& InEntries,
    const TMap<FName, UGV2ScreenWidgetBase*>& InWidgets)
{
    TabEntries = InEntries;
    TabScreenWidgets.Empty();
    for (const auto& Pair : InWidgets)
    {
        TabScreenWidgets.Add(Pair.Key, Pair.Value);
    }

    // Attach widgets to panel
    if (TabContentPanel != nullptr)
    {
        TabContentPanel->ClearChildren();
        for (const FGV2TabItemEntry& Entry : TabEntries)
        {
            if (TObjectPtr<UGV2ScreenWidgetBase> Child = TabScreenWidgets.FindRef(Entry.Key))
            {
                TabContentPanel->AddChild(Child);
            }
        }
    }

    // Determine active tab key
    const bool bPrevKeyValid = !ActiveTabKey.IsNone() && TabEntries.ContainsByPredicate([this](const FGV2TabItemEntry& Entry)
    {
        return Entry.Key == ActiveTabKey;
    });

    FName NewActiveKey = NAME_None;
    if (bPrevKeyValid)
    {
        NewActiveKey = ActiveTabKey;
    }
    else if (!DefaultTabKey.IsNone() && TabEntries.ContainsByPredicate([this](const FGV2TabItemEntry& Entry)
    {
        return Entry.Key == DefaultTabKey;
    }))
    {
        NewActiveKey = DefaultTabKey;
    }
    else if (TabEntries.Num() > 0)
    {
        NewActiveKey = TabEntries[0].Key;
    }

    ActiveTabKey = NewActiveKey;
    ActiveTabIndex = TabEntries.IndexOfByPredicate([this](const FGV2TabItemEntry& Entry)
    {
        return Entry.Key == ActiveTabKey;
    });

    UpdateActiveTabDisplay();
    // GBF-06: ApplyTabEntries is reached from the cancellable nested-screen Commit
    // path. Tab-model callbacks are deliberately absent, so model application cannot
    // execute arbitrary Blueprint/delegate observers before publication.

    // Sync active tab state with runtime coordinator
    const FString ResolvedPath = !ContainerPath.IsEmpty()
        ? ContainerPath
        : (!ConfiguredScreenFieldId.IsNone() ? FString::Printf(TEXT("location_content/main/%s"), *ConfiguredScreenFieldId.ToString()) : FString());

    if (!ResolvedPath.IsEmpty())
    {
        if (const UWorld* World = GetWorld())
        {
            if (const UGameInstance* GI = World->GetGameInstance())
            {
                if (UGV2RuntimeSubsystem* Runtime = GI->GetSubsystem<UGV2RuntimeSubsystem>())
                {
                    Runtime->SetActiveTab(ResolvedPath, ActiveTabKey.ToString());
                }
            }
        }
    }
}

void UGV2TabContainerWidgetBase::ResetTabContainerModel()
{
    if (TabContentPanel != nullptr)
    {
        TabContentPanel->ClearChildren();
    }
    for (const auto& Pair : TabScreenWidgets)
    {
        if (Pair.Value != nullptr)
        {
            Pair.Value->RemoveFromParent();
        }
    }
    TabScreenWidgets.Empty();
    TabEntries.Empty();
    DefaultTabKey = NAME_None;
    ActiveTabKey = NAME_None;
    ActiveTabIndex = INDEX_NONE;
}

bool UGV2TabContainerWidgetBase::SelectTabByKey(FName InTabKey)
{
    if (InTabKey == ActiveTabKey)
    {
        return true;
    }

    const int32 FoundIndex = TabEntries.IndexOfByPredicate([InTabKey](const FGV2TabItemEntry& Entry)
    {
        return Entry.Key == InTabKey;
    });

    if (FoundIndex == INDEX_NONE)
    {
        return false;
    }

    ActiveTabKey = InTabKey;
    ActiveTabIndex = FoundIndex;

    UpdateActiveTabDisplay();

    // Sync active tab state with runtime coordinator
    const FString ResolvedPath = !ContainerPath.IsEmpty()
        ? ContainerPath
        : (!ConfiguredScreenFieldId.IsNone() ? FString::Printf(TEXT("location_content/main/%s"), *ConfiguredScreenFieldId.ToString()) : FString());

    if (!ResolvedPath.IsEmpty())
    {
        if (const UWorld* World = GetWorld())
        {
            if (const UGameInstance* GI = World->GetGameInstance())
            {
                if (UGV2RuntimeSubsystem* Runtime = GI->GetSubsystem<UGV2RuntimeSubsystem>())
                {
                    Runtime->SetActiveTab(ResolvedPath, ActiveTabKey.ToString());
                }
            }
        }
    }

    return true;
}

bool UGV2TabContainerWidgetBase::SelectTabByIndex(int32 InIndex)
{
    if (!TabEntries.IsValidIndex(InIndex))
    {
        return false;
    }
    return SelectTabByKey(TabEntries[InIndex].Key);
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

bool UGV2TabContainerWidgetBase::ApplyPreparedKey(FName PropertyName, FName Value)
{
    // DUC-03: `default_tab_key` is its own capability, routed by name so it can never be
    // confused with this host's own `key` identity.
    if (PropertyName == TEXT("default_tab_key"))
    {
        ApplyDefaultTabKey(Value);
        return true;
    }
    GetPropertyHostState().SetKey(Value);
    return true;
}

void UGV2TabContainerWidgetBase::ApplyPreparedTabs(
    const TArray<GV2PresentationApply::FPreparedTabEntry>& PreparedEntries)
{
    TArray<FGV2TabItemEntry> Entries;
    TMap<FName, UGV2ScreenWidgetBase*> Widgets;
    Entries.Reserve(PreparedEntries.Num());
    for (const GV2PresentationApply::FPreparedTabEntry& FlatEntry : PreparedEntries)
    {
        FGV2TabItemEntry Entry;
        Entry.Key = FlatEntry.Key;
        Entry.Title.Text = FlatEntry.Title.Text;
        Entry.Title.StyleToken = FlatEntry.Title.StyleToken;
        Entry.Title.NormalizedMarkup = FlatEntry.Title.NormalizedMarkup;
        Entry.ScreenId = FlatEntry.ScreenId;
        Entries.Add(MoveTemp(Entry));

        if (UGV2ScreenWidgetBase* ScreenWidget = Cast<UGV2ScreenWidgetBase>(FlatEntry.ScreenWidget.Get()))
        {
            Widgets.Add(FlatEntry.Key, ScreenWidget);
        }
    }
    ApplyTabEntries(Entries, Widgets);
}

void UGV2TabContainerWidgetBase::ResetPreparedTabs()
{
    ResetTabContainerModel();
}
