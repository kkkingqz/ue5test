#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
    #if defined(GV2_CONTENT_AUTHORING_EXPORTS)
        #define GV2_CONTENT_AUTHORING_API __declspec(dllexport)
    #elif defined(GV2_CONTENT_AUTHORING_IMPORTS)
        #define GV2_CONTENT_AUTHORING_API __declspec(dllimport)
    #else
        #define GV2_CONTENT_AUTHORING_API
    #endif
#else
    #if defined(GV2_CONTENT_AUTHORING_EXPORTS)
        #define GV2_CONTENT_AUTHORING_API __attribute__((visibility("default")))
    #else
        #define GV2_CONTENT_AUTHORING_API
    #endif
#endif

namespace GV2ContentAuthoring
{
GV2_CONTENT_AUTHORING_API const char* GetVersionString();
}
