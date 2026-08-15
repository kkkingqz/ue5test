#pragma once

#include "GV2RuntimeCore/GV2RuntimeCoreAPI.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"
#include "GV2ContentCore/Value.h"

#include <cstddef>
#include <string>

struct lua_State;

namespace GV2RuntimeCore
{
class GV2_PORTABLE_API FGV2LuaMarshaller
{
public:
    static constexpr int DefaultMaxDepth = 64;
    static constexpr std::size_t DefaultMaxNodes = 10000;

    static bool PushValue(
        lua_State* State,
        const FValue& Value,
        FRuntimeFault& OutFault,
        int MaxDepth = DefaultMaxDepth,
        std::size_t MaxNodes = DefaultMaxNodes);

    static bool PushObject(
        lua_State* State,
        const FValue::FObject& Object,
        FRuntimeFault& OutFault,
        int MaxDepth = DefaultMaxDepth,
        std::size_t MaxNodes = DefaultMaxNodes);

    static bool PushValue(
        lua_State* State,
        const GV2ContentCore::FValue& Value,
        FRuntimeFault& OutFault,
        int MaxDepth = DefaultMaxDepth,
        std::size_t MaxNodes = DefaultMaxNodes);

    static bool PushObject(
        lua_State* State,
        const GV2ContentCore::FValue::FObject& Object,
        FRuntimeFault& OutFault,
        int MaxDepth = DefaultMaxDepth,
        std::size_t MaxNodes = DefaultMaxNodes);

    static bool ReadFlatScalarObject(
        lua_State* State,
        int TableIndex,
        FValue::FObject& OutObject,
        FRuntimeFault& OutFault);

    static void PushNull(lua_State* State);
};
} // namespace GV2RuntimeCore
