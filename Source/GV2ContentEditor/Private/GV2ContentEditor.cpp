#include "GV2ContentEditor/GV2ContentEditor.h"

#if defined(__UNREAL__) || defined(UE_GAME) || defined(UE_EDITOR) || defined(WITH_ENGINE)
#include "Modules/ModuleManager.h"

class FGV2ContentEditorModule : public IGV2ContentEditorModule
{
public:
    virtual void StartupModule() override
    {
    }

    virtual void ShutdownModule() override
    {
    }
};

IMPLEMENT_MODULE(FGV2ContentEditorModule, GV2ContentEditor)
#endif
