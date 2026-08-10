#include "GV2RuntimeCore/GV2RuntimeSession.h"

extern "C"
{
#include "lauxlib.h"
#include "lualib.h"
}

#include <cassert>
#include <cmath>
#include <thread>

static_assert(LUA_VERSION_RELEASE_NUM == GV2RuntimeCore::FRuntimeSession::LuaReleaseNumber);

namespace GV2RuntimeCore
{
namespace
{
constexpr char TestRuntimeSource[] = R"lua(
assert(io == nil and os == nil and debug == nil and coroutine == nil and package == nil)
assert(load == nil and loadfile == nil and dofile == nil)
assert(math.random == nil and math.randomseed == nil)

function game.runtime.dispatch_command(request)
    assert(type(request) == "table", "command request must be a table")
    assert(type(request.command_id) == "string" and request.command_id ~= "", "command_id is required")
    assert(type(request.args) == "table", "args table is required")
    assert(type(request.sequence) == "number", "sequence is required")
    if request.command_id == "core:command.test.force_error" then
        error("forced test runtime error")
    end
    game.runtime.last_sequence = request.sequence
    game.runtime.command_count = (game.runtime.command_count or 0) + 1
    return request.sequence
end

function game.runtime.dispatch_semantic_input(input)
    assert(type(input) == "table", "semantic input must be a table")
    assert(input.session_generation == game.runtime.session_generation, "session generation mismatch")
    return game.runtime.dispatch_command({
        command_id = input.command_id,
        args = input.args,
        sequence = input.sequence,
        source = "semantic_input",
    })
end
)lua";

constexpr int MaxValueDepth = 64;
constexpr std::size_t MaxValueNodes = 10000;

struct FStackRestore
{
    lua_State* State;
    int Top;

    ~FStackRestore()
    {
        lua_settop(State, Top);
    }
};

struct FExecutionGuard
{
    bool& Flag;

    explicit FExecutionGuard(bool& InFlag) : Flag(InFlag)
    {
        assert(!Flag);
        Flag = true;
    }

    ~FExecutionGuard()
    {
        Flag = false;
    }
};

void PushString(lua_State* State, const std::string& Value)
{
    lua_pushlstring(State, Value.data(), Value.size());
}

void RemoveGlobal(lua_State* State, const char* Name)
{
    lua_pushnil(State);
    lua_setglobal(State, Name);
}

void RemoveTableField(lua_State* State, const char* TableName, const char* FieldName)
{
    lua_getglobal(State, TableName);
    assert(lua_istable(State, -1));
    lua_pushnil(State);
    lua_setfield(State, -2, FieldName);
    lua_pop(State, 1);
}

int Traceback(lua_State* State)
{
    const char* Message = lua_tostring(State, 1);
    luaL_traceback(State, State, Message != nullptr ? Message : "Lua error", 1);
    return 1;
}

void ReadLuaError(lua_State* State, const char* Code, const char* Fallback, FRuntimeFault& OutFault)
{
    OutFault.Code = Code;
    const char* Message = lua_tostring(State, -1);
    OutFault.Message = Message != nullptr ? Message : Fallback;
}

bool PushValue(
    lua_State* State,
    const FValue& Value,
    const int Depth,
    std::size_t& NodeCount,
    FRuntimeFault& OutFault);

bool PushObject(
    lua_State* State,
    const FValue::FObject& Object,
    const int Depth,
    std::size_t& NodeCount,
    FRuntimeFault& OutFault)
{
    lua_createtable(State, 0, static_cast<int>(Object.size()));
    for (const auto& [Name, Value] : Object)
    {
        if (Name.empty() || !PushValue(State, Value, Depth + 1, NodeCount, OutFault))
        {
            if (OutFault.Code.empty())
            {
                OutFault.Code = "PortableValueFieldInvalid";
                OutFault.Message = "Portable object contains an empty field name.";
            }
            return false;
        }
        lua_setfield(State, -2, Name.c_str());
    }
    return true;
}

bool PushValue(
    lua_State* State,
    const FValue& Value,
    const int Depth,
    std::size_t& NodeCount,
    FRuntimeFault& OutFault)
{
    if (Depth > MaxValueDepth || ++NodeCount > MaxValueNodes)
    {
        OutFault.Code = "PortableValueLimitExceeded";
        OutFault.Message = "Portable value exceeds runtime depth or node limits.";
        return false;
    }

    if (std::holds_alternative<std::monostate>(Value.Data))
    {
        lua_getglobal(State, "game");
        lua_getfield(State, -1, "null");
        lua_remove(State, -2);
        return true;
    }
    if (const bool* Boolean = std::get_if<bool>(&Value.Data))
    {
        lua_pushboolean(State, *Boolean);
        return true;
    }
    if (const std::int64_t* Integer = std::get_if<std::int64_t>(&Value.Data))
    {
        lua_pushinteger(State, static_cast<lua_Integer>(*Integer));
        return true;
    }
    if (const double* Number = std::get_if<double>(&Value.Data))
    {
        if (!std::isfinite(*Number))
        {
            OutFault.Code = "PortableValueNonFinite";
            OutFault.Message = "Portable number must be finite.";
            return false;
        }
        lua_pushnumber(State, static_cast<lua_Number>(*Number));
        return true;
    }
    if (const std::string* String = std::get_if<std::string>(&Value.Data))
    {
        PushString(State, *String);
        return true;
    }
    if (const FValue::FArray* Array = std::get_if<FValue::FArray>(&Value.Data))
    {
        lua_createtable(State, static_cast<int>(Array->size()), 0);
        for (std::size_t Index = 0; Index < Array->size(); ++Index)
        {
            if (!PushValue(State, (*Array)[Index], Depth + 1, NodeCount, OutFault))
            {
                return false;
            }
            lua_rawseti(State, -2, static_cast<lua_Integer>(Index + 1));
        }
        return true;
    }
    return PushObject(State, std::get<FValue::FObject>(Value.Data), Depth, NodeCount, OutFault);
}
}

struct FRuntimeSession::FImpl
{
    lua_State* State = nullptr;
    std::thread::id OwnerThread;
    std::int32_t SessionGeneration = 0;
    bool bExecuting = false;

    bool IsOwnerThread() const
    {
        return State == nullptr || OwnerThread == std::this_thread::get_id();
    }

    bool BeginEntry(const char* EntryPoint, FRuntimeFault& OutFault)
    {
        OutFault = {};
        if (State == nullptr)
        {
            OutFault = {"LuaVmNotStarted", std::string(EntryPoint) + " requires a started VM."};
            return false;
        }
        if (!IsOwnerThread())
        {
            OutFault = {"RuntimeWrongThread", std::string(EntryPoint) + " must execute on the session owner thread."};
            return false;
        }
        if (bExecuting)
        {
            OutFault = {"LuaReentryRejected", std::string(EntryPoint) + " attempted synchronous re-entry."};
            return false;
        }
        return true;
    }

    bool OpenEnvironment(FRuntimeFault& OutFault)
    {
        luaL_requiref(State, LUA_GNAME, luaopen_base, 1);
        lua_pop(State, 1);
        luaL_requiref(State, LUA_TABLIBNAME, luaopen_table, 1);
        lua_pop(State, 1);
        luaL_requiref(State, LUA_STRLIBNAME, luaopen_string, 1);
        lua_pop(State, 1);
        luaL_requiref(State, LUA_MATHLIBNAME, luaopen_math, 1);
        lua_pop(State, 1);
        luaL_requiref(State, LUA_UTF8LIBNAME, luaopen_utf8, 1);
        lua_pop(State, 1);

        RemoveGlobal(State, "load");
        RemoveGlobal(State, "loadfile");
        RemoveGlobal(State, "dofile");
        RemoveTableField(State, LUA_MATHLIBNAME, "random");
        RemoveTableField(State, LUA_MATHLIBNAME, "randomseed");

        lua_createtable(State, 0, 2);
        lua_createtable(State, 0, 3);
        lua_pushinteger(State, SessionGeneration);
        lua_setfield(State, -2, "session_generation");
        lua_setfield(State, -2, "runtime");
        lua_createtable(State, 0, 0);
        lua_setfield(State, -2, "null");
        lua_setglobal(State, "game");

        if (lua_gettop(State) != 0)
        {
            lua_settop(State, 0);
            OutFault = {"LuaStackImbalance", "Lua environment initialization left an unbalanced stack."};
            return false;
        }
        return true;
    }

    bool ExecuteSource(const char* Source, std::size_t Length, const char* Name, FRuntimeFault& OutFault)
    {
        if (!BeginEntry("bootstrap", OutFault))
        {
            return false;
        }

        FStackRestore Stack{State, lua_gettop(State)};
        FExecutionGuard Execution(bExecuting);
        lua_pushcfunction(State, Traceback);
        const int ErrorHandler = lua_gettop(State);

        if (luaL_loadbufferx(State, Source, Length, Name, "t") != LUA_OK)
        {
            ReadLuaError(State, "LuaSourceLoadError", "Lua source failed to load.", OutFault);
            return false;
        }
        if (lua_pcall(State, 0, 0, ErrorHandler) != LUA_OK)
        {
            ReadLuaError(State, "LuaBootstrapError", "Lua bootstrap failed.", OutFault);
            return false;
        }
        return true;
    }

    bool CallDispatcher(
        const char* FunctionName,
        const FValue::FObject& Envelope,
        std::int64_t ExpectedSequence,
        FRuntimeFault& OutFault)
    {
        if (!BeginEntry(FunctionName, OutFault))
        {
            return false;
        }

        FStackRestore Stack{State, lua_gettop(State)};
        FExecutionGuard Execution(bExecuting);
        lua_pushcfunction(State, Traceback);
        const int ErrorHandler = lua_gettop(State);

        lua_getglobal(State, "game");
        lua_getfield(State, -1, "runtime");
        lua_getfield(State, -1, FunctionName);
        lua_remove(State, -2);
        lua_remove(State, -2);
        if (!lua_isfunction(State, -1))
        {
            OutFault = {"LuaEntryPointMissing", std::string("Fixed entry point is missing: ") + FunctionName};
            return false;
        }

        std::size_t NodeCount = 0;
        if (!PushObject(State, Envelope, 0, NodeCount, OutFault))
        {
            return false;
        }
        if (lua_pcall(State, 1, 1, ErrorHandler) != LUA_OK)
        {
            ReadLuaError(State, "LuaDispatchError", "Lua dispatcher failed.", OutFault);
            return false;
        }

        int IsInteger = 0;
        const lua_Integer Returned = lua_tointegerx(State, -1, &IsInteger);
        if (IsInteger == 0 || Returned != ExpectedSequence)
        {
            OutFault = {"LuaDispatchResultInvalid", "Lua dispatcher returned an invalid sequence."};
            return false;
        }
        return true;
    }
};

FRuntimeSession::FRuntimeSession() : Impl(std::make_unique<FImpl>())
{
}

FRuntimeSession::~FRuntimeSession()
{
    Stop();
}

bool FRuntimeSession::StartTestRuntime(
    const std::int32_t InSessionGeneration,
    FRuntimeFault& OutFault)
{
    Stop();
    OutFault = {};
    if (InSessionGeneration <= 0)
    {
        OutFault = {"InvalidSessionGeneration", "Runtime requires a positive session generation."};
        return false;
    }

    Impl->State = luaL_newstate();
    if (Impl->State == nullptr)
    {
        OutFault = {"LuaVmAllocationFailed", "Lua VM allocation failed."};
        return false;
    }
    Impl->OwnerThread = std::this_thread::get_id();
    Impl->SessionGeneration = InSessionGeneration;

    if (!Impl->OpenEnvironment(OutFault)
        || !Impl->ExecuteSource(
            TestRuntimeSource,
            sizeof(TestRuntimeSource) - 1,
            "@core/test_runtime.lua",
            OutFault))
    {
        Stop();
        return false;
    }
    return true;
}

void FRuntimeSession::Stop()
{
    assert(Impl != nullptr);
    assert(Impl->IsOwnerThread());
    assert(!Impl->bExecuting);
    if (Impl->State != nullptr)
    {
        lua_close(Impl->State);
        Impl->State = nullptr;
    }
    Impl->SessionGeneration = 0;
    Impl->OwnerThread = {};
}

bool FRuntimeSession::DispatchSemanticInput(
    const FSemanticInput& Input,
    FRuntimeFault& OutFault)
{
    if (Input.SessionGeneration != Impl->SessionGeneration)
    {
        OutFault = {"StaleSessionGeneration", "Semantic input belongs to a different session generation."};
        return false;
    }

    FValue::FArray NodePath;
    NodePath.reserve(Input.NodeKeyPath.size());
    for (const std::string& Segment : Input.NodeKeyPath)
    {
        NodePath.emplace_back(Segment);
    }

    FValue::FObject Envelope{
        {"session_generation", FValue(static_cast<std::int64_t>(Input.SessionGeneration))},
        {"ui_instance_id", FValue(Input.UiInstanceId)},
        {"revision", FValue(Input.Revision)},
        {"sequence", FValue(Input.Sequence)},
        {"node_key_path", FValue(std::move(NodePath))},
        {"command_id", FValue(Input.CommandId)},
        {"args", FValue(Input.Args)},
    };
    if (!Input.ElementId.empty())
    {
        Envelope.emplace("element_id", FValue(Input.ElementId));
    }
    return Impl->CallDispatcher("dispatch_semantic_input", Envelope, Input.Sequence, OutFault);
}

bool FRuntimeSession::DispatchCommand(
    const FCommandRequest& Request,
    FRuntimeFault& OutFault)
{
    FValue::FObject Envelope{
        {"command_id", FValue(Request.CommandId)},
        {"args", FValue(Request.Args)},
        {"sequence", FValue(Request.Sequence)},
        {"source", FValue(std::string("headless"))},
    };
    return Impl->CallDispatcher("dispatch_command", Envelope, Request.Sequence, OutFault);
}

bool FRuntimeSession::IsStarted() const
{
    return Impl->State != nullptr;
}

bool FRuntimeSession::IsExecuting() const
{
    return Impl->bExecuting;
}

std::int32_t FRuntimeSession::GetSessionGeneration() const
{
    return Impl->SessionGeneration;
}
}
