#pragma once

#include "GV2ContentEditor/GV2ContentEditor.h"
#include "GV2ContentEditor/GV2EditorAdapter.h"
#include "GV2ContentEditor/Widgets/SGV2DefinitionBrowser.h"
#include "GV2ContentEditor/Widgets/SGV2DefinitionProperties.h"
#include "GV2ContentEditor/Widgets/SGV2ReferencePanel.h"

#if defined(__UNREAL__) || defined(UE_GAME) || defined(UE_EDITOR) || defined(WITH_ENGINE)
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Docking/SDockTab.h"

namespace GV2ContentEditor
{

class GV2_CONTENT_EDITOR_API SGV2ContentEditorTab : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGV2ContentEditorTab) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    static const FName TabName;

    void OpenDefinition(const FString& DefinitionId);

private:
    void HandleDefinitionSelected(const FString& DefinitionId);
    void HandleFieldValueChanged();
    void HandleNavigateToDefinition(const FString& DefinitionId);

private:
    TSharedPtr<FGV2EditorAdapter> Adapter;

    TSharedPtr<SGV2DefinitionBrowser> BrowserWidget;
    TSharedPtr<SGV2DefinitionProperties> PropertiesWidget;
    TSharedPtr<SGV2ReferencePanel> ReferenceWidget;
};

} // namespace GV2ContentEditor
#endif
