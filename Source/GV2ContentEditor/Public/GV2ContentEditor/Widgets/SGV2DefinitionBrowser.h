#pragma once

#include "GV2ContentEditor/GV2ContentEditor.h"
#include "GV2ContentEditor/EditorAdapterTypes.h"
#include "GV2ContentEditor/GV2EditorAdapter.h"

#if defined(__UNREAL__) || defined(UE_GAME) || defined(UE_EDITOR) || defined(WITH_ENGINE)
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Input/SSearchBox.h"

namespace GV2ContentEditor
{

DECLARE_DELEGATE_OneParam(FOnDefinitionSelected, const FString& /*DefinitionId*/);
DECLARE_DELEGATE(FOnDefinitionsChanged);
DECLARE_DELEGATE_OneParam(FOnDefinitionOperationCompleted, const FGV2EditorAuthoringResult&);

class GV2_CONTENT_EDITOR_API SGV2DefinitionBrowser : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGV2DefinitionBrowser) {}
        SLATE_EVENT(FOnDefinitionSelected, OnDefinitionSelected)
        SLATE_EVENT(FOnDefinitionsChanged, OnDefinitionsChanged)
        SLATE_EVENT(FOnDefinitionOperationCompleted, OnOperationCompleted)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, TSharedPtr<FGV2EditorAdapter> InAdapter);

    void RefreshList();
    void SetSelectedDefinition(const FString& DefinitionId);

private:
    TSharedRef<ITableRow> OnGenerateRow(
        TSharedPtr<FGV2DefinitionSummary> Item,
        const TSharedRef<STableViewBase>& OwnerTable);

    void OnSelectionChanged(
        TSharedPtr<FGV2DefinitionSummary> SelectedItem,
        ESelectInfo::Type SelectInfo);

    TSharedPtr<SWidget> OnContextMenuOpening();

    void OnSearchTextChanged(const FText& InSearchText);

    void FilterItems();

    void HandleCopyId();
    FReply HandleCreate();
    void HandleDuplicate();
    void HandleRename();
    void HandleDelete();
    void ReportOperation(const FGV2EditorAuthoringResult& Result);

private:
    TSharedPtr<FGV2EditorAdapter> Adapter;
    FOnDefinitionSelected OnDefinitionSelected;
    FOnDefinitionsChanged OnDefinitionsChanged;
    FOnDefinitionOperationCompleted OnOperationCompleted;

    TArray<TSharedPtr<FGV2DefinitionSummary>> AllItems;
    TArray<TSharedPtr<FGV2DefinitionSummary>> FilteredItems;
    TSharedPtr<SListView<TSharedPtr<FGV2DefinitionSummary>>> ListView;
    FString CurrentFilterText;
};

} // namespace GV2ContentEditor
#endif
