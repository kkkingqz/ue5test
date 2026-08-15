#pragma once

#if defined(GV2_TEST_SUPPORT_EXPORTS)
    #if defined(_MSC_VER)
        #define GV2_TEST_SUPPORT_API __declspec(dllexport)
    #elif defined(__GNUC__) || defined(__clang__)
        #define GV2_TEST_SUPPORT_API __attribute__((visibility("default")))
    #else
        #define GV2_TEST_SUPPORT_API
    #endif
#elif defined(GV2_TEST_SUPPORT_IMPORTS)
    #if defined(_MSC_VER)
        #define GV2_TEST_SUPPORT_API __declspec(dllimport)
    #else
        #define GV2_TEST_SUPPORT_API
    #endif
#else
    #define GV2_TEST_SUPPORT_API
#endif
