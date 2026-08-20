#pragma once

#include "GV2ContentEditor/GV2ContentEditor.h"
#include "GV2ContentEditor/EditorAdapterTypes.h"
#include "GV2ContentEditor/GV2EditorAdapter.h"
#include "GV2ContentEditor/ReferenceScanner.h"

#if defined(__UNREAL__) || defined(UE_GAME) || defined(UE_EDITOR) || defined(WITH_ENGINE)
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

namespace GV2ContentEditor
{

DECLARE_DELEGATE_OneParam(FOnNavigateToDefinition, const FString& /*DefinitionId*/);

class GV2_CONTENT_EDITOR_API SGV2ReferencePanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGV2ReferencePanel) {}
        SLATE_EVENT(FOnNavigateToDefinition, OnNavigateToDefinition)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, TSharedPtr<FGV2EditorAdapter> InAdapter);

    void RefreshReferences();

private:
    TSharedRef<ITableRow> OnGenerateOutgoingRow(
        TSharedPtr<FGV2ReferenceItem> Item,
        const TSharedRef<STableViewBase>& OwnerTable);

    TSharedRef<ITableRow> OnGenerateIncomingRow(
        TSharedPtr<FGV2ReferenceItem> Item,
        const TSharedRef<STableViewBase>& OwnerTable);

    void OnOutgoingSelectionChanged(
        TSharedPtr<FGV2ReferenceItem> SelectedItem,
        ESelectInfo::Type SelectInfo);

    void OnIncomingSelectionChanged(
        TSharedPtr<FGV2ReferenceItem> SelectedItem,
        ESelectInfo::Type SelectInfo);

private:
    TSharedPtr<FGV2EditorAdapter> Adapter;
    FOnNavigateToDefinition OnNavigateToDefinition;

    TArray<TSharedPtr<FGV2ReferenceItem>> OutgoingItems;
    TArray<TSharedPtr<FGV2ReferenceItem>> IncomingItems;

    TSharedPtr<SListView<TSharedPtr<FGV2ReferenceItem>>> OutgoingListView;
    TSharedPtr<SListView<TSharedPtr<FGV2ReferenceItem>>> IncomingListView;
};

} // namespace GV2ContentEditor
#endif
