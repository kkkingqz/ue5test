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

    FString ContentRootPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData"));
    std::vector<FGV2EditorDiagnostic> InitDiags;
    Adapter->Initialize(TCHAR_TO_UTF8(*ContentRootPath), InitDiags);

    ChildSlot
    [
        SNew(SSplitter)
        .Orientation(Orient_Horizontal)
        + SSplitter::Slot()
        .Value(0.25f)
        [
            SAssignNew(BrowserWidget, SGV2DefinitionBrowser, Adapter)
            .OnDefinitionSelected(this, &SGV2ContentEditorTab::HandleDefinitionSelected)
        ]
        + SSplitter::Slot()
        .Value(0.50f)
        [
            SAssignNew(PropertiesWidget, SGV2DefinitionProperties, Adapter)
            .OnFieldValueChanged(this, &SGV2ContentEditorTab::HandleFieldValueChanged)
        ]
        + SSplitter::Slot()
        .Value(0.25f)
        [
            SAssignNew(ReferenceWidget, SGV2ReferencePanel, Adapter)
            .OnNavigateToDefinition(this, &SGV2ContentEditorTab::HandleNavigateToDefinition)
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

void SGV2ContentEditorTab::HandleNavigateToDefinition(const FString& DefinitionId)
{
    OpenDefinition(DefinitionId);
}

} // namespace GV2ContentEditor
#endif
