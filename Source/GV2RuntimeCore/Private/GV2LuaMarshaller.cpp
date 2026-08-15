#include "GV2RuntimeCore/GV2LuaMarshaller.h"
#include "GV2RuntimeCore/Testing/GV2LuaMarshallerConformance.h"

extern "C"
{
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <cmath>
#include <limits>
#include <string_view>

namespace GV2RuntimeCore
{
namespace
{
template <typename TValue>
struct TValueTraits;

template <>
struct TValueTraits<GV2RuntimeCore::FValue>
{
    using FArrayType = GV2RuntimeCore::FValue::FArray;
    using FObjectType = GV2RuntimeCore::FValue::FObject;

    static bool IsNull(const GV2RuntimeCore::FValue& Value)
    {
        return std::holds_alternative<std::monostate>(Value.Data);
    }
    static bool IsBoolean(const GV2RuntimeCore::FValue& Value)
    {
        return std::holds_alternative<bool>(Value.Data);
    }
    static bool IsInteger(const GV2RuntimeCore::FValue& Value)
    {
        return std::holds_alternative<std::int64_t>(Value.Data);
    }
    static bool IsNumber(const GV2RuntimeCore::FValue& Value)
    {
        return std::holds_alternative<double>(Value.Data);
    }
    static bool IsString(const GV2RuntimeCore::FValue& Value)
    {
        return std::holds_alternative<std::string>(Value.Data);
    }
    static bool IsArray(const GV2RuntimeCore::FValue& Value)
    {
        return std::holds_alternative<FArrayType>(Value.Data);
    }
    static bool IsObject(const GV2RuntimeCore::FValue& Value)
    {
        return std::holds_alternative<FObjectType>(Value.Data);
    }

    static bool AsBoolean(const GV2RuntimeCore::FValue& Value)
    {
        return std::get<bool>(Value.Data);
    }
    static std::int64_t AsInteger(const GV2RuntimeCore::FValue& Value)
    {
        return std::get<std::int64_t>(Value.Data);
    }
    static double AsNumber(const GV2RuntimeCore::FValue& Value)
    {
        return std::get<double>(Value.Data);
    }
    static std::string_view AsString(const GV2RuntimeCore::FValue& Value)
    {
        return std::get<std::string>(Value.Data);
    }
    static const FArrayType& AsArray(const GV2RuntimeCore::FValue& Value)
    {
        return std::get<FArrayType>(Value.Data);
    }
    static const FObjectType& AsObject(const GV2RuntimeCore::FValue& Value)
    {
        return std::get<FObjectType>(Value.Data);
    }
};

template <>
struct TValueTraits<GV2ContentCore::FValue>
{
    using FArrayType = GV2ContentCore::FValue::FArray;
    using FObjectType = GV2ContentCore::FValue::FObject;

    static bool IsNull(const GV2ContentCore::FValue& Value)
    {
        return Value.IsNull();
    }
    static bool IsBoolean(const GV2ContentCore::FValue& Value)
    {
        return Value.IsBoolean();
    }
    static bool IsInteger(const GV2ContentCore::FValue& Value)
    {
        return Value.IsInteger();
    }
    static bool IsNumber(const GV2ContentCore::FValue& Value)
    {
        return Value.IsNumber();
    }
    static bool IsString(const GV2ContentCore::FValue& Value)
    {
        return Value.IsString();
    }
    static bool IsArray(const GV2ContentCore::FValue& Value)
    {
        return Value.IsArray();
    }
    static bool IsObject(const GV2ContentCore::FValue& Value)
    {
        return Value.IsObject();
    }

    static bool AsBoolean(const GV2ContentCore::FValue& Value)
    {
        return Value.AsBoolean();
    }
    static std::int64_t AsInteger(const GV2ContentCore::FValue& Value)
    {
        return Value.AsInteger();
    }
    static double AsNumber(const GV2ContentCore::FValue& Value)
    {
        return Value.AsNumber();
    }
    static std::string_view AsString(const GV2ContentCore::FValue& Value)
    {
        return Value.AsString();
    }
    static const FArrayType& AsArray(const GV2ContentCore::FValue& Value)
    {
        return Value.AsArray();
    }
    static const FObjectType& AsObject(const GV2ContentCore::FValue& Value)
    {
        return Value.AsObject();
    }
};

template <typename TObject>
bool PushObjectRecursive(
    lua_State* State,
    const TObject& Object,
    int Depth,
    std::size_t& NodeCount,
    int MaxDepth,
    std::size_t MaxNodes,
    FRuntimeFault& OutFault);

template <typename TValue>
bool PushValueRecursive(
    lua_State* State,
    const TValue& Value,
    int Depth,
    std::size_t& NodeCount,
    int MaxDepth,
    std::size_t MaxNodes,
    FRuntimeFault& OutFault)
{
    if (Depth > MaxDepth || ++NodeCount > MaxNodes)
    {
        OutFault.Code = "PortableValueLimitExceeded";
        OutFault.Message = "Portable value exceeds runtime depth or node limits.";
        return false;
    }

    if (TValueTraits<TValue>::IsNull(Value))
    {
        FGV2LuaMarshaller::PushNull(State);
        return true;
    }
    if (TValueTraits<TValue>::IsBoolean(Value))
    {
        lua_pushboolean(State, TValueTraits<TValue>::AsBoolean(Value) ? 1 : 0);
        return true;
    }
    if (TValueTraits<TValue>::IsInteger(Value))
    {
        lua_pushinteger(State, static_cast<lua_Integer>(TValueTraits<TValue>::AsInteger(Value)));
        return true;
    }
    if (TValueTraits<TValue>::IsNumber(Value))
    {
        const double Number = TValueTraits<TValue>::AsNumber(Value);
        if (!std::isfinite(Number))
        {
            OutFault.Code = "PortableValueNonFinite";
            OutFault.Message = "Portable number must be finite.";
            return false;
        }
        lua_pushnumber(State, static_cast<lua_Number>(Number));
        return true;
    }
    if (TValueTraits<TValue>::IsString(Value))
    {
        const std::string_view Str = TValueTraits<TValue>::AsString(Value);
        lua_pushlstring(State, Str.data(), Str.size());
        return true;
    }
    if (TValueTraits<TValue>::IsArray(Value))
    {
        const auto& Array = TValueTraits<TValue>::AsArray(Value);
        lua_createtable(State, static_cast<int>(Array.size()), 0);
        for (std::size_t Index = 0; Index < Array.size(); ++Index)
        {
            if (!PushValueRecursive(State, Array[Index], Depth + 1, NodeCount, MaxDepth, MaxNodes, OutFault))
            {
                return false;
            }
            lua_rawseti(State, -2, static_cast<lua_Integer>(Index + 1));
        }
        return true;
    }
    if (TValueTraits<TValue>::IsObject(Value))
    {
        return PushObjectRecursive(
            State,
            TValueTraits<TValue>::AsObject(Value),
            Depth,
            NodeCount,
            MaxDepth,
            MaxNodes,
            OutFault);
    }

    OutFault.Code = "PortableValueFieldInvalid";
    OutFault.Message = "Portable value has an unknown variant state.";
    return false;
}

template <typename TObject>
bool PushObjectRecursive(
    lua_State* State,
    const TObject& Object,
    int Depth,
    std::size_t& NodeCount,
    int MaxDepth,
    std::size_t MaxNodes,
    FRuntimeFault& OutFault)
{
    lua_createtable(State, 0, static_cast<int>(Object.size()));
    for (const auto& [Name, Value] : Object)
    {
        if (Name.empty())
        {
            OutFault.Code = "PortableValueFieldInvalid";
            OutFault.Message = "Portable object contains an empty field name.";
            return false;
        }
        if (!PushValueRecursive(State, Value, Depth + 1, NodeCount, MaxDepth, MaxNodes, OutFault))
        {
            if (OutFault.Code.empty())
            {
                OutFault.Code = "PortableValueFieldInvalid";
                OutFault.Message = "Portable object contains an invalid field.";
            }
            return false;
        }
        lua_setfield(State, -2, Name.c_str());
    }
    return true;
}
} // namespace

void FGV2LuaMarshaller::PushNull(lua_State* State)
{
    lua_getglobal(State, "game");
    if (lua_istable(State, -1))
    {
        lua_getfield(State, -1, "null");
        lua_remove(State, -2);
    }
    else
    {
        lua_pop(State, 1);
        lua_pushnil(State);
    }
}

bool FGV2LuaMarshaller::PushValue(
    lua_State* State,
    const FValue& Value,
    FRuntimeFault& OutFault,
    const int MaxDepth,
    const std::size_t MaxNodes)
{
    std::size_t NodeCount = 0;
    return PushValueRecursive(State, Value, 0, NodeCount, MaxDepth, MaxNodes, OutFault);
}

bool FGV2LuaMarshaller::PushObject(
    lua_State* State,
    const FValue::FObject& Object,
    FRuntimeFault& OutFault,
    const int MaxDepth,
    const std::size_t MaxNodes)
{
    std::size_t NodeCount = 0;
    return PushObjectRecursive(State, Object, 0, NodeCount, MaxDepth, MaxNodes, OutFault);
}

bool FGV2LuaMarshaller::PushValue(
    lua_State* State,
    const GV2ContentCore::FValue& Value,
    FRuntimeFault& OutFault,
    const int MaxDepth,
    const std::size_t MaxNodes)
{
    std::size_t NodeCount = 0;
    return PushValueRecursive(State, Value, 0, NodeCount, MaxDepth, MaxNodes, OutFault);
}

bool FGV2LuaMarshaller::PushObject(
    lua_State* State,
    const GV2ContentCore::FValue::FObject& Object,
    FRuntimeFault& OutFault,
    const int MaxDepth,
    const std::size_t MaxNodes)
{
    std::size_t NodeCount = 0;
    return PushObjectRecursive(State, Object, 0, NodeCount, MaxDepth, MaxNodes, OutFault);
}

bool FGV2LuaMarshaller::ReadFlatScalarObject(
    lua_State* State,
    const int TableIndex,
    FValue::FObject& OutObject,
    FRuntimeFault& OutFault)
{
    const int AbsoluteIndex = lua_absindex(State, TableIndex);
    if (!lua_istable(State, AbsoluteIndex) || lua_rawlen(State, AbsoluteIndex) != 0)
    {
        OutFault = {"LuaScreenRequestInvalid", "Rich text span args must be an object."};
        return false;
    }

    OutObject.clear();
    lua_pushnil(State);
    while (lua_next(State, AbsoluteIndex) != 0)
    {
        if (lua_type(State, -2) != LUA_TSTRING || OutObject.size() >= 32)
        {
            OutFault = {"LuaScreenRequestInvalid", "Rich text span args require string keys and at most 32 fields."};
            lua_pop(State, 2);
            return false;
        }

        std::size_t KeyLength = 0;
        const char* KeyData = lua_tolstring(State, -2, &KeyLength);
        std::string Key(KeyData, KeyLength);
        if (Key.empty())
        {
            OutFault = {"LuaScreenRequestInvalid", "Rich text span args contain an empty field name."};
            lua_pop(State, 2);
            return false;
        }

        FValue Value;
        switch (lua_type(State, -1))
        {
        case LUA_TBOOLEAN:
            Value = FValue(lua_toboolean(State, -1) != 0);
            break;
        case LUA_TNUMBER:
            if (lua_isinteger(State, -1))
            {
                Value = FValue(static_cast<std::int64_t>(lua_tointeger(State, -1)));
            }
            else
            {
                const double Number = static_cast<double>(lua_tonumber(State, -1));
                if (!std::isfinite(Number))
                {
                    OutFault = {"LuaScreenRequestInvalid", "Rich text span args contain a non-finite number."};
                    lua_pop(State, 2);
                    return false;
                }
                Value = FValue(Number);
            }
            break;
        case LUA_TSTRING:
        {
            std::size_t StringLength = 0;
            const char* StringData = lua_tolstring(State, -1, &StringLength);
            Value = FValue(std::string(StringData, StringLength));
            break;
        }
        default:
            OutFault = {"LuaScreenRequestInvalid", "Rich text span args support only boolean, integer, number, and string values."};
            lua_pop(State, 2);
            return false;
        }
        OutObject.emplace(std::move(Key), std::move(Value));
        lua_pop(State, 1);
    }
    return true;
}

namespace Testing
{
std::string RunLuaMarshallerConformance()
{
    lua_State* State = luaL_newstate();
    if (State == nullptr)
    {
        return "lua_state_creation_failed";
    }

    luaL_requiref(State, "_G", luaopen_base, 1);
    lua_pop(State, 1);
    luaL_requiref(State, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(State, 1);

    // Initialize minimal game table with null sentinel
    lua_newtable(State);
    lua_newtable(State); // game.null sentinel
    lua_setfield(State, -2, "null");
    lua_setglobal(State, "game");

    FRuntimeFault Fault;

    // 1. Test GV2RuntimeCore::FValue types
    {
        // Null -> game.null
        GV2RuntimeCore::FValue NullVal;
        if (!FGV2LuaMarshaller::PushValue(State, NullVal, Fault))
        {
            lua_close(State);
            return "runtime_null_push_failed";
        }
        if (!lua_istable(State, -1))
        {
            lua_close(State);
            return "runtime_null_not_table";
        }
        lua_pop(State, 1);

        // Boolean
        GV2RuntimeCore::FValue BoolVal(true);
        if (!FGV2LuaMarshaller::PushValue(State, BoolVal, Fault) || !lua_isboolean(State, -1) || !lua_toboolean(State, -1))
        {
            lua_close(State);
            return "runtime_bool_push_failed";
        }
        lua_pop(State, 1);

        // Integer
        GV2RuntimeCore::FValue IntVal(static_cast<std::int64_t>(42));
        if (!FGV2LuaMarshaller::PushValue(State, IntVal, Fault) || !lua_isinteger(State, -1) || lua_tointeger(State, -1) != 42)
        {
            lua_close(State);
            return "runtime_int_push_failed";
        }
        lua_pop(State, 1);

        // Finite double
        GV2RuntimeCore::FValue DoubleVal(3.14159);
        if (!FGV2LuaMarshaller::PushValue(State, DoubleVal, Fault) || !lua_isnumber(State, -1) || lua_isinteger(State, -1))
        {
            lua_close(State);
            return "runtime_double_push_failed";
        }
        lua_pop(State, 1);

        // Non-finite double rejected
        GV2RuntimeCore::FValue InfVal(std::numeric_limits<double>::infinity());
        if (FGV2LuaMarshaller::PushValue(State, InfVal, Fault) || Fault.Code != "PortableValueNonFinite")
        {
            lua_close(State);
            return "runtime_inf_not_rejected";
        }

        // String
        GV2RuntimeCore::FValue StrVal(std::string("hello_world"));
        if (!FGV2LuaMarshaller::PushValue(State, StrVal, Fault) || !lua_isstring(State, -1) || std::string(lua_tostring(State, -1)) != "hello_world")
        {
            lua_close(State);
            return "runtime_str_push_failed";
        }
        lua_pop(State, 1);

        // Array
        GV2RuntimeCore::FValue::FArray Arr;
        Arr.emplace_back(GV2RuntimeCore::FValue(static_cast<std::int64_t>(1)));
        Arr.emplace_back(GV2RuntimeCore::FValue(static_cast<std::int64_t>(2)));
        GV2RuntimeCore::FValue ArrVal(std::move(Arr));
        if (!FGV2LuaMarshaller::PushValue(State, ArrVal, Fault) || !lua_istable(State, -1) || lua_rawlen(State, -1) != 2)
        {
            lua_close(State);
            return "runtime_arr_push_failed";
        }
        lua_pop(State, 1);

        // Object with canonical key order
        GV2RuntimeCore::FValue::FObject Obj;
        Obj.emplace("b_key", GV2RuntimeCore::FValue(static_cast<std::int64_t>(20)));
        Obj.emplace("a_key", GV2RuntimeCore::FValue(static_cast<std::int64_t>(10)));
        GV2RuntimeCore::FValue ObjVal(Obj);
        if (!FGV2LuaMarshaller::PushValue(State, ObjVal, Fault) || !lua_istable(State, -1))
        {
            lua_close(State);
            return "runtime_obj_push_failed";
        }
        lua_getfield(State, -1, "a_key");
        if (lua_tointeger(State, -1) != 10)
        {
            lua_close(State);
            return "runtime_obj_field_a_failed";
        }
        lua_pop(State, 1);
        lua_getfield(State, -1, "b_key");
        if (lua_tointeger(State, -1) != 20)
        {
            lua_close(State);
            return "runtime_obj_field_b_failed";
        }
        lua_pop(State, 2);

        // Empty field name rejected
        GV2RuntimeCore::FValue::FObject BadObj;
        BadObj.emplace("", GV2RuntimeCore::FValue(static_cast<std::int64_t>(1)));
        GV2RuntimeCore::FValue BadObjVal(std::move(BadObj));
        if (FGV2LuaMarshaller::PushValue(State, BadObjVal, Fault) || Fault.Code != "PortableValueFieldInvalid")
        {
            lua_close(State);
            return "runtime_empty_field_not_rejected";
        }

        // Limit exceeded
        if (FGV2LuaMarshaller::PushValue(State, ObjVal, Fault, 0, 1000) || Fault.Code != "PortableValueLimitExceeded")
        {
            lua_close(State);
            return "runtime_depth_limit_not_enforced";
        }
    }

    // 2. Test GV2ContentCore::FValue types
    {
        // Null
        GV2ContentCore::FValue NullVal = GV2ContentCore::FValue::MakeNull();
        if (!FGV2LuaMarshaller::PushValue(State, NullVal, Fault) || !lua_istable(State, -1))
        {
            lua_close(State);
            return "content_null_push_failed";
        }
        lua_pop(State, 1);

        // Boolean
        GV2ContentCore::FValue BoolVal = GV2ContentCore::FValue::MakeBoolean(false);
        if (!FGV2LuaMarshaller::PushValue(State, BoolVal, Fault) || !lua_isboolean(State, -1) || lua_toboolean(State, -1) != 0)
        {
            lua_close(State);
            return "content_bool_push_failed";
        }
        lua_pop(State, 1);

        // Integer
        GV2ContentCore::FValue IntVal = GV2ContentCore::FValue::MakeInteger(100);
        if (!FGV2LuaMarshaller::PushValue(State, IntVal, Fault) || !lua_isinteger(State, -1) || lua_tointeger(State, -1) != 100)
        {
            lua_close(State);
            return "content_int_push_failed";
        }
        lua_pop(State, 1);

        // Number
        GV2ContentCore::FValue NumVal = GV2ContentCore::FValue::MakeNumber(2.718);
        if (!FGV2LuaMarshaller::PushValue(State, NumVal, Fault) || !lua_isnumber(State, -1) || lua_isinteger(State, -1))
        {
            lua_close(State);
            return "content_num_push_failed";
        }
        lua_pop(State, 1);

        // String
        GV2ContentCore::FValue StrVal = GV2ContentCore::FValue::MakeString("content_string");
        if (!FGV2LuaMarshaller::PushValue(State, StrVal, Fault) || !lua_isstring(State, -1) || std::string(lua_tostring(State, -1)) != "content_string")
        {
            lua_close(State);
            return "content_str_push_failed";
        }
        lua_pop(State, 1);

        // Array
        GV2ContentCore::FValue ArrVal = GV2ContentCore::FValue::MakeArray({
            GV2ContentCore::FValue::MakeInteger(10),
            GV2ContentCore::FValue::MakeInteger(20)
        });
        if (!FGV2LuaMarshaller::PushValue(State, ArrVal, Fault) || !lua_istable(State, -1) || lua_rawlen(State, -1) != 2)
        {
            lua_close(State);
            return "content_arr_push_failed";
        }
        lua_pop(State, 1);

        // Object
        GV2ContentCore::FValue ObjVal = GV2ContentCore::FValue::MakeObject({
            {"name", GV2ContentCore::FValue::MakeString("Iron Sword")},
            {"weight", GV2ContentCore::FValue::MakeInteger(5)}
        });
        if (!FGV2LuaMarshaller::PushValue(State, ObjVal, Fault) || !lua_istable(State, -1))
        {
            lua_close(State);
            return "content_obj_push_failed";
        }
        lua_getfield(State, -1, "name");
        if (std::string(lua_tostring(State, -1)) != "Iron Sword")
        {
            lua_close(State);
            return "content_obj_name_field_failed";
        }
        lua_pop(State, 1);
        lua_getfield(State, -1, "weight");
        if (lua_tointeger(State, -1) != 5)
        {
            lua_close(State);
            return "content_obj_weight_field_failed";
        }
        lua_pop(State, 2);

        // Limit exceeded
        if (FGV2LuaMarshaller::PushValue(State, ObjVal, Fault, 100, 1) || Fault.Code != "PortableValueLimitExceeded")
        {
            lua_close(State);
            return "content_node_limit_not_enforced";
        }
    }

    // 3. Test ReadFlatScalarObject
    {
        lua_newtable(State);
        lua_pushboolean(State, 1);
        lua_setfield(State, -2, "flag");
        lua_pushinteger(State, 7);
        lua_setfield(State, -2, "count");
        lua_pushstring(State, "val");
        lua_setfield(State, -2, "text");

        GV2RuntimeCore::FValue::FObject ReadObj;
        if (!FGV2LuaMarshaller::ReadFlatScalarObject(State, -1, ReadObj, Fault) || ReadObj.size() != 3)
        {
            lua_close(State);
            return "read_flat_scalar_failed";
        }
        lua_pop(State, 1);
    }

    lua_close(State);
    return {};
}
} // namespace Testing
} // namespace GV2RuntimeCore
