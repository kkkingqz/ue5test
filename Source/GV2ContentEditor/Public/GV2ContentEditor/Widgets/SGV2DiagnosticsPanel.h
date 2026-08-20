#pragma once

#include "GV2ContentEditor/GV2ContentEditor.h"
#include "GV2ContentEditor/EditorAdapterTypes.h"
#include "GV2ContentEditor/GV2EditorAdapter.h"

#if defined(__UNREAL__) || defined(UE_GAME) || defined(UE_EDITOR) || defined(WITH_ENGINE)
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

namespace GV2ContentEditor
{

DECLARE_DELEGATE_TwoParams(FOnNavigateToDiagnostic, const FString& /*DefinitionId*/, const FString& /*JsonPointer*/);

class GV2_CONTENT_EDITOR_API SGV2DiagnosticsPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGV2DiagnosticsPanel) {}
        SLATE_EVENT(FOnNavigateToDiagnostic, OnNavigateToDiagnostic)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, TSharedPtr<FGV2EditorAdapter> InAdapter);

    void SetDiagnostics(const std::vector<FGV2EditorDiagnostic>& Diagnostics);
    void RefreshDiagnostics();

private:
    TSharedRef<ITableRow> OnGenerateRow(
        TSharedPtr<FGV2EditorDiagnostic> Item,
        const TSharedRef<STableViewBase>& OwnerTable);

    void OnSelectionChanged(
        TSharedPtr<FGV2EditorDiagnostic> SelectedItem,
        ESelectInfo::Type SelectInfo);

private:
    TSharedPtr<FGV2EditorAdapter> Adapter;
    FOnNavigateToDiagnostic OnNavigateToDiagnostic;

    TArray<TSharedPtr<FGV2EditorDiagnostic>> DiagnosticItems;
    TSharedPtr<SListView<TSharedPtr<FGV2EditorDiagnostic>>> ListView;
};

} // namespace GV2ContentEditor
#endif
