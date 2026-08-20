#include "GV2ContentEditor/Widgets/SGV2ContentEditorTab.h"

#if defined(__UNREAL__) || defined(UE_GAME) || defined(UE_EDITOR) || defined(WITH_ENGINE)
#include "Misc/Paths.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace GV2ContentEditor
{

const FName SGV2ContentEditorTab::TabName("GV2ContentEditorTab");

void SGV2ContentEditorTab::Construct(const FArguments& /*InArgs*/)
{
    Adapter = MakeShared<FGV2EditorAdapter>();

    FString ContentRootPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData")));
    std::vector<FGV2EditorDiagnostic> InitDiags;
    Adapter->Initialize(TCHAR_TO_UTF8(*ContentRootPath), InitDiags);

    ChildSlot
    [
        SNew(SSplitter)
        .Orientation(Orient_Vertical)
        + SSplitter::Slot()
        .Value(0.75f)
        [
            SNew(SSplitter)
            .Orientation(Orient_Horizontal)
            + SSplitter::Slot()
            .Value(0.25f)
            [
                SAssignNew(BrowserWidget, SGV2DefinitionBrowser, Adapter)
                .OnDefinitionSelected(this, &SGV2ContentEditorTab::HandleDefinitionSelected)
                .OnOperationCompleted(this, &SGV2ContentEditorTab::HandleSaveCompleted)
                .OnDefinitionsChanged_Lambda([this]() {
                    if (PropertiesWidget.IsValid()) PropertiesWidget->RefreshProperties();
                    if (ReferenceWidget.IsValid()) ReferenceWidget->RefreshReferences();
                    if (DiagnosticsWidget.IsValid()) DiagnosticsWidget->RefreshDiagnostics();
                })
            ]
            + SSplitter::Slot()
            .Value(0.50f)
            [
                SAssignNew(PropertiesWidget, SGV2DefinitionProperties, Adapter)
                .OnFieldValueChanged(this, &SGV2ContentEditorTab::HandleFieldValueChanged)
                .OnSaveCompleted(this, &SGV2ContentEditorTab::HandleSaveCompleted)
            ]
            + SSplitter::Slot()
            .Value(0.25f)
            [
                SAssignNew(ReferenceWidget, SGV2ReferencePanel, Adapter)
                .OnNavigateToDefinition(this, &SGV2ContentEditorTab::HandleNavigateToDefinition)
            ]
        ]
        + SSplitter::Slot()
        .Value(0.25f)
        [
            SAssignNew(DiagnosticsWidget, SGV2DiagnosticsPanel, Adapter)
            .OnNavigateToDiagnostic(this, &SGV2ContentEditorTab::HandleNavigateToDiagnostic)
        ]
    ];
}

void SGV2ContentEditorTab::OpenDefinition(const FString& DefinitionId)
{
    if (BrowserWidget.IsValid())
    {
        BrowserWidget->SetSelectedDefinition(DefinitionId);
    }
    HandleDefinitionSelected(DefinitionId);
}

void SGV2ContentEditorTab::HandleDefinitionSelected(const FString& DefinitionId)
{
    if (Adapter.IsValid())
    {
        std::vector<FGV2EditorDiagnostic> LoadDiags;
        Adapter->LoadDefinition(TCHAR_TO_UTF8(*DefinitionId), LoadDiags);
        if (DiagnosticsWidget.IsValid())
        {
            DiagnosticsWidget->SetDiagnostics(LoadDiags);
        }
    }

    if (PropertiesWidget.IsValid())
    {
        PropertiesWidget->RefreshProperties();
    }
    if (ReferenceWidget.IsValid())
    {
        ReferenceWidget->RefreshReferences();
    }
}

void SGV2ContentEditorTab::HandleFieldValueChanged()
{
    if (BrowserWidget.IsValid())
    {
        BrowserWidget->RefreshList();
    }
    if (ReferenceWidget.IsValid())
    {
        ReferenceWidget->RefreshReferences();
    }
}

void SGV2ContentEditorTab::HandleSaveCompleted(const FGV2EditorAuthoringResult& Result)
{
    if (BrowserWidget.IsValid())
    {
        BrowserWidget->RefreshList();
    }
    if (ReferenceWidget.IsValid())
    {
        ReferenceWidget->RefreshReferences();
    }
    if (DiagnosticsWidget.IsValid())
    {
        DiagnosticsWidget->SetDiagnostics(Result.Diagnostics);
    }
}

void SGV2ContentEditorTab::HandleNavigateToDefinition(const FString& DefinitionId)
{
    OpenDefinition(DefinitionId);
}

void SGV2ContentEditorTab::HandleNavigateToDiagnostic(const FString& DefinitionId, const FString& JsonPointer)
{
    if (!DefinitionId.IsEmpty())
    {
        OpenDefinition(DefinitionId);
    }
    if (PropertiesWidget.IsValid())
    {
        PropertiesWidget->FocusField(JsonPointer);
    }
}

} // namespace GV2ContentEditor
#endif
