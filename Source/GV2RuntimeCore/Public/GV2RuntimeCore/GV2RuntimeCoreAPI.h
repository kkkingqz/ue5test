#pragma once

#if defined(_WIN32)
#if defined(GV2_RUNTIME_CORE_EXPORTS)
#define GV2_PORTABLE_API __declspec(dllexport)
#elif defined(GV2_RUNTIME_CORE_IMPORTS)
#define GV2_PORTABLE_API __declspec(dllimport)
#else
#define GV2_PORTABLE_API
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define GV2_PORTABLE_API __attribute__((visibility("default")))
#else
#define GV2_PORTABLE_API
#endif
