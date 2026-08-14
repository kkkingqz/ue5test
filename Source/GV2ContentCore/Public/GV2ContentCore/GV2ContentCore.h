#pragma once

#if defined(GV2_CONTENT_CORE_EXPORTS)
    #if defined(_MSC_VER)
        #define GV2_CONTENT_CORE_API __declspec(dllexport)
    #elif defined(__GNUC__) || defined(__clang__)
        #define GV2_CONTENT_CORE_API __attribute__((visibility("default")))
    #else
        #define GV2_CONTENT_CORE_API
    #endif
#elif defined(GV2_CONTENT_CORE_IMPORTS)
    #if defined(_MSC_VER)
        #define GV2_CONTENT_CORE_API __declspec(dllimport)
    #else
        #define GV2_CONTENT_CORE_API
    #endif
#else
    #define GV2_CONTENT_CORE_API
#endif

namespace GV2ContentCore
{
    /**
     * Portable Content Core version identification function.
     * Guaranteed to compile without Unreal or Lua dependencies.
     */
    GV2_CONTENT_CORE_API const char* GetVersionString();
}
