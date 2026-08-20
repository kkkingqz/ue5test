#pragma once

#if defined(GV2CONTENTEDITOR_API)
    #define GV2_CONTENT_EDITOR_API GV2CONTENTEDITOR_API
#elif defined(GV2_CONTENT_EDITOR_EXPORTS)
    #if defined(_MSC_VER)
        #define GV2_CONTENT_EDITOR_API __declspec(dllexport)
    #elif defined(__GNUC__) || defined(__clang__)
        #define GV2_CONTENT_EDITOR_API __attribute__((visibility("default")))
    #else
        #define GV2_CONTENT_EDITOR_API
    #endif
#elif defined(GV2_CONTENT_EDITOR_IMPORTS)
    #if defined(_MSC_VER)
        #define GV2_CONTENT_EDITOR_API __declspec(dllimport)
    #else
        #define GV2_CONTENT_EDITOR_API
    #endif
#else
    #define GV2_CONTENT_EDITOR_API
#endif

#if defined(__UNREAL__) || defined(UE_GAME) || defined(UE_EDITOR) || defined(WITH_ENGINE)
#include "Modules/ModuleManager.h"

class IGV2ContentEditorModule : public IModuleInterface
{
public:
    static inline IGV2ContentEditorModule& Get()
    {
        return FModuleManager::LoadModuleChecked<IGV2ContentEditorModule>("GV2ContentEditor");
    }

    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("GV2ContentEditor");
    }
};
#endif
