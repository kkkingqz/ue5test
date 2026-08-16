#include "GV2RuntimeCore/GV2HostServices.h"
#include "GV2RuntimeCore/GV2LuaMarshaller.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"
#include "GV2RuntimeCore/GV2StableId.h"

extern "C"
{
#include "lauxlib.h"
#include "lualib.h"
}

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <set>
#include <string_view>
#include <thread>

static_assert(LUA_VERSION_RELEASE_NUM == GV2RuntimeCore::FRuntimeSession::LuaReleaseNumber);

namespace GV2RuntimeCore
{
namespace
{
constexpr int MaxValueDepth = 64;
constexpr std::size_t MaxValueNodes = 10000;
constexpr const char* SessionImplRegistryKey = "GV2.SessionImpl";
constexpr const char* ModuleManifestSourceName = "@Scripts/bootstrap/manifest.lua";
constexpr const char* LoadedModulesRegistryKey = "GV2.LoadedModules";
constexpr const char* AllowedDependenciesRegistryKey = "GV2.AllowedDependencies";
constexpr const char* CurrentModuleRegistryKey = "GV2.CurrentModule";

struct FModuleSpec
{
    std::string ModuleId;
    std::string SourceName;
    std::vector<std::string> Dependencies;
};

bool IsCanonicalModuleSourcePath(const std::string_view Value)
{
    if (Value.empty() || Value.size() > 240 || Value.front() == '/'
        || !Value.ends_with(".lua") || Value.find("..") != std::string_view::npos
        || Value.find("//") != std::string_view::npos
        || Value.find('\\') != std::string_view::npos)
    {
        return false;
    }
    for (const char Character : Value)
    {
        const bool bIsLowercaseLetter = Character >= 'a' && Character <= 'z';
        const bool bIsDigit = Character >= '0' && Character <= '9';
        if (!bIsLowercaseLetter && !bIsDigit
            && Character != '_' && Character != '/' && Character != '.')
        {
            return false;
        }
    }
    const std::string_view Stem = Value.substr(0, Value.size() - 4);
    std::size_t SegmentStart = 0;
    while (SegmentStart <= Stem.size())
    {
        const std::size_t Slash = Stem.find('/', SegmentStart);
        const std::size_t SegmentEnd = Slash == std::string_view::npos ? Stem.size() : Slash;
        if (!FStableId::IsValidSegment(Stem.substr(SegmentStart, SegmentEnd - SegmentStart)))
        {
            return false;
        }
        if (Slash == std::string_view::npos)
        {
            break;
        }
        SegmentStart = Slash + 1;
    }
    return true;
}

struct FStackRestore
{
    lua_State* State;
    int Top;

    ~FStackRestore()
    {
        lua_settop(State, Top);
    }
};

// TAS-02: while a Lua spec chunk and its cases run, `require()` is allowed
// for any already-loaded module (CurrentModuleRegistryKey/
// AllowedDependenciesRegistryKey are set up by the caller). This guard
// clears both registry keys back to nil on every exit path, mirroring
// ExecuteModule's clear-after-load behavior so a spec can never leave
// `require` open beyond its own execution.
struct FRequireContextGuard
{
    lua_State* State;

    ~FRequireContextGuard()
    {
        lua_pushnil(State);
        lua_setfield(State, LUA_REGISTRYINDEX, CurrentModuleRegistryKey);
        lua_pushnil(State);
        lua_setfield(State, LUA_REGISTRYINDEX, AllowedDependenciesRegistryKey);
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
}

struct FRuntimeSession::FImpl
{
    lua_State* State = nullptr;
    std::thread::id OwnerThread;
    std::int32_t SessionGeneration = 0;
    GV2ContentCore::FRepositoryReadHandle PinnedRepository;
    bool bExecuting = false;
    ISaveSlotStorage* SaveSlotStorage = nullptr;

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

        lua_pushlightuserdata(State, this);
        lua_setfield(State, LUA_REGISTRYINDEX, SessionImplRegistryKey);

        lua_createtable(State, 0, 8);
        lua_createtable(State, 0, 3);
        lua_pushinteger(State, SessionGeneration);
        lua_setfield(State, -2, "session_generation");
        lua_setfield(State, -2, "runtime");
        lua_createtable(State, 0, 1);
        lua_setfield(State, -2, "ui");
        lua_createtable(State, 0, 2);
        lua_setfield(State, -2, "debug");
        lua_createtable(State, 0, 0);
        lua_setfield(State, -2, "null");
        lua_createtable(State, 0, 4);
        lua_setfield(State, -2, "instances");
        lua_createtable(State, 0, 4);
        lua_setfield(State, -2, "services");

        // game.repository
        lua_createtable(State, 0, 4);
        lua_pushcfunction(State, &FImpl::RepositoryGet);
        lua_setfield(State, -2, "get");
        lua_pushcfunction(State, &FImpl::RepositoryRequire);
        lua_setfield(State, -2, "require");
        lua_pushcfunction(State, &FImpl::RepositoryList);
        lua_setfield(State, -2, "list");
        lua_pushcfunction(State, &FImpl::RepositoryExists);
        lua_setfield(State, -2, "exists");

        // Make game.repository read-only
        lua_createtable(State, 0, 2);
        lua_pushcfunction(State, &FImpl::ReadOnlyTableError);
        lua_setfield(State, -2, "__newindex");
        lua_pushboolean(State, 0);
        lua_setfield(State, -2, "__metatable");
        lua_setmetatable(State, -2);

        lua_setfield(State, -2, "repository");

        // game.save_slots (SAV-05/06/10): the one host capability Lua's
        // core:module.runtime.save uses to publish a save container. Only
        // "write" is exposed — reading a slot back is out of scope until
        // Cold Start Load (M4).
        lua_createtable(State, 0, 1);
        lua_pushcfunction(State, &FImpl::SaveSlotsWrite);
        lua_setfield(State, -2, "write");
        lua_createtable(State, 0, 2);
        lua_pushcfunction(State, &FImpl::ReadOnlyTableError);
        lua_setfield(State, -2, "__newindex");
        lua_pushboolean(State, 0);
        lua_setfield(State, -2, "__metatable");
        lua_setmetatable(State, -2);
        lua_setfield(State, -2, "save_slots");

        lua_setglobal(State, "game");

        lua_createtable(State, 0, 16);
        lua_setfield(State, LUA_REGISTRYINDEX, LoadedModulesRegistryKey);
        lua_pushcfunction(State, &FImpl::RequireModule);
        lua_setglobal(State, "require");

        if (lua_gettop(State) != 0)
        {
            lua_settop(State, 0);
            OutFault = {"LuaStackImbalance", "Lua environment initialization left an unbalanced stack."};
            return false;
        }
        return true;
    }

    static int RequireModule(lua_State* InState)
    {
        std::size_t ModuleIdLength = 0;
        const char* ModuleId = luaL_checklstring(InState, 1, &ModuleIdLength);

        lua_getfield(InState, LUA_REGISTRYINDEX, CurrentModuleRegistryKey);
        if (lua_type(InState, -1) != LUA_TSTRING)
        {
            return luaL_error(InState, "require is allowed only during declared module initialization");
        }
        lua_pop(InState, 1);

        lua_getfield(InState, LUA_REGISTRYINDEX, AllowedDependenciesRegistryKey);
        lua_pushlstring(InState, ModuleId, ModuleIdLength);
        lua_gettable(InState, -2);
        const bool bAllowed = lua_toboolean(InState, -1) != 0;
        lua_pop(InState, 2);
        if (!bAllowed)
        {
            return luaL_error(InState, "module import is not declared in dependencies: %s", ModuleId);
        }

        lua_getfield(InState, LUA_REGISTRYINDEX, LoadedModulesRegistryKey);
        lua_pushlstring(InState, ModuleId, ModuleIdLength);
        lua_gettable(InState, -2);
        lua_remove(InState, -2);
        if (lua_isnil(InState, -1))
        {
            return luaL_error(InState, "declared module dependency is not loaded: %s", ModuleId);
        }
        return 1;
    }

    static FImpl* GetSessionImpl(lua_State* InState)
    {
        lua_getfield(InState, LUA_REGISTRYINDEX, SessionImplRegistryKey);
        void* Ptr = lua_touserdata(InState, -1);
        lua_pop(InState, 1);
        return static_cast<FImpl*>(Ptr);
    }

    static int ReadOnlyTableError(lua_State* InState)
    {
        return luaL_error(InState, "attempt to modify read-only table: game.repository");
    }

    // SAV-05/06/10: game.save_slots.write(slot_id, bytes) -> ok, err_code.
    // A pure pass-through to ISaveSlotStorage::WriteSlot — no path, no
    // filesystem type, no interpretation of bytes crosses this call in
    // either direction, matching ISaveSlotStorage (GV2HostServices.h).
    static int SaveSlotsWrite(lua_State* InState)
    {
        if (lua_gettop(InState) < 2
            || lua_type(InState, 1) != LUA_TSTRING
            || lua_type(InState, 2) != LUA_TSTRING)
        {
            return luaL_error(InState, "invalid_arguments: save_slots.write expects (slot_id: string, bytes: string)");
        }

        std::size_t SlotIdLength = 0;
        const char* SlotIdRaw = lua_tolstring(InState, 1, &SlotIdLength);
        std::size_t BytesLength = 0;
        const char* BytesRaw = lua_tolstring(InState, 2, &BytesLength);

        FImpl* Impl = GetSessionImpl(InState);
        if (Impl == nullptr || Impl->SaveSlotStorage == nullptr)
        {
            lua_pushboolean(InState, 0);
            lua_pushstring(InState, "unavailable");
            return 2;
        }

        const FSaveSlotWriteResult Result = Impl->SaveSlotStorage->WriteSlot(
            std::string(SlotIdRaw, SlotIdLength),
            std::string(BytesRaw, BytesLength));
        if (Result.Result == ESaveSlotResult::Ok)
        {
            lua_pushboolean(InState, 1);
            lua_pushnil(InState);
            return 2;
        }

        const char* Code = "failure";
        if (Result.Result == ESaveSlotResult::NotFound)
        {
            Code = "not_found";
        }
        else if (Result.Result == ESaveSlotResult::Unreadable)
        {
            Code = "unreadable";
        }
        lua_pushboolean(InState, 0);
        lua_pushstring(InState, Code);
        return 2;
    }

    static int RepositoryGet(lua_State* InState)
    {
        if (lua_gettop(InState) < 1 || lua_type(InState, 1) != LUA_TSTRING)
        {
            lua_pushnil(InState);
            lua_createtable(InState, 0, 2);
            lua_pushstring(InState, "invalid_id");
            lua_setfield(InState, -2, "code");
            lua_pushstring(InState, "repository.get expects a string definition ID parameter");
            lua_setfield(InState, -2, "message");
            return 2;
        }

        std::size_t IdLength = 0;
        const char* RawId = lua_tolstring(InState, 1, &IdLength);
        std::string_view IdView(RawId, IdLength);
        auto ParsedId = GV2ContentCore::FDefinitionId::Parse(IdView);
        if (!ParsedId.has_value())
        {
            lua_pushnil(InState);
            lua_createtable(InState, 0, 2);
            lua_pushstring(InState, "invalid_id");
            lua_setfield(InState, -2, "code");
            lua_pushlstring(InState, RawId, IdLength);
            lua_setfield(InState, -2, "requested_id");
            return 2;
        }

        FImpl* Impl = GetSessionImpl(InState);
        if (Impl == nullptr || !Impl->PinnedRepository.IsValid())
        {
            lua_pushnil(InState);
            lua_createtable(InState, 0, 2);
            lua_pushstring(InState, "invalid_handle");
            lua_setfield(InState, -2, "code");
            lua_pushlstring(InState, RawId, IdLength);
            lua_setfield(InState, -2, "requested_id");
            return 2;
        }

        auto QueryResult = Impl->PinnedRepository.Require(*ParsedId);
        if (QueryResult.Definition != nullptr)
        {
            GV2RuntimeCore::FRuntimeFault PushFault;
            if (!FGV2LuaMarshaller::PushValue(InState, *QueryResult.Definition, PushFault))
            {
                lua_pushnil(InState);
                lua_createtable(InState, 0, 2);
                lua_pushstring(InState, PushFault.Code.c_str());
                lua_setfield(InState, -2, "code");
                lua_pushstring(InState, PushFault.Message.c_str());
                lua_setfield(InState, -2, "message");
                return 2;
            }
            lua_pushnil(InState);
            return 2;
        }

        lua_pushnil(InState);
        lua_createtable(InState, 0, 3);
        std::string Code = QueryResult.Error ? QueryResult.Error->Code : "not_found";
        if (Code.rfind("core:diagnostic.repository.read.", 0) == 0)
        {
            Code = Code.substr(std::string("core:diagnostic.repository.read.").length());
        }
        lua_pushstring(InState, Code.c_str());
        lua_setfield(InState, -2, "code");
        lua_pushlstring(InState, RawId, IdLength);
        lua_setfield(InState, -2, "requested_id");
        if (QueryResult.Error && QueryResult.Error->CanonicalId)
        {
            lua_pushstring(InState, QueryResult.Error->CanonicalId->c_str());
            lua_setfield(InState, -2, "canonical_id");
        }
        return 2;
    }

    static int RepositoryRequire(lua_State* InState)
    {
        if (lua_gettop(InState) < 1 || lua_type(InState, 1) != LUA_TSTRING)
        {
            return luaL_error(InState, "invalid_id: repository.require expects a string definition ID parameter");
        }

        std::size_t IdLength = 0;
        const char* RawId = lua_tolstring(InState, 1, &IdLength);
        std::string_view IdView(RawId, IdLength);
        auto ParsedId = GV2ContentCore::FDefinitionId::Parse(IdView);
        if (!ParsedId.has_value())
        {
            return luaL_error(InState, "invalid_id: invalid definition ID '%s'", RawId);
        }

        FImpl* Impl = GetSessionImpl(InState);
        if (Impl == nullptr || !Impl->PinnedRepository.IsValid())
        {
            return luaL_error(InState, "invalid_handle: pinned repository read handle is invalid");
        }

        auto QueryResult = Impl->PinnedRepository.Require(*ParsedId);
        if (QueryResult.Definition != nullptr)
        {
            GV2RuntimeCore::FRuntimeFault PushFault;
            if (!FGV2LuaMarshaller::PushValue(InState, *QueryResult.Definition, PushFault))
            {
                return luaL_error(InState, "%s: %s", PushFault.Code.c_str(), PushFault.Message.c_str());
            }
            return 1;
        }

        std::string Code = QueryResult.Error ? QueryResult.Error->Code : "not_found";
        if (Code.rfind("core:diagnostic.repository.read.", 0) == 0)
        {
            Code = Code.substr(std::string("core:diagnostic.repository.read.").length());
        }
        return luaL_error(InState, "%s: definition '%s' not available in repository", Code.c_str(), RawId);
    }

    static int RepositoryList(lua_State* InState)
    {
        if (lua_gettop(InState) < 1 || lua_type(InState, 1) != LUA_TSTRING)
        {
            lua_createtable(InState, 0, 0);
            return 1;
        }

        std::size_t KindLength = 0;
        const char* RawKind = lua_tolstring(InState, 1, &KindLength);
        std::string_view KindView(RawKind, KindLength);

        FImpl* Impl = GetSessionImpl(InState);
        if (Impl == nullptr || !Impl->PinnedRepository.IsValid())
        {
            lua_createtable(InState, 0, 0);
            return 1;
        }

        const std::vector<GV2ContentCore::FDefinitionId> List = Impl->PinnedRepository.List(KindView);
        lua_createtable(InState, static_cast<int>(List.size()), 0);
        for (std::size_t i = 0; i < List.size(); ++i)
        {
            lua_pushstring(InState, List[i].ToString().c_str());
            lua_rawseti(InState, -2, static_cast<lua_Integer>(i + 1));
        }
        return 1;
    }

    static int RepositoryExists(lua_State* InState)
    {
        if (lua_gettop(InState) < 1 || lua_type(InState, 1) != LUA_TSTRING)
        {
            lua_pushboolean(InState, 0);
            return 1;
        }

        std::size_t IdLength = 0;
        const char* RawId = lua_tolstring(InState, 1, &IdLength);
        std::string_view IdView(RawId, IdLength);
        auto ParsedId = GV2ContentCore::FDefinitionId::Parse(IdView);
        if (!ParsedId.has_value())
        {
            lua_pushboolean(InState, 0);
            return 1;
        }

        FImpl* Impl = GetSessionImpl(InState);
        if (Impl == nullptr || !Impl->PinnedRepository.IsValid())
        {
            lua_pushboolean(InState, 0);
            return 1;
        }

        const bool bExists = (Impl->PinnedRepository.Find(*ParsedId) != nullptr);
        lua_pushboolean(InState, bExists ? 1 : 0);
        return 1;
    }

    bool ReadRequiredStringField(
        const int TableIndex,
        const char* FieldName,
        std::string& OutValue,
        FRuntimeFault& OutFault)
    {
        const int AbsoluteIndex = lua_absindex(State, TableIndex);
        lua_getfield(State, AbsoluteIndex, FieldName);
        if (lua_type(State, -1) != LUA_TSTRING)
        {
            lua_pop(State, 1);
            OutFault = {
                "LuaModuleManifestInvalid",
                std::string("Module manifest field must be a string: ") + FieldName};
            return false;
        }
        std::size_t Length = 0;
        const char* Value = lua_tolstring(State, -1, &Length);
        assert(Value != nullptr);
        OutValue.assign(Value, Length);
        lua_pop(State, 1);
        return true;
    }

    bool LoadModuleGraph(
        const std::vector<FRuntimeSource>& Sources,
        std::vector<FModuleSpec>& OutLoadOrder,
        std::map<std::string, const FRuntimeSource*, std::less<>>& OutSourcesByName,
        FRuntimeFault& OutFault)
    {
        OutLoadOrder.clear();
        OutSourcesByName.clear();
        for (const FRuntimeSource& Source : Sources)
        {
            if (Source.Name.empty() || Source.Text.empty()
                || !OutSourcesByName.emplace(Source.Name, &Source).second)
            {
                OutFault = {"LuaRuntimeSourceInvalid", "Runtime source names and text must be non-empty and unique."};
                return false;
            }
        }

        const auto ManifestSource = OutSourcesByName.find(ModuleManifestSourceName);
        if (ManifestSource == OutSourcesByName.end())
        {
            OutFault = {"LuaModuleManifestMissing", "Required module manifest is missing."};
            return false;
        }
        if (!BeginEntry("module_manifest", OutFault))
        {
            return false;
        }

        FStackRestore Stack{State, lua_gettop(State)};
        FExecutionGuard Execution(bExecuting);
        lua_pushcfunction(State, Traceback);
        const int ErrorHandler = lua_gettop(State);
        const FRuntimeSource& Manifest = *ManifestSource->second;
        if (luaL_loadbufferx(
                State,
                Manifest.Text.data(),
                Manifest.Text.size(),
                Manifest.Name.c_str(),
                "t") != LUA_OK)
        {
            ReadLuaError(State, "LuaModuleManifestInvalid", "Module manifest failed to compile.", OutFault);
            return false;
        }
        if (lua_pcall(State, 0, 1, ErrorHandler) != LUA_OK)
        {
            ReadLuaError(State, "LuaModuleManifestInvalid", "Module manifest failed to execute.", OutFault);
            return false;
        }
        if (!lua_istable(State, -1))
        {
            OutFault = {"LuaModuleManifestInvalid", "Module manifest must return a table."};
            return false;
        }
        const int ManifestIndex = lua_absindex(State, -1);

        std::string EntryModuleId;
        if (!ReadRequiredStringField(ManifestIndex, "entry_module_id", EntryModuleId, OutFault)
            || !FStableId::IsOfKind(EntryModuleId, "module"))
        {
            if (OutFault.Code.empty())
            {
                OutFault = {"LuaModuleManifestInvalid", "entry_module_id must be a canonical module ID."};
            }
            return false;
        }

        lua_getfield(State, ManifestIndex, "modules");
        if (!lua_istable(State, -1))
        {
            OutFault = {"LuaModuleManifestInvalid", "modules must be an array."};
            return false;
        }
        const int ModulesIndex = lua_absindex(State, -1);
        const lua_Unsigned ModuleCount = lua_rawlen(State, ModulesIndex);
        if (ModuleCount == 0 || ModuleCount > 4096)
        {
            OutFault = {"LuaModuleManifestInvalid", "Module manifest must contain between 1 and 4096 modules."};
            return false;
        }

        std::vector<FModuleSpec> Specs;
        Specs.reserve(static_cast<std::size_t>(ModuleCount));
        std::map<std::string, std::size_t, std::less<>> SpecIndexById;
        std::set<std::string, std::less<>> DeclaredSourceNames;
        for (lua_Unsigned Index = 1; Index <= ModuleCount; ++Index)
        {
            lua_rawgeti(State, ModulesIndex, static_cast<lua_Integer>(Index));
            if (!lua_istable(State, -1))
            {
                OutFault = {"LuaModuleManifestInvalid", "Each module descriptor must be a table."};
                return false;
            }
            const int SpecTableIndex = lua_absindex(State, -1);
            FModuleSpec Spec;
            std::string RelativeSource;
            if (!ReadRequiredStringField(SpecTableIndex, "module_id", Spec.ModuleId, OutFault)
                || !ReadRequiredStringField(SpecTableIndex, "source", RelativeSource, OutFault)
                || !FStableId::IsOfKind(Spec.ModuleId, "module")
                || !IsCanonicalModuleSourcePath(RelativeSource))
            {
                if (OutFault.Code.empty())
                {
                    OutFault = {"LuaModuleManifestInvalid", "Module ID or source path is not canonical."};
                }
                return false;
            }
            Spec.SourceName = "@Scripts/" + RelativeSource;
            if (Spec.SourceName == ModuleManifestSourceName
                || !SpecIndexById.emplace(Spec.ModuleId, Specs.size()).second
                || !DeclaredSourceNames.emplace(Spec.SourceName).second)
            {
                OutFault = {"LuaModuleManifestInvalid", "Module IDs and source paths must be unique."};
                return false;
            }

            lua_getfield(State, SpecTableIndex, "dependencies");
            if (!lua_istable(State, -1))
            {
                OutFault = {"LuaModuleManifestInvalid", "Module dependencies must be an array."};
                return false;
            }
            const lua_Unsigned DependencyCount = lua_rawlen(State, -1);
            std::set<std::string, std::less<>> SeenDependencies;
            for (lua_Unsigned DependencyIndex = 1; DependencyIndex <= DependencyCount; ++DependencyIndex)
            {
                lua_rawgeti(State, -1, static_cast<lua_Integer>(DependencyIndex));
                if (lua_type(State, -1) != LUA_TSTRING)
                {
                    OutFault = {"LuaModuleManifestInvalid", "Module dependency IDs must be strings."};
                    return false;
                }
                std::size_t Length = 0;
                const char* Value = lua_tolstring(State, -1, &Length);
                std::string Dependency(Value, Length);
                lua_pop(State, 1);
                if (!FStableId::IsOfKind(Dependency, "module")
                    || Dependency == Spec.ModuleId
                    || !SeenDependencies.emplace(Dependency).second)
                {
                    OutFault = {"LuaModuleManifestInvalid", "Module dependencies must be canonical, unique, and non-self."};
                    return false;
                }
                Spec.Dependencies.emplace_back(std::move(Dependency));
            }
            lua_pop(State, 1);
            Specs.emplace_back(std::move(Spec));
            lua_pop(State, 1);
        }
        lua_pop(State, 1);

        if (SpecIndexById.find(EntryModuleId) == SpecIndexById.end())
        {
            OutFault = {"LuaModuleManifestInvalid", "entry_module_id is not declared."};
            return false;
        }
        for (const FModuleSpec& Spec : Specs)
        {
            if (OutSourcesByName.find(Spec.SourceName) == OutSourcesByName.end())
            {
                OutFault = {"LuaModuleSourceMissing", Spec.ModuleId + ": declared module source is missing: " + Spec.SourceName};
                return false;
            }
            for (const std::string& Dependency : Spec.Dependencies)
            {
                if (SpecIndexById.find(Dependency) == SpecIndexById.end())
                {
                    OutFault = {"LuaModuleDependencyMissing", Spec.ModuleId + ": declared module dependency is missing: " + Dependency};
                    return false;
                }
            }
        }
        for (const auto& [SourceName, SourcePtr] : OutSourcesByName)
        {
            if (SourceName != ModuleManifestSourceName && DeclaredSourceNames.find(SourceName) == DeclaredSourceNames.end())
            {
                OutFault = {"LuaModuleSourceUnlisted", "Unlisted Lua source found: " + SourceName};
                return false;
            }
        }

        std::map<std::string, int, std::less<>> VisitState;
        std::function<bool(const std::string&)> Visit = [&](const std::string& ModuleId)
        {
            int& StateValue = VisitState[ModuleId];
            if (StateValue == 1)
            {
                OutFault = {"LuaModuleDependencyCycle", ModuleId + ": module dependency graph contains a cycle"};
                return false;
            }
            if (StateValue == 2)
            {
                return true;
            }
            StateValue = 1;
            const FModuleSpec& Spec = Specs[SpecIndexById.at(ModuleId)];
            for (const std::string& Dependency : Spec.Dependencies)
            {
                if (!Visit(Dependency))
                {
                    return false;
                }
            }
            StateValue = 2;
            OutLoadOrder.emplace_back(Spec);
            return true;
        };
        if (!Visit(EntryModuleId))
        {
            return false;
        }
        if (OutLoadOrder.size() != Specs.size())
        {
            OutFault = {"LuaModuleManifestInvalid", "Every declared module must be reachable from entry_module_id."};
            return false;
        }
        return true;
    }

    bool ExecuteModule(
        const FModuleSpec& Spec,
        const FRuntimeSource& Source,
        FRuntimeFault& OutFault)
    {
        if (!BeginEntry(Spec.ModuleId.c_str(), OutFault))
        {
            return false;
        }

        FStackRestore Stack{State, lua_gettop(State)};
        FExecutionGuard Execution(bExecuting);
        PushString(State, Spec.ModuleId);
        lua_setfield(State, LUA_REGISTRYINDEX, CurrentModuleRegistryKey);
        lua_createtable(State, 0, static_cast<int>(Spec.Dependencies.size()));
        for (const std::string& Dependency : Spec.Dependencies)
        {
            lua_pushboolean(State, 1);
            lua_setfield(State, -2, Dependency.c_str());
        }
        lua_setfield(State, LUA_REGISTRYINDEX, AllowedDependenciesRegistryKey);

        lua_pushcfunction(State, Traceback);
        const int ErrorHandler = lua_gettop(State);
        if (luaL_loadbufferx(
                State,
                Source.Text.data(),
                Source.Text.size(),
                Source.Name.c_str(),
                "t") != LUA_OK)
        {
            ReadLuaError(State, "LuaModuleSyntaxError", "Lua module failed to compile.", OutFault);
            OutFault.Message = Spec.ModuleId + ": " + OutFault.Message;
            return false;
        }
        if (lua_pcall(State, 0, 1, ErrorHandler) != LUA_OK)
        {
            ReadLuaError(State, "LuaModuleLoadError", "Lua module failed to initialize.", OutFault);
            OutFault.Message = Spec.ModuleId + ": " + OutFault.Message;
            return false;
        }
        if (!lua_istable(State, -1))
        {
            OutFault = {"LuaModuleExportInvalid", "Lua module must return an export table: " + Spec.ModuleId};
            return false;
        }

        lua_getfield(State, LUA_REGISTRYINDEX, LoadedModulesRegistryKey);
        lua_pushvalue(State, -2);
        lua_setfield(State, -2, Spec.ModuleId.c_str());
        lua_pop(State, 1);
        lua_pushnil(State);
        lua_setfield(State, LUA_REGISTRYINDEX, CurrentModuleRegistryKey);
        lua_pushnil(State);
        lua_setfield(State, LUA_REGISTRYINDEX, AllowedDependenciesRegistryKey);
        return true;
    }

    bool RunLifecyclePhase(
        const char* PhaseName,
        const std::vector<FModuleSpec>& LoadOrder,
        FRuntimeFault& OutFault)
    {
        for (const FModuleSpec& Spec : LoadOrder)
        {
            if (!BeginEntry(Spec.ModuleId.c_str(), OutFault))
            {
                return false;
            }

            FStackRestore Stack{State, lua_gettop(State)};
            FExecutionGuard Execution(bExecuting);

            lua_pushcfunction(State, Traceback);
            const int ErrorHandler = lua_gettop(State);

            lua_getfield(State, LUA_REGISTRYINDEX, LoadedModulesRegistryKey);
            lua_getfield(State, -1, Spec.ModuleId.c_str());
            lua_remove(State, -2);

            if (!lua_istable(State, -1))
            {
                OutFault = {"LuaModuleLifecycleError", "Module export table is missing: " + Spec.ModuleId};
                return false;
            }

            lua_getfield(State, -1, PhaseName);
            if (lua_isnil(State, -1))
            {
                continue;
            }
            if (!lua_isfunction(State, -1))
            {
                OutFault = {
                    "LuaModuleLifecycleInvalid",
                    std::string("Module hook '") + PhaseName + "' must be a function: " + Spec.ModuleId};
                return false;
            }

            lua_createtable(State, 0, 1);
            lua_pushinteger(State, SessionGeneration);
            lua_setfield(State, -2, "session_generation");

            int NumArgs = 1;
            if (std::strcmp(PhaseName, "validate_state") == 0)
            {
                lua_pushnil(State);
                NumArgs = 2;
            }

            if (lua_pcall(State, NumArgs, 0, ErrorHandler) != LUA_OK)
            {
                ReadLuaError(State, "LuaModuleLifecycleError", "Module lifecycle hook failed.", OutFault);
                return false;
            }
        }
        return true;
    }

    bool IsCanonicalStateSection(const char* SectionKey)
    {
        FStackRestore Stack{State, lua_gettop(State)};
        lua_getfield(State, LUA_REGISTRYINDEX, LoadedModulesRegistryKey);
        if (lua_istable(State, -1))
        {
            lua_getfield(State, -1, "core:module.runtime.state_validator");
            if (lua_istable(State, -1))
            {
                lua_getfield(State, -1, "is_canonical_section");
                if (lua_isfunction(State, -1))
                {
                    lua_pushstring(State, SectionKey);
                    if (lua_pcall(State, 1, 1, 0) == LUA_OK)
                    {
                        const bool bResult = lua_toboolean(State, -1) != 0;
                        return bResult;
                    }
                }
            }
        }
        return false;
    }

    bool MergeStateContribution(
        int TargetTreeIndex,
        int ContributionIndex,
        const std::string& ModuleId,
        FRuntimeFault& OutFault)
    {
        std::string ModNamespace = ModuleId;
        const auto ColonPos = ModuleId.find(':');
        if (ColonPos != std::string::npos)
        {
            ModNamespace = ModuleId.substr(0, ColonPos);
        }

        const int AbsTarget = lua_absindex(State, TargetTreeIndex);
        const int AbsContrib = lua_absindex(State, ContributionIndex);

        lua_pushnil(State);
        while (lua_next(State, AbsContrib) != 0)
        {
            // key is at -2, value is at -1
            if (lua_type(State, -2) != LUA_TSTRING)
            {
                OutFault = {
                    "LuaModuleDefaultStateInvalid",
                    "State contribution section keys must be strings: " + ModuleId};
                lua_pop(State, 2);
                return false;
            }

            std::size_t KeyLen = 0;
            const char* KeyChars = lua_tolstring(State, -2, &KeyLen);
            std::string_view SectionKey(KeyChars, KeyLen);

            if (!IsCanonicalStateSection(KeyChars))
            {
                OutFault = {
                    "LuaModuleDefaultStateInvalid",
                    "Unknown canonical state section '" + std::string(SectionKey) + "' in module contribution: " + ModuleId};
                lua_pop(State, 2);
                return false;
            }

            if (lua_type(State, -1) != LUA_TTABLE)
            {
                OutFault = {
                    "LuaModuleDefaultStateInvalid",
                    "State contribution section '" + std::string(SectionKey) + "' must be a table: " + ModuleId};
                lua_pop(State, 2);
                return false;
            }

            if (lua_getmetatable(State, -1) != 0)
            {
                lua_pop(State, 1); // pop metatable
                OutFault = {
                    "LuaStateValidationInvalid",
                    "State contribution section '" + std::string(SectionKey) + "' cannot have a metatable: " + ModuleId};
                lua_pop(State, 2);
                return false;
            }

            // Get target section table
            lua_getfield(State, AbsTarget, KeyChars);
            assert(lua_istable(State, -1));
            const int TargetSection = lua_absindex(State, -1);

            // Copy all fields from source section into target section with collision and isolation checks
            const int SourceSection = lua_absindex(State, -2);
            lua_pushnil(State);
            while (lua_next(State, SourceSection) != 0)
            {
                // key at -2, value at -1

                // Check section isolation for 'mods': module can only write to its own mod_id / namespace
                if (SectionKey == "mods")
                {
                    if (lua_type(State, -2) != LUA_TSTRING)
                    {
                        OutFault = {
                            "LuaModuleDefaultStateInvalid",
                            "Mod state keys must be string mod IDs: " + ModuleId};
                        lua_pop(State, 4);
                        return false;
                    }
                    std::size_t ModKeyLen = 0;
                    const char* ModKeyChars = lua_tolstring(State, -2, &ModKeyLen);
                    std::string_view ModKey(ModKeyChars, ModKeyLen);

                    if (ModKey != ModNamespace && ModKey != ModuleId)
                    {
                        OutFault = {
                            "LuaModuleDefaultStateInvalid",
                            "Module '" + ModuleId + "' attempted to contribute to forbidden mod section '" + std::string(ModKey) + "'"};
                        lua_pop(State, 4);
                        return false;
                    }
                }

                // If section is "meta" and key is a nested container table (instance_counters, prng, time)
                if (SectionKey == "meta" && lua_type(State, -2) == LUA_TSTRING && lua_type(State, -1) == LUA_TTABLE)
                {
                    std::size_t MetaKeyLen = 0;
                    const char* MetaKeyChars = lua_tolstring(State, -2, &MetaKeyLen);
                    std::string_view MetaSubKey(MetaKeyChars, MetaKeyLen);
                    if (MetaSubKey == "instance_counters" || MetaSubKey == "prng" || MetaSubKey == "time")
                    {
                        lua_pushvalue(State, -2);
                        lua_gettable(State, TargetSection);
                        if (lua_istable(State, -1))
                        {
                            const int TargetSubTable = lua_absindex(State, -1);
                            const int SourceSubTable = lua_absindex(State, -2);
                            lua_pushnil(State);
                            while (lua_next(State, SourceSubTable) != 0)
                            {
                                // key at -2, value at -1
                                lua_pushvalue(State, -2);
                                lua_pushvalue(State, -2);
                                lua_settable(State, TargetSubTable);
                                lua_pop(State, 1);
                            }
                            lua_pop(State, 1); // pop target sub table
                            lua_pop(State, 1); // pop source value
                            continue;
                        }
                        lua_pop(State, 1); // pop non-table
                    }
                }

                // Check collision / override of existing key (except for default meta primitives)
                bool bSkipCollision = false;
                if (SectionKey == "meta" && lua_type(State, -2) == LUA_TSTRING)
                {
                    std::size_t MetaKeyLen = 0;
                    const char* MetaKeyChars = lua_tolstring(State, -2, &MetaKeyLen);
                    std::string_view MetaSubKey(MetaKeyChars, MetaKeyLen);
                    if (MetaSubKey == "schema_version" || MetaSubKey == "save_version" || MetaSubKey == "save_id")
                    {
                        bSkipCollision = true;
                    }
                }

                lua_pushvalue(State, -2); // push key to check in TargetSection
                lua_gettable(State, TargetSection);
                const bool bKeyAlreadyExists = !lua_isnil(State, -1);
                lua_pop(State, 1); // pop check result

                if (bKeyAlreadyExists && !bSkipCollision)
                {
                    std::string CollidingKey;
                    if (lua_type(State, -2) == LUA_TSTRING)
                    {
                        CollidingKey = lua_tostring(State, -2);
                    }
                    else if (lua_type(State, -2) == LUA_TNUMBER)
                    {
                        CollidingKey = std::to_string(lua_tointeger(State, -2));
                    }
                    else
                    {
                        CollidingKey = "unknown";
                    }

                    OutFault = {
                        "LuaModuleDefaultStateInvalid",
                        "Duplicate state contribution key '" + CollidingKey + "' in section '" + std::string(SectionKey) + "' from module: " + ModuleId};
                    lua_pop(State, 4);
                    return false;
                }

                lua_pushvalue(State, -2); // duplicate key for settable
                lua_pushvalue(State, -2); // duplicate value for settable
                lua_settable(State, TargetSection);
                lua_pop(State, 1); // pop value, keep key for next
            }

            lua_pop(State, 1); // pop target section table
            lua_pop(State, 1); // pop value, keep key for next
        }
        return true;
    }

    bool ValidateCanonicalStateTree(int TreeIndex, FRuntimeFault& OutFault)
    {
        if (!BeginEntry("state_validator", OutFault))
        {
            return false;
        }

        FStackRestore Stack{State, lua_gettop(State)};
        FExecutionGuard Execution(bExecuting);

        lua_pushcfunction(State, Traceback);
        const int ErrorHandler = lua_gettop(State);

        lua_getfield(State, LUA_REGISTRYINDEX, LoadedModulesRegistryKey);
        if (!lua_istable(State, -1))
        {
            return true;
        }

        lua_getfield(State, -1, "core:module.runtime.state_validator");
        if (!lua_istable(State, -1))
        {
            return true;
        }

        lua_getfield(State, -1, "validate_state_tree");
        if (!lua_isfunction(State, -1))
        {
            OutFault = {"StateValidatorMissing", "Function 'validate_state_tree' is missing in 'core:module.runtime.state_validator'."};
            return false;
        }

        lua_pushvalue(State, TreeIndex);

        if (lua_pcall(State, 1, 0, ErrorHandler) != LUA_OK)
        {
            ReadLuaError(State, "LuaStateValidationInvalid", "Canonical state tree validation failed.", OutFault);
            return false;
        }

        return true;
    }

    bool CreateDefaultCanonicalStateTree(int& OutTreeRef)
    {
        FStackRestore Stack{State, lua_gettop(State)};
        lua_getfield(State, LUA_REGISTRYINDEX, LoadedModulesRegistryKey);
        if (lua_istable(State, -1))
        {
            lua_getfield(State, -1, "core:module.runtime.state_validator");
            if (lua_istable(State, -1))
            {
                lua_getfield(State, -1, "create_empty_canonical_state");
                if (lua_isfunction(State, -1))
                {
                    if (lua_pcall(State, 0, 1, 0) == LUA_OK && lua_istable(State, -1))
                    {
                        OutTreeRef = luaL_ref(State, LUA_REGISTRYINDEX);
                        return true;
                    }
                }
            }
        }

        lua_createtable(State, 0, 0);
        OutTreeRef = luaL_ref(State, LUA_REGISTRYINDEX);
        return true;
    }

    // SAV-12/13/14/15/16: cold-start load counterpart to
    // CreateDefaultCanonicalStateTree above. Calls
    // core:module.runtime.load.decode_and_prepare(container_bytes), which
    // does preflight, payload decode, and reference-rewrite entirely in
    // Lua and returns either a ready-to-use tree or (nil, typed_error) —
    // C++ only ever sees the final table or a string error code, never a
    // partially-built tree (ADR-0021: canonical state never crosses the
    // boundary piecemeal).
    bool DecodeAndPrepareCanonicalStateTree(
        const std::string& ContainerBytes,
        int& OutTreeRef,
        FRuntimeFault& OutFault)
    {
        FStackRestore Stack{State, lua_gettop(State)};
        FExecutionGuard Execution(bExecuting);

        lua_pushcfunction(State, Traceback);
        const int ErrorHandler = lua_gettop(State);

        lua_getfield(State, LUA_REGISTRYINDEX, LoadedModulesRegistryKey);
        if (!lua_istable(State, -1))
        {
            OutFault = {"SaveLoadModuleMissing", "core:module.runtime.load is not loaded."};
            return false;
        }
        lua_getfield(State, -1, "core:module.runtime.load");
        lua_remove(State, -2);
        if (!lua_istable(State, -1))
        {
            OutFault = {"SaveLoadModuleMissing", "core:module.runtime.load is not loaded."};
            return false;
        }
        lua_getfield(State, -1, "decode_and_prepare");
        if (!lua_isfunction(State, -1))
        {
            OutFault = {"SaveLoadModuleMissing", "core:module.runtime.load.decode_and_prepare is missing."};
            return false;
        }

        PushString(State, ContainerBytes);
        if (lua_pcall(State, 1, 2, ErrorHandler) != LUA_OK)
        {
            ReadLuaError(State, "SaveLoadError", "core:module.runtime.load.decode_and_prepare failed.", OutFault);
            return false;
        }

        // Stack: [..., tree_or_nil, err_or_nil]
        if (lua_istable(State, -2))
        {
            lua_pop(State, 1); // pop err (nil)
            OutTreeRef = luaL_ref(State, LUA_REGISTRYINDEX); // pops tree
            return true;
        }

        std::string ErrCode = "SaveLoadFailed";
        if (lua_type(State, -1) == LUA_TSTRING)
        {
            ErrCode = lua_tostring(State, -1);
        }
        OutFault = {ErrCode, "Cold start load failed: " + ErrCode};
        lua_pop(State, 2);
        return false;
    }

    // SAV-20: calls core:module.runtime.migrate.verify_complete() after
    // every module's "migrate_state" hook has run, to reject — explicitly,
    // not silently — any pending migration no module claimed.
    bool VerifyMigrationsComplete(FRuntimeFault& OutFault)
    {
        FStackRestore Stack{State, lua_gettop(State)};
        FExecutionGuard Execution(bExecuting);

        lua_pushcfunction(State, Traceback);
        const int ErrorHandler = lua_gettop(State);

        lua_getfield(State, LUA_REGISTRYINDEX, LoadedModulesRegistryKey);
        if (!lua_istable(State, -1))
        {
            OutFault = {"SaveLoadModuleMissing", "core:module.runtime.migrate is not loaded."};
            return false;
        }
        lua_getfield(State, -1, "core:module.runtime.migrate");
        lua_remove(State, -2);
        if (!lua_istable(State, -1))
        {
            OutFault = {"SaveLoadModuleMissing", "core:module.runtime.migrate is not loaded."};
            return false;
        }
        lua_getfield(State, -1, "verify_complete");
        if (!lua_isfunction(State, -1))
        {
            OutFault = {"SaveLoadModuleMissing", "core:module.runtime.migrate.verify_complete is missing."};
            return false;
        }

        if (lua_pcall(State, 0, 1, ErrorHandler) != LUA_OK)
        {
            ReadLuaError(State, "MigrationError", "core:module.runtime.migrate.verify_complete failed.", OutFault);
            return false;
        }

        if (lua_isnil(State, -1))
        {
            lua_pop(State, 1);
            return true;
        }
        std::string ErrCode = "MigrationMissing";
        if (lua_type(State, -1) == LUA_TSTRING)
        {
            ErrCode = lua_tostring(State, -1);
        }
        OutFault = {ErrCode, "Cold start load migration incomplete: " + ErrCode};
        lua_pop(State, 1);
        return false;
    }

    // Calls .freeze() on the table found by walking `game` then each field
    // in Path (e.g. {"commands", "validators"} -> game.commands.validators),
    // if every step and the final freeze() method exist. Used at the end of
    // the "register" lifecycle phase for every registry the game facade
    // exposes (game.services, game.commands.validators, ...), so late
    // registration is uniformly rejected.
    void FreezeGameRegistry(std::initializer_list<const char*> Path)
    {
        const int Base = lua_gettop(State);
        lua_getglobal(State, "game");
        bool bFound = lua_istable(State, -1) != 0;
        for (const char* Field : Path)
        {
            if (!bFound)
            {
                break;
            }
            lua_getfield(State, -1, Field);
            bFound = lua_istable(State, -1) != 0;
        }
        if (bFound)
        {
            lua_getfield(State, -1, "freeze");
            if (lua_isfunction(State, -1))
            {
                lua_pcall(State, 0, 0, 0); // Ignore result; missing/erroring freeze leaves the registry unfrozen.
            }
        }
        lua_settop(State, Base);
    }

    bool RunLifecycleHooks(
        const std::vector<FModuleSpec>& LoadOrder,
        const std::string* LoadContainerBytes,
        FRuntimeFault& OutFault)
    {
        // 1. Phase "register"
        if (!RunLifecyclePhase("register", LoadOrder, OutFault))
        {
            return false;
        }

        // Freeze registries at the end of register phase (GEW-01: validators,
        // GEW-10: event subscribers freeze together with the other registries).
        FreezeGameRegistry({"services"});
        FreezeGameRegistry({"commands", "validators"});
        FreezeGameRegistry({"events", "subscribers"});
        FreezeGameRegistry({"events"});

        // 2. Obtain the canonical state tree. SAV-12/13/14/15/16: on a
        // cold-start load, the tree comes whole from the save container —
        // preflight, payload decode, and reference-rewrite all happen
        // inside one Lua call (core:module.runtime.load.decode_and_prepare)
        // so C++ never sees canonical state (ADR-0021) — and module
        // "create_default_state" contributions are skipped entirely: a
        // loaded tree is already complete, running defaults over it would
        // duplicate or clobber loaded data.
        int TreeRef = LUA_NOREF;
        if (LoadContainerBytes != nullptr)
        {
            if (!DecodeAndPrepareCanonicalStateTree(*LoadContainerBytes, TreeRef, OutFault))
            {
                return false;
            }
        }
        else
        {
        if (!CreateDefaultCanonicalStateTree(TreeRef))
        {
            OutFault = {"LuaStateInitializationFailed", "Failed to initialize canonical state tree."};
            return false;
        }

        // 3. Phase "create_default_state"
        for (const FModuleSpec& Spec : LoadOrder)
        {
            if (!BeginEntry(Spec.ModuleId.c_str(), OutFault))
            {
                luaL_unref(State, LUA_REGISTRYINDEX, TreeRef);
                return false;
            }

            FStackRestore Stack{State, lua_gettop(State)};
            FExecutionGuard Execution(bExecuting);

            lua_pushcfunction(State, Traceback);
            const int ErrorHandler = lua_gettop(State);

            lua_getfield(State, LUA_REGISTRYINDEX, LoadedModulesRegistryKey);
            lua_getfield(State, -1, Spec.ModuleId.c_str());
            lua_remove(State, -2);

            if (!lua_istable(State, -1))
            {
                luaL_unref(State, LUA_REGISTRYINDEX, TreeRef);
                OutFault = {"LuaModuleLifecycleError", "Module export table is missing: " + Spec.ModuleId};
                return false;
            }

            lua_getfield(State, -1, "create_default_state");
            if (lua_isnil(State, -1))
            {
                continue;
            }
            if (!lua_isfunction(State, -1))
            {
                luaL_unref(State, LUA_REGISTRYINDEX, TreeRef);
                OutFault = {
                    "LuaModuleLifecycleInvalid",
                    "Module hook 'create_default_state' must be a function: " + Spec.ModuleId};
                return false;
            }

            lua_createtable(State, 0, 1);
            lua_pushinteger(State, SessionGeneration);
            lua_setfield(State, -2, "session_generation");

            if (lua_pcall(State, 1, 1, ErrorHandler) != LUA_OK)
            {
                luaL_unref(State, LUA_REGISTRYINDEX, TreeRef);
                ReadLuaError(State, "LuaModuleLifecycleError", "Module create_default_state failed.", OutFault);
                return false;
            }

            if (!lua_isnil(State, -1))
            {
                if (!lua_istable(State, -1))
                {
                    luaL_unref(State, LUA_REGISTRYINDEX, TreeRef);
                    OutFault = {
                        "LuaModuleDefaultStateInvalid",
                        "Module create_default_state must return a table or nil: " + Spec.ModuleId};
                    return false;
                }

                lua_rawgeti(State, LUA_REGISTRYINDEX, TreeRef);
                const int TreeIndex = lua_gettop(State);
                const int ContribIndex = TreeIndex - 1;
                if (!MergeStateContribution(TreeIndex, ContribIndex, Spec.ModuleId, OutFault))
                {
                    luaL_unref(State, LUA_REGISTRYINDEX, TreeRef);
                    return false;
                }
                lua_pop(State, 1); // pop Tree
            }
        }
        } // end LoadContainerBytes == nullptr (NewGame default-state branch)

        // SAV-18/19/20: phase "migrate_state" — only on a cold-start load,
        // between decode (above) and restore_instances (below), so a
        // migration can repair shape before restore_instances/validate_state
        // see the tree. Same (ctx, tree) calling convention as
        // "restore_instances"/"validate_state". core:module.runtime.load
        // already rejected a downgrade (a saved section newer than this
        // build) before returning a tree at all; what remains here is
        // giving every module a chance to claim a pending migration, then
        // verifying none was silently left unclaimed.
        if (LoadContainerBytes != nullptr)
        {
            for (const FModuleSpec& Spec : LoadOrder)
            {
                if (!BeginEntry(Spec.ModuleId.c_str(), OutFault))
                {
                    luaL_unref(State, LUA_REGISTRYINDEX, TreeRef);
                    return false;
                }

                FStackRestore Stack{State, lua_gettop(State)};
                FExecutionGuard Execution(bExecuting);

                lua_pushcfunction(State, Traceback);
                const int ErrorHandler = lua_gettop(State);

                lua_getfield(State, LUA_REGISTRYINDEX, LoadedModulesRegistryKey);
                lua_getfield(State, -1, Spec.ModuleId.c_str());
                lua_remove(State, -2);

                if (!lua_istable(State, -1))
                {
                    luaL_unref(State, LUA_REGISTRYINDEX, TreeRef);
                    OutFault = {"LuaModuleLifecycleError", "Module export table is missing: " + Spec.ModuleId};
                    return false;
                }

                lua_getfield(State, -1, "migrate_state");
                if (lua_isnil(State, -1))
                {
                    continue;
                }
                if (!lua_isfunction(State, -1))
                {
                    luaL_unref(State, LUA_REGISTRYINDEX, TreeRef);
                    OutFault = {
                        "LuaModuleLifecycleInvalid",
                        "Module hook 'migrate_state' must be a function: " + Spec.ModuleId};
                    return false;
                }

                lua_createtable(State, 0, 1);
                lua_pushinteger(State, SessionGeneration);
                lua_setfield(State, -2, "session_generation");

                lua_rawgeti(State, LUA_REGISTRYINDEX, TreeRef);

                if (lua_pcall(State, 2, 0, ErrorHandler) != LUA_OK)
                {
                    luaL_unref(State, LUA_REGISTRYINDEX, TreeRef);
                    ReadLuaError(State, "LuaModuleLifecycleError", "Module migrate_state failed.", OutFault);
                    return false;
                }
            }

            if (!VerifyMigrationsComplete(OutFault))
            {
                luaL_unref(State, LUA_REGISTRYINDEX, TreeRef);
                return false;
            }
        }

        // 4. Validate canonical state tree structure & value types (runs
        // for both NewGame and cold-start load — a loaded tree gets the
        // exact same structural scrutiny a freshly-defaulted one does).
        lua_rawgeti(State, LUA_REGISTRYINDEX, TreeRef);
        const int RootTreeIndex = lua_gettop(State);
        if (!ValidateCanonicalStateTree(RootTreeIndex, OutFault))
        {
            lua_pop(State, 1);
            luaL_unref(State, LUA_REGISTRYINDEX, TreeRef);
            return false;
        }
        lua_pop(State, 1);

        // SAV-17: phase "restore_instances" — only on a cold-start load (a
        // freshly-defaulted state has nothing to restore). Same (ctx, tree)
        // calling convention as "validate_state" below. Absence of the
        // hook in a module is not an error (BootstrapAndSessionLifecycle.md).
        if (LoadContainerBytes != nullptr)
        {
            for (const FModuleSpec& Spec : LoadOrder)
            {
                if (!BeginEntry(Spec.ModuleId.c_str(), OutFault))
                {
                    luaL_unref(State, LUA_REGISTRYINDEX, TreeRef);
                    return false;
                }

                FStackRestore Stack{State, lua_gettop(State)};
                FExecutionGuard Execution(bExecuting);

                lua_pushcfunction(State, Traceback);
                const int ErrorHandler = lua_gettop(State);

                lua_getfield(State, LUA_REGISTRYINDEX, LoadedModulesRegistryKey);
                lua_getfield(State, -1, Spec.ModuleId.c_str());
                lua_remove(State, -2);

                if (!lua_istable(State, -1))
                {
                    luaL_unref(State, LUA_REGISTRYINDEX, TreeRef);
                    OutFault = {"LuaModuleLifecycleError", "Module export table is missing: " + Spec.ModuleId};
                    return false;
                }

                lua_getfield(State, -1, "restore_instances");
                if (lua_isnil(State, -1))
                {
                    continue;
                }
                if (!lua_isfunction(State, -1))
                {
                    luaL_unref(State, LUA_REGISTRYINDEX, TreeRef);
                    OutFault = {
                        "LuaModuleLifecycleInvalid",
                        "Module hook 'restore_instances' must be a function: " + Spec.ModuleId};
                    return false;
                }

                lua_createtable(State, 0, 1);
                lua_pushinteger(State, SessionGeneration);
                lua_setfield(State, -2, "session_generation");

                lua_rawgeti(State, LUA_REGISTRYINDEX, TreeRef);

                if (lua_pcall(State, 2, 0, ErrorHandler) != LUA_OK)
                {
                    luaL_unref(State, LUA_REGISTRYINDEX, TreeRef);
                    ReadLuaError(State, "LuaModuleLifecycleError", "Module restore_instances failed.", OutFault);
                    return false;
                }
            }
        }

        // 5. Phase "validate_state"
        for (const FModuleSpec& Spec : LoadOrder)
        {
            if (!BeginEntry(Spec.ModuleId.c_str(), OutFault))
            {
                luaL_unref(State, LUA_REGISTRYINDEX, TreeRef);
                return false;
            }

            FStackRestore Stack{State, lua_gettop(State)};
            FExecutionGuard Execution(bExecuting);

            lua_pushcfunction(State, Traceback);
            const int ErrorHandler = lua_gettop(State);

            lua_getfield(State, LUA_REGISTRYINDEX, LoadedModulesRegistryKey);
            lua_getfield(State, -1, Spec.ModuleId.c_str());
            lua_remove(State, -2);

            if (!lua_istable(State, -1))
            {
                luaL_unref(State, LUA_REGISTRYINDEX, TreeRef);
                OutFault = {"LuaModuleLifecycleError", "Module export table is missing: " + Spec.ModuleId};
                return false;
            }

            lua_getfield(State, -1, "validate_state");
            if (lua_isnil(State, -1))
            {
                continue;
            }
            if (!lua_isfunction(State, -1))
            {
                luaL_unref(State, LUA_REGISTRYINDEX, TreeRef);
                OutFault = {
                    "LuaModuleLifecycleInvalid",
                    "Module hook 'validate_state' must be a function: " + Spec.ModuleId};
                return false;
            }

            lua_createtable(State, 0, 1);
            lua_pushinteger(State, SessionGeneration);
            lua_setfield(State, -2, "session_generation");

            lua_rawgeti(State, LUA_REGISTRYINDEX, TreeRef);

            if (lua_pcall(State, 2, 0, ErrorHandler) != LUA_OK)
            {
                luaL_unref(State, LUA_REGISTRYINDEX, TreeRef);
                ReadLuaError(State, "LuaModuleLifecycleError", "Module validate_state failed.", OutFault);
                return false;
            }
        }

        // 5. Assign canonical state to game.state
        lua_getglobal(State, "game");
        lua_rawgeti(State, LUA_REGISTRYINDEX, TreeRef);

        lua_getfield(State, LUA_REGISTRYINDEX, LoadedModulesRegistryKey);
        if (lua_istable(State, -1))
        {
            lua_getfield(State, -1, "core:module.runtime.mutation_window");
            if (lua_istable(State, -1))
            {
                lua_getfield(State, -1, "guard_state");
                if (lua_isfunction(State, -1))
                {
                    lua_pushvalue(State, -4);
                    if (lua_pcall(State, 1, 1, 0) == LUA_OK && lua_istable(State, -1))
                    {
                        lua_replace(State, -4);
                    }
                    else
                    {
                        lua_pop(State, 1);
                    }
                }
                else
                {
                    lua_pop(State, 1);
                }
            }
            lua_pop(State, 1);
        }
        lua_pop(State, 1);

        lua_setfield(State, -2, "state");
        lua_pop(State, 1); // pop game
        luaL_unref(State, LUA_REGISTRYINDEX, TreeRef);

        // 6. Phase "start"
        if (!RunLifecyclePhase("start", LoadOrder, OutFault))
        {
            return false;
        }

        return true;
    }

    bool LoadModules(
        const std::vector<FRuntimeSource>& Sources,
        const std::string* LoadContainerBytes,
        FRuntimeFault& OutFault)
    {
        std::vector<FModuleSpec> LoadOrder;
        std::map<std::string, const FRuntimeSource*, std::less<>> SourcesByName;
        if (!LoadModuleGraph(Sources, LoadOrder, SourcesByName, OutFault))
        {
            return false;
        }
        for (const FModuleSpec& Spec : LoadOrder)
        {
            if (!ExecuteModule(Spec, *SourcesByName.at(Spec.SourceName), OutFault))
            {
                return false;
            }
        }
        if (!RunLifecycleHooks(LoadOrder, LoadContainerBytes, OutFault))
        {
            return false;
        }
        return true;
    }

    bool CheckScripts(
        const std::vector<FRuntimeSource>& Sources,
        std::size_t* OutModuleCount,
        FRuntimeFault& OutFault)
    {
        std::vector<FModuleSpec> LoadOrder;
        std::map<std::string, const FRuntimeSource*, std::less<>> SourcesByName;
        if (!LoadModuleGraph(Sources, LoadOrder, SourcesByName, OutFault))
        {
            return false;
        }
        for (const FModuleSpec& Spec : LoadOrder)
        {
            if (!ExecuteModule(Spec, *SourcesByName.at(Spec.SourceName), OutFault))
            {
                return false;
            }
        }
        if (OutModuleCount != nullptr)
        {
            *OutModuleCount = LoadOrder.size();
        }
        return true;
    }

    bool RunLuaSpec(
        const std::string& SpecChunkName,
        const std::string& SpecSource,
        std::vector<FLuaSpecCaseResult>& OutCaseResults,
        FRuntimeFault& OutFault)
    {
        OutCaseResults.clear();
        if (!BeginEntry("run_lua_spec", OutFault))
        {
            return false;
        }

        FStackRestore Stack{State, lua_gettop(State)};
        FExecutionGuard Execution(bExecuting);

        // Grant require() access to every module the production bootstrap
        // has already loaded. Specs are read-only test code, never part of
        // the shipped module tree, so they do not declare their own
        // dependency list the way Scripts/ modules do.
        lua_getfield(State, LUA_REGISTRYINDEX, LoadedModulesRegistryKey);
        const int LoadedModulesIndex = lua_gettop(State);
        lua_createtable(State, 0, 0);
        const int AllowedIndex = lua_gettop(State);
        lua_pushnil(State);
        while (lua_next(State, LoadedModulesIndex) != 0)
        {
            lua_pop(State, 1);
            lua_pushvalue(State, -1);
            lua_pushboolean(State, 1);
            lua_settable(State, AllowedIndex);
        }
        lua_setfield(State, LUA_REGISTRYINDEX, AllowedDependenciesRegistryKey);
        lua_pop(State, 1); // pop LoadedModules

        PushString(State, std::string("@spec"));
        lua_setfield(State, LUA_REGISTRYINDEX, CurrentModuleRegistryKey);
        FRequireContextGuard RequireContext{State};

        lua_pushcfunction(State, Traceback);
        const int ErrorHandler = lua_gettop(State);

        if (luaL_loadbufferx(
                State,
                SpecSource.data(),
                SpecSource.size(),
                SpecChunkName.c_str(),
                "t") != LUA_OK)
        {
            ReadLuaError(State, "LuaSpecSyntaxError", "Lua spec failed to compile.", OutFault);
            return false;
        }
        if (lua_pcall(State, 0, 1, ErrorHandler) != LUA_OK)
        {
            ReadLuaError(State, "LuaSpecLoadError", "Lua spec failed to execute.", OutFault);
            return false;
        }
        if (!lua_istable(State, -1))
        {
            OutFault = {"LuaSpecFormatInvalid", "Lua spec must return a table of named cases."};
            return false;
        }
        const int CasesTableIndex = lua_gettop(State);

        std::vector<std::string> CaseNames;
        lua_pushnil(State);
        while (lua_next(State, CasesTableIndex) != 0)
        {
            if (lua_type(State, -2) != LUA_TSTRING)
            {
                OutFault = {"LuaSpecFormatInvalid", "Lua spec case keys must be strings."};
                return false;
            }
            if (!lua_isfunction(State, -1))
            {
                std::size_t KeyLength = 0;
                const char* Key = lua_tolstring(State, -2, &KeyLength);
                OutFault = {
                    "LuaSpecFormatInvalid",
                    "Lua spec case '" + std::string(Key, KeyLength) + "' must be a zero-arg function."};
                return false;
            }
            std::size_t KeyLength = 0;
            const char* Key = lua_tolstring(State, -2, &KeyLength);
            CaseNames.emplace_back(Key, KeyLength);
            lua_pop(State, 1); // pop value, keep key for lua_next
        }

        if (CaseNames.empty())
        {
            OutFault = {"LuaSpecEmpty", "Lua spec must return at least one named case."};
            return false;
        }
        std::sort(CaseNames.begin(), CaseNames.end());

        OutCaseResults.reserve(CaseNames.size());
        for (const std::string& CaseName : CaseNames)
        {
            lua_getfield(State, CasesTableIndex, CaseName.c_str());
            if (lua_pcall(State, 0, 0, ErrorHandler) != LUA_OK)
            {
                const char* Message = lua_tostring(State, -1);
                FLuaSpecCaseResult Result;
                Result.CaseId = CaseName;
                Result.Success = false;
                Result.ErrorMessage = Message != nullptr ? Message : "Lua spec case failed.";
                lua_pop(State, 1);
                OutCaseResults.push_back(std::move(Result));
                continue;
            }
            OutCaseResults.push_back(FLuaSpecCaseResult{CaseName, true, ""});
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

        if (!FGV2LuaMarshaller::PushObject(State, Envelope, OutFault))
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

    bool ReadPortableValue(
        const int ValueIndex,
        FValue& OutValue,
        const int Depth,
        std::size_t& NodeCount,
        std::set<const void*>& ActiveTables,
        FRuntimeFault& OutFault)
    {
        if (Depth > MaxValueDepth || ++NodeCount > MaxValueNodes)
        {
            OutFault = {"LuaScreenRequestInvalid", "Screen Field value exceeds depth or node limits."};
            return false;
        }

        const int AbsoluteIndex = lua_absindex(State, ValueIndex);
        switch (lua_type(State, AbsoluteIndex))
        {
        case LUA_TBOOLEAN:
            OutValue = FValue(lua_toboolean(State, AbsoluteIndex) != 0);
            return true;
        case LUA_TNUMBER:
            if (lua_isinteger(State, AbsoluteIndex))
            {
                OutValue = FValue(static_cast<std::int64_t>(lua_tointeger(State, AbsoluteIndex)));
                return true;
            }
            if (const double Number = static_cast<double>(lua_tonumber(State, AbsoluteIndex)); std::isfinite(Number))
            {
                OutValue = FValue(Number);
                return true;
            }
            OutFault = {"LuaScreenRequestInvalid", "Screen Field contains a non-finite number."};
            return false;
        case LUA_TSTRING:
        {
            std::size_t Length = 0;
            const char* Text = lua_tolstring(State, AbsoluteIndex, &Length);
            OutValue = FValue(std::string(Text, Length));
            return true;
        }
        case LUA_TTABLE:
            break;
        default:
            OutFault = {"LuaScreenRequestInvalid", "Screen Field values support only scalar, array and object values."};
            return false;
        }

        const void* Identity = lua_topointer(State, AbsoluteIndex);
        if (!ActiveTables.emplace(Identity).second)
        {
            OutFault = {"LuaScreenRequestInvalid", "Screen Field value contains a table cycle."};
            return false;
        }
        struct FActiveTableGuard
        {
            std::set<const void*>& Tables;
            const void* Identity;
            ~FActiveTableGuard() { Tables.erase(Identity); }
        } Guard{ActiveTables, Identity};

        const lua_Unsigned ArrayLength = lua_rawlen(State, AbsoluteIndex);
        if (ArrayLength > 0)
        {
            lua_Unsigned EntryCount = 0;
            lua_pushnil(State);
            while (lua_next(State, AbsoluteIndex) != 0)
            {
                ++EntryCount;
                const bool bArrayKey = lua_isinteger(State, -2)
                    && lua_tointeger(State, -2) >= 1
                    && static_cast<lua_Unsigned>(lua_tointeger(State, -2)) <= ArrayLength;
                lua_pop(State, 1);
                if (!bArrayKey)
                {
                    lua_pop(State, 1);
                    OutFault = {"LuaScreenRequestInvalid", "Screen Field array contains a non-array key."};
                    return false;
                }
            }
            if (EntryCount != ArrayLength)
            {
                OutFault = {"LuaScreenRequestInvalid", "Screen Field array must be dense."};
                return false;
            }
            FValue::FArray Array;
            Array.reserve(static_cast<std::size_t>(ArrayLength));
            for (lua_Unsigned Index = 1; Index <= ArrayLength; ++Index)
            {
                lua_rawgeti(State, AbsoluteIndex, static_cast<lua_Integer>(Index));
                FValue& Item = Array.emplace_back();
                const bool bValid = ReadPortableValue(-1, Item, Depth + 1, NodeCount, ActiveTables, OutFault);
                lua_pop(State, 1);
                if (!bValid) return false;
            }
            OutValue = FValue(std::move(Array));
            return true;
        }

        FValue::FObject Object;
        lua_pushnil(State);
        while (lua_next(State, AbsoluteIndex) != 0)
        {
            if (lua_type(State, -2) != LUA_TSTRING)
            {
                lua_pop(State, 2);
                OutFault = {"LuaScreenRequestInvalid", "Screen Field object keys must be strings."};
                return false;
            }
            std::size_t KeyLength = 0;
            const char* KeyData = lua_tolstring(State, -2, &KeyLength);
            std::string Key(KeyData, KeyLength);
            FValue Value;
            if (Key.empty() || Object.find(Key) != Object.end()
                || !ReadPortableValue(-1, Value, Depth + 1, NodeCount, ActiveTables, OutFault))
            {
                lua_pop(State, 2);
                if (OutFault.Code.empty())
                {
                    OutFault = {"LuaScreenRequestInvalid", "Screen Field object contains an invalid key."};
                }
                return false;
            }
            Object.emplace(std::move(Key), std::move(Value));
            lua_pop(State, 1);
        }
        OutValue = FValue(std::move(Object));
        return true;
    }

    bool ReadScreenRequest(const int TableIndex, FScreenRequest& OutRequest, FRuntimeFault& OutFault)
    {
        if (!lua_istable(State, TableIndex))
        {
            OutFault = {"LuaScreenRequestInvalid", "Screen request must be a table."};
            return false;
        }
        const int AbsoluteIndex = lua_absindex(State, TableIndex);
        FScreenRequest Candidate;
        lua_getfield(State, AbsoluteIndex, "screen_id");
        if (lua_type(State, -1) != LUA_TSTRING)
        {
            lua_pop(State, 1);
            OutFault = {"LuaScreenRequestInvalid", "Screen request screen_id must be a string."};
            return false;
        }
        std::size_t ScreenIdLength = 0;
        const char* ScreenId = lua_tolstring(State, -1, &ScreenIdLength);
        Candidate.ScreenId.assign(ScreenId, ScreenIdLength);
        lua_pop(State, 1);
        if (!FStableId::IsOfKind(Candidate.ScreenId, "screen"))
        {
            OutFault = {"LuaScreenRequestInvalid", "Screen request returned an invalid screen_id."};
            return false;
        }

        lua_getfield(State, AbsoluteIndex, "fields");
        if (!lua_istable(State, -1) || lua_rawlen(State, -1) != 0)
        {
            lua_pop(State, 1);
            OutFault = {"LuaScreenRequestInvalid", "Screen request fields must be an object."};
            return false;
        }
        const int FieldsIndex = lua_absindex(State, -1);
        std::set<std::string, std::less<>> SeenFields;
        std::size_t NodeCount = 0;
        std::set<const void*> ActiveTables;
        lua_pushnil(State);
        while (lua_next(State, FieldsIndex) != 0)
        {
            if (lua_type(State, -2) != LUA_TSTRING || !lua_istable(State, -1)
                || Candidate.Fields.size() >= 128)
            {
                lua_pop(State, 3);
                OutFault = {"LuaScreenRequestInvalid", "Screen request contains an invalid field entry."};
                return false;
            }
            std::size_t FieldIdLength = 0;
            const char* FieldId = lua_tolstring(State, -2, &FieldIdLength);
            FScreenField& Field = Candidate.Fields.emplace_back();
            Field.FieldId.assign(FieldId, FieldIdLength);
            if (!FStableId::IsValidSegment(Field.FieldId) || !SeenFields.emplace(Field.FieldId).second)
            {
                lua_pop(State, 3);
                OutFault = {"LuaScreenRequestInvalid", "Screen request field_id is invalid or duplicated."};
                return false;
            }
            const int FieldIndex = lua_absindex(State, -1);
            lua_getfield(State, FieldIndex, "schema_id");
            if (lua_type(State, -1) != LUA_TSTRING)
            {
                lua_pop(State, 4);
                OutFault = {"LuaScreenRequestInvalid", "Screen Field schema_id must be a string."};
                return false;
            }
            std::size_t SchemaLength = 0;
            const char* SchemaId = lua_tolstring(State, -1, &SchemaLength);
            Field.SchemaId.assign(SchemaId, SchemaLength);
            lua_pop(State, 1);
            if (!FStableId::IsOfKind(Field.SchemaId, "schema"))
            {
                lua_pop(State, 3);
                OutFault = {"LuaScreenRequestInvalid", "Screen Field schema_id is invalid."};
                return false;
            }
            lua_getfield(State, FieldIndex, "value");
            const bool bValueValid = ReadPortableValue(
                -1, Field.Value, 0, NodeCount, ActiveTables, OutFault);
            lua_pop(State, 1);
            if (!bValueValid)
            {
                lua_pop(State, 3);
                return false;
            }
            lua_pop(State, 1);
        }
        lua_pop(State, 1);
        std::sort(Candidate.Fields.begin(), Candidate.Fields.end(), [](const FScreenField& Left, const FScreenField& Right)
        {
            return Left.FieldId < Right.FieldId;
        });
        OutRequest = std::move(Candidate);
        return true;
    }

    bool CallTakePendingScreen(
        std::optional<FScreenRequest>& OutRequest,
        FRuntimeFault& OutFault)
    {
        if (!BeginEntry("take_pending_screen", OutFault))
        {
            return false;
        }

        FStackRestore Stack{State, lua_gettop(State)};
        FExecutionGuard Execution(bExecuting);
        lua_pushcfunction(State, Traceback);
        const int ErrorHandler = lua_gettop(State);

        lua_getglobal(State, "game");
        lua_getfield(State, -1, "ui");
        lua_getfield(State, -1, "take_pending_screen");
        lua_remove(State, -2);
        lua_remove(State, -2);
        if (!lua_isfunction(State, -1))
        {
            OutFault = {"LuaEntryPointMissing", "Fixed entry point is missing: take_pending_screen"};
            return false;
        }
        if (lua_pcall(State, 0, 1, ErrorHandler) != LUA_OK)
        {
            ReadLuaError(State, "LuaPresentationTakeError", "Lua failed to return pending presentation.", OutFault);
            return false;
        }
        if (lua_isnil(State, -1))
        {
            OutRequest.reset();
            return true;
        }

        FScreenRequest Candidate;
        if (!ReadScreenRequest(-1, Candidate, OutFault))
        {
            return false;
        }
        OutRequest = std::move(Candidate);
        return true;
    }

    std::string CallGetCanonicalStateHash(FRuntimeFault* OutFault) const
    {
        if (!State)
        {
            return "";
        }
        if (!IsOwnerThread())
        {
            if (OutFault)
            {
                *OutFault = {"WrongThread", "Session access from non-owner thread"};
            }
            return "";
        }
        if (bExecuting)
        {
            if (OutFault)
            {
                *OutFault = {"ReentrantExecution", "Session access while executing"};
            }
            return "";
        }

        FStackRestore Stack{State, lua_gettop(State)};
        FExecutionGuard Execution(const_cast<bool&>(bExecuting));
        lua_pushcfunction(State, Traceback);
        const int ErrorHandler = lua_gettop(State);

        lua_getglobal(State, "game");
        if (!lua_istable(State, -1))
        {
            return "";
        }
        lua_getfield(State, -1, "runtime");
        if (!lua_istable(State, -1))
        {
            return "";
        }
        lua_getfield(State, -1, "get_canonical_state_hash");
        if (!lua_isfunction(State, -1))
        {
            return "";
        }
        lua_remove(State, -2);
        lua_remove(State, -2);

        if (lua_pcall(State, 0, 1, ErrorHandler) != LUA_OK)
        {
            if (OutFault)
            {
                ReadLuaError(State, "LuaStateHashError", "Failed to compute canonical state hash.", *OutFault);
            }
            return "";
        }

        if (lua_isstring(State, -1))
        {
            size_t Len = 0;
            const char* Str = lua_tolstring(State, -1, &Len);
            return std::string(Str, Len);
        }

        return "";
    }
};

FRuntimeSession::FRuntimeSession() : Impl(std::make_unique<FImpl>())
{
}

FRuntimeSession::~FRuntimeSession()
{
    Stop();
}

bool FRuntimeSession::Start(
    const std::int32_t InSessionGeneration,
    const GV2ContentCore::FRepositoryReadHandle& PinnedRepository,
    const std::vector<FRuntimeSource>& Sources,
    FRuntimeFault& OutFault)
{
    OutFault = {};
    if (!Stop(&OutFault))
    {
        return false;
    }
    if (!PinnedRepository.IsValid())
    {
        OutFault = {"RepositoryNotReady", "Runtime session requires a valid pinned repository read handle."};
        return false;
    }
    if (InSessionGeneration <= 0)
    {
        OutFault = {"InvalidSessionGeneration", "Runtime requires a positive session generation."};
        return false;
    }
    if (Sources.empty())
    {
        OutFault = {"LuaRuntimeSourceMissing", "Runtime requires at least one Lua source."};
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
    Impl->PinnedRepository = PinnedRepository;

    if (!Impl->OpenEnvironment(OutFault))
    {
        Stop();
        return false;
    }
    if (!Impl->LoadModules(Sources, nullptr, OutFault))
    {
        Stop();
        return false;
    }
    return true;
}

bool FRuntimeSession::StartFromSave(
    const std::int32_t InSessionGeneration,
    const GV2ContentCore::FRepositoryReadHandle& PinnedRepository,
    const std::vector<FRuntimeSource>& Sources,
    ISaveSlotStorage& Storage,
    const std::string& SaveSlotId,
    FRuntimeFault& OutFault)
{
    OutFault = {};
    if (!Stop(&OutFault))
    {
        return false;
    }

    // SAV-12: the slot is read with no Lua VM in existence yet — a missing
    // or unreadable slot is a configuration failure the caller (composition
    // root) surfaces as a recovery surface before any VM cost is paid.
    const FSaveSlotReadResult ReadResult = Storage.ReadSlot(SaveSlotId);
    if (ReadResult.Result == ESaveSlotResult::NotFound)
    {
        OutFault = {"SaveSlotNotFound", "Cold start load requested slot '" + SaveSlotId + "', which does not exist."};
        return false;
    }
    if (ReadResult.Result != ESaveSlotResult::Ok)
    {
        OutFault = {"SaveSlotUnreadable", "Cold start load requested slot '" + SaveSlotId + "', which is not readable."};
        return false;
    }

    if (!PinnedRepository.IsValid())
    {
        OutFault = {"RepositoryNotReady", "Runtime session requires a valid pinned repository read handle."};
        return false;
    }
    if (InSessionGeneration <= 0)
    {
        OutFault = {"InvalidSessionGeneration", "Runtime requires a positive session generation."};
        return false;
    }
    if (Sources.empty())
    {
        OutFault = {"LuaRuntimeSourceMissing", "Runtime requires at least one Lua source."};
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
    Impl->PinnedRepository = PinnedRepository;

    if (!Impl->OpenEnvironment(OutFault))
    {
        Stop();
        return false;
    }
    if (!Impl->LoadModules(Sources, &ReadResult.Bytes, OutFault))
    {
        Stop();
        return false;
    }
    return true;
}

bool FRuntimeSession::CheckScripts(
    const std::int32_t InSessionGeneration,
    const GV2ContentCore::FRepositoryReadHandle& PinnedRepository,
    const std::vector<FRuntimeSource>& Sources,
    std::size_t* OutModuleCount,
    FRuntimeFault& OutFault)
{
    OutFault = {};
    if (!Stop(&OutFault))
    {
        return false;
    }
    if (!PinnedRepository.IsValid())
    {
        OutFault = {"RepositoryNotReady", "Runtime session requires a valid pinned repository read handle."};
        return false;
    }
    if (InSessionGeneration <= 0)
    {
        OutFault = {"InvalidSessionGeneration", "Runtime requires a positive session generation."};
        return false;
    }
    if (Sources.empty())
    {
        OutFault = {"LuaRuntimeSourceMissing", "Runtime requires at least one Lua source."};
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
    Impl->PinnedRepository = PinnedRepository;

    if (!Impl->OpenEnvironment(OutFault))
    {
        Stop();
        return false;
    }
    const bool bSuccess = Impl->CheckScripts(Sources, OutModuleCount, OutFault);
    Stop();
    return bSuccess;
}

bool FRuntimeSession::Stop(FRuntimeFault* OutFault)
{
    if (OutFault != nullptr)
    {
        *OutFault = {};
    }
    if (Impl == nullptr)
    {
        return true;
    }
    if (Impl->State == nullptr)
    {
        Impl->PinnedRepository = {};
        Impl->SessionGeneration = 0;
        Impl->OwnerThread = {};
        Impl->bExecuting = false;
        return true;
    }
    if (!Impl->IsOwnerThread())
    {
        if (OutFault != nullptr)
        {
            *OutFault = {"RuntimeWrongThread", "Stop must execute on the session owner thread."};
        }
        return false;
    }
    if (Impl->bExecuting)
    {
        if (OutFault != nullptr)
        {
            *OutFault = {"LuaReentryRejected", "Stop attempted during active Lua execution."};
        }
        return false;
    }
    lua_close(Impl->State);
    Impl->State = nullptr;
    Impl->PinnedRepository = {};
    Impl->SessionGeneration = 0;
    Impl->OwnerThread = {};
    Impl->bExecuting = false;
    return true;
}

void FRuntimeSession::SetSaveSlotStorage(ISaveSlotStorage* Storage)
{
    if (Impl)
    {
        Impl->SaveSlotStorage = Storage;
    }
}

bool FRuntimeSession::RunLuaSpec(
    const std::string& SpecChunkName,
    const std::string& SpecSource,
    std::vector<FLuaSpecCaseResult>& OutCaseResults,
    FRuntimeFault& OutFault)
{
    return Impl->RunLuaSpec(SpecChunkName, SpecSource, OutCaseResults, OutFault);
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

bool FRuntimeSession::TakePendingScreen(
    std::optional<FScreenRequest>& OutRequest,
    FRuntimeFault& OutFault)
{
    OutRequest.reset();
    return Impl->CallTakePendingScreen(OutRequest, OutFault);
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

const GV2ContentCore::FRepositoryReadHandle& FRuntimeSession::GetPinnedRepository() const
{
    static const GV2ContentCore::FRepositoryReadHandle EmptyHandle;
    return Impl ? Impl->PinnedRepository : EmptyHandle;
}

std::string FRuntimeSession::GetCanonicalStateHash(FRuntimeFault* OutFault) const
{
    return Impl ? Impl->CallGetCanonicalStateHash(OutFault) : "";
}
}
