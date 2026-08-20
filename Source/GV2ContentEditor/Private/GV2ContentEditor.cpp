#include "GV2ContentEditor/GV2ContentEditor.h"
#include "GV2ContentEditor/Widgets/SGV2ContentEditorTab.h"

#if defined(__UNREAL__) || defined(UE_GAME) || defined(UE_EDITOR) || defined(WITH_ENGINE)
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

class FGV2ContentEditorModule : public IGV2ContentEditorModule
{
public:
    virtual void StartupModule() override
    {
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
            GV2ContentEditor::SGV2ContentEditorTab::TabName,
            FOnSpawnTab::CreateRaw(this, &FGV2ContentEditorModule::SpawnContentEditorTab))
            .SetDisplayName(FText::FromString(TEXT("GV2 Content Editor")))
            .SetTooltipText(FText::FromString(TEXT("Open the GV2 Content Editor window")))
            .SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory());
    }

    virtual void ShutdownModule() override
    {
        if (FSlateApplication::IsInitialized())
        {
            FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GV2ContentEditor::SGV2ContentEditorTab::TabName);
        }
    }

private:
    TSharedRef<SDockTab> SpawnContentEditorTab(const FSpawnTabArgs& /*SpawnTabArgs*/)
    {
        return SNew(SDockTab)
            .TabRole(ETabRole::NomadTab)
            [
                SNew(GV2ContentEditor::SGV2ContentEditorTab)
            ];
    }
};

IMPLEMENT_MODULE(FGV2ContentEditorModule, GV2ContentEditor)
#endif
