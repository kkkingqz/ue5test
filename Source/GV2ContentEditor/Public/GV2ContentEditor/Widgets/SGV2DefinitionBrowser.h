#pragma once

#include "GV2ContentEditor/GV2ContentEditor.h"
#include "GV2ContentEditor/EditorAdapterTypes.h"
#include "GV2ContentEditor/GV2EditorAdapter.h"
#include "GV2ContentAuthoring/AuthoringIndex.h"

#if defined(__UNREAL__) || defined(UE_GAME) || defined(UE_EDITOR) || defined(WITH_ENGINE)
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Input/SSearchBox.h"

namespace GV2ContentEditor
{

enum class EGV2DefinitionTreeNodeType : uint8
{
    Namespace,
    Kind,
    PathSegment,
    Definition
};

struct GV2_CONTENT_EDITOR_API FGV2DefinitionTreeNode : public TSharedFromThis<FGV2DefinitionTreeNode>
{
    FString DisplayName;
    EGV2DefinitionTreeNodeType NodeType = EGV2DefinitionTreeNodeType::PathSegment;
    FString StableId;
    FString DefinitionType;
    FString PackageId;
    FString RelativeSource;
    int32 LoadIndex = 0;
    int32 ProviderCount = 1;
    bool bIsOverridden = false;
    FString WinnerPackageId;
    bool bIsDirty = false;
    bool bHasDirtyDescendant = false;

    TWeakPtr<FGV2DefinitionTreeNode> Parent;
    TArray<TSharedPtr<FGV2DefinitionTreeNode>> Children;
};

DECLARE_DELEGATE_OneParam(FOnDefinitionSelected, const FString& /*DefinitionId*/);
DECLARE_DELEGATE_OneParam(FOnLocatorSelected, const GV2ContentAuthoring::FAuthoringLocator& /*Locator*/);
DECLARE_DELEGATE(FOnDefinitionsChanged);
DECLARE_DELEGATE_OneParam(FOnDefinitionOperationCompleted, const FGV2EditorAuthoringResult&);

class GV2_CONTENT_EDITOR_API SGV2DefinitionBrowser : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGV2DefinitionBrowser) {}
        SLATE_EVENT(FOnDefinitionSelected, OnDefinitionSelected)
        SLATE_EVENT(FOnLocatorSelected, OnLocatorSelected)
        SLATE_EVENT(FOnDefinitionsChanged, OnDefinitionsChanged)
        SLATE_EVENT(FOnDefinitionOperationCompleted, OnOperationCompleted)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, TSharedPtr<FGV2EditorAdapter> InAdapter);

    void RefreshList();
    void SetSelectedDefinition(const FString& DefinitionId);
    void SetSelectedLocator(const GV2ContentAuthoring::FAuthoringLocator& Locator);

    const TArray<TSharedPtr<FGV2DefinitionTreeNode>>& GetRootNodes() const { return RootNodes; }
    const TArray<TSharedPtr<FGV2DefinitionTreeNode>>& GetFilteredRootNodes() const { return FilteredRootNodes; }

private:
    TSharedRef<ITableRow> OnGenerateRow(
        TSharedPtr<FGV2DefinitionTreeNode> Item,
        const TSharedRef<STableViewBase>& OwnerTable);

    void OnGetChildren(
        TSharedPtr<FGV2DefinitionTreeNode> Item,
        TArray<TSharedPtr<FGV2DefinitionTreeNode>>& OutChildren);

    void OnSelectionChanged(
        TSharedPtr<FGV2DefinitionTreeNode> SelectedItem,
        ESelectInfo::Type SelectInfo);

    TSharedPtr<SWidget> OnContextMenuOpening();

    void OnSearchTextChanged(const FText& InSearchText);

    void FilterItems();
    void BuildTree();
    void UpdateDirtyState();

    void HandleCopyId();
    FReply HandleCreate();
    void HandleCreateAction();
    void HandleDuplicate();
    void HandleRename();
    void HandleDelete();
    void HandleSelectProvider(const GV2ContentAuthoring::FAuthoringLocator& Locator);
    void ReportOperation(const FGV2EditorAuthoringResult& Result);

private:
    TSharedPtr<FGV2EditorAdapter> Adapter;
    FOnDefinitionSelected OnDefinitionSelected;
    FOnLocatorSelected OnLocatorSelected;
    FOnDefinitionsChanged OnDefinitionsChanged;
    FOnDefinitionOperationCompleted OnOperationCompleted;

    TArray<TSharedPtr<FGV2DefinitionTreeNode>> RootNodes;
    TArray<TSharedPtr<FGV2DefinitionTreeNode>> FilteredRootNodes;
    TSharedPtr<STreeView<TSharedPtr<FGV2DefinitionTreeNode>>> TreeView;
    FString CurrentFilterText;

    TSet<FString> SavedExpandedIds;
    bool bIsSearching = false;
};

} // namespace GV2ContentEditor
#endif
