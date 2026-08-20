#include "GV2ContentEditor/Widgets/SGV2DefinitionBrowser.h"

#if defined(__UNREAL__) || defined(UE_GAME) || defined(UE_EDITOR) || defined(WITH_ENGINE)
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace GV2ContentEditor
{

void SGV2DefinitionBrowser::Construct(const FArguments& InArgs, TSharedPtr<FGV2EditorAdapter> InAdapter)
{
    Adapter = InAdapter;
    OnDefinitionSelected = InArgs._OnDefinitionSelected;
    OnDefinitionsChanged = InArgs._OnDefinitionsChanged;

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f)
        [
            SNew(SSearchBox)
            .HintText(FText::FromString(TEXT("Search definitions by ID or kind...")))
            .OnTextChanged(this, &SGV2DefinitionBrowser::OnSearchTextChanged)
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(2.0f)
        [
            SAssignNew(ListView, SListView<TSharedPtr<FGV2DefinitionSummary>>)
            .ListItemsSource(&FilteredItems)
            .OnGenerateRow(this, &SGV2DefinitionBrowser::OnGenerateRow)
            .OnSelectionChanged(this, &SGV2DefinitionBrowser::OnSelectionChanged)
            .OnContextMenuOpening(this, &SGV2DefinitionBrowser::OnContextMenuOpening)
            .SelectionMode(ESelectionMode::Single)
        ]
    ];

    RefreshList();
}

void SGV2DefinitionBrowser::RefreshList()
{
    AllItems.Empty();
    if (Adapter.IsValid())
    {
        auto Summaries = Adapter->ListDefinitions();
        for (const auto& Summary : Summaries)
        {
            AllItems.Add(MakeShared<FGV2DefinitionSummary>(Summary));
        }
    }
    FilterItems();
}

void SGV2DefinitionBrowser::SetSelectedDefinition(const FString& DefinitionId)
{
    std::string TargetId = TCHAR_TO_UTF8(*DefinitionId);
    for (const auto& Item : FilteredItems)
    {
        if (Item.IsValid() && Item->Id == TargetId)
        {
            if (ListView.IsValid())
            {
                ListView->SetSelection(Item);
                ListView->RequestScrollIntoView(Item);
            }
            break;
        }
    }
}

void SGV2DefinitionBrowser::OnSearchTextChanged(const FText& InSearchText)
{
    CurrentFilterText = InSearchText.ToString();
    FilterItems();
}

void SGV2DefinitionBrowser::FilterItems()
{
    FilteredItems.Empty();
    FString FilterLower = CurrentFilterText.ToLower();

    for (const auto& Item : AllItems)
    {
        if (!Item.IsValid()) continue;

        if (FilterLower.IsEmpty() ||
            FString(Item->Id.c_str()).ToLower().Contains(FilterLower) ||
            FString(Item->Type.c_str()).ToLower().Contains(FilterLower) ||
            FString(Item->PackageId.c_str()).ToLower().Contains(FilterLower))
        {
            FilteredItems.Add(Item);
        }
    }

    if (ListView.IsValid())
    {
        ListView->RequestListRefresh();
    }
}

TSharedRef<ITableRow> SGV2DefinitionBrowser::OnGenerateRow(
    TSharedPtr<FGV2DefinitionSummary> Item,
    const TSharedRef<STableViewBase>& OwnerTable)
{
    bool bIsDirty = false;
    if (Adapter.IsValid())
    {
        const auto* CurrentDef = Adapter->GetCurrentDefinition();
        if (CurrentDef != nullptr && CurrentDef->Id == Item->Id && Adapter->IsDirty())
        {
            bIsDirty = true;
        }
    }

    FString DisplayText = FString::Printf(TEXT("%s%s"),
        UTF8_TO_TCHAR(Item->Id.c_str()),
        bIsDirty ? TEXT(" *") : TEXT(""));

    FString SubText = FString::Printf(TEXT("[%s] %s"),
        UTF8_TO_TCHAR(Item->Type.c_str()),
        UTF8_TO_TCHAR(Item->PackageId.c_str()));

    return SNew(STableRow<TSharedPtr<FGV2DefinitionSummary>>, OwnerTable)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .Padding(4.0f, 2.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Text(FText::FromString(DisplayText))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Text(FText::FromString(SubText))
                .ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
            ]
        ]
    ];
}

void SGV2DefinitionBrowser::OnSelectionChanged(
    TSharedPtr<FGV2DefinitionSummary> SelectedItem,
    ESelectInfo::Type /*SelectInfo*/)
{
    if (SelectedItem.IsValid() && OnDefinitionSelected.IsBound())
    {
        OnDefinitionSelected.Execute(UTF8_TO_TCHAR(SelectedItem->Id.c_str()));
    }
}

TSharedPtr<SWidget> SGV2DefinitionBrowser::OnContextMenuOpening()
{
    auto SelectedItems = ListView ? ListView->GetSelectedItems() : TArray<TSharedPtr<FGV2DefinitionSummary>>();
    if (SelectedItems.IsEmpty() || !SelectedItems[0].IsValid())
    {
        return nullptr;
    }

    FMenuBuilder MenuBuilder(true, nullptr);

    MenuBuilder.AddMenuEntry(
        FText::FromString(TEXT("Copy Stable ID")),
        FText::FromString(TEXT("Copy definition ID to clipboard")),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateSP(this, &SGV2DefinitionBrowser::HandleCopyId)));

    MenuBuilder.AddMenuEntry(
        FText::FromString(TEXT("Duplicate")),
        FText::FromString(TEXT("Duplicate this definition")),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateSP(this, &SGV2DefinitionBrowser::HandleDuplicate)));

    MenuBuilder.AddMenuEntry(
        FText::FromString(TEXT("Delete")),
        FText::FromString(TEXT("Delete this definition")),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateSP(this, &SGV2DefinitionBrowser::HandleDelete)));

    return MenuBuilder.MakeWidget();
}

void SGV2DefinitionBrowser::HandleCopyId()
{
    auto SelectedItems = ListView ? ListView->GetSelectedItems() : TArray<TSharedPtr<FGV2DefinitionSummary>>();
    if (!SelectedItems.IsEmpty() && SelectedItems[0].IsValid())
    {
        FPlatformApplicationMisc::ClipboardCopy(UTF8_TO_TCHAR(SelectedItems[0]->Id.c_str()));
    }
}

void SGV2DefinitionBrowser::HandleDuplicate()
{
    auto SelectedItems = ListView ? ListView->GetSelectedItems() : TArray<TSharedPtr<FGV2DefinitionSummary>>();
    if (!SelectedItems.IsEmpty() && SelectedItems[0].IsValid() && Adapter.IsValid())
    {
        std::string SourceId = SelectedItems[0]->Id;
        std::string TargetId = SourceId + "_copy";
        auto Result = Adapter->DuplicateDefinition(SourceId, TargetId);
        if (Result.IsSuccess())
        {
            RefreshList();
            SetSelectedDefinition(UTF8_TO_TCHAR(TargetId.c_str()));
            if (OnDefinitionsChanged.IsBound()) OnDefinitionsChanged.Execute();
        }
    }
}

void SGV2DefinitionBrowser::HandleDelete()
{
    auto SelectedItems = ListView ? ListView->GetSelectedItems() : TArray<TSharedPtr<FGV2DefinitionSummary>>();
    if (!SelectedItems.IsEmpty() && SelectedItems[0].IsValid() && Adapter.IsValid())
    {
        std::string TargetId = SelectedItems[0]->Id;
        auto InRefs = Adapter->GetIncomingReferences(TargetId);
        if (!InRefs.empty())
        {
            // CED-14: Deletion check - prevent deletion if referenced
            return;
        }

        auto Result = Adapter->DeleteDefinition(TargetId);
        if (Result.IsSuccess())
        {
            RefreshList();
            if (OnDefinitionsChanged.IsBound()) OnDefinitionsChanged.Execute();
        }
    }
}

} // namespace GV2ContentEditor
#endif
