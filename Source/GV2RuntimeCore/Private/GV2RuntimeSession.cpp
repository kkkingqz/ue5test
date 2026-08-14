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
#include <functional>
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

bool ReadFlatScalarObject(
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
            return false;
        }

        std::size_t KeyLength = 0;
        const char* KeyData = lua_tolstring(State, -2, &KeyLength);
        std::string Key(KeyData, KeyLength);
        if (Key.empty())
        {
            OutFault = {"LuaScreenRequestInvalid", "Rich text span args contain an empty field name."};
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
            return false;
        }
        OutObject.emplace(std::move(Key), std::move(Value));
        lua_pop(State, 1);
    }
    return true;
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
        lua_createtable(State, 0, 1);
        lua_setfield(State, -2, "ui");
        lua_createtable(State, 0, 2);
        lua_setfield(State, -2, "debug");
        lua_createtable(State, 0, 0);
        lua_setfield(State, -2, "null");
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
                OutFault = {"LuaModuleSourceMissing", "Declared module source is missing: " + Spec.SourceName};
                return false;
            }
            for (const std::string& Dependency : Spec.Dependencies)
            {
                if (SpecIndexById.find(Dependency) == SpecIndexById.end())
                {
                    OutFault = {"LuaModuleDependencyMissing", "Declared module dependency is missing: " + Dependency};
                    return false;
                }
            }
        }
        if (OutSourcesByName.size() != DeclaredSourceNames.size() + 1)
        {
            OutFault = {"LuaModuleSourceUnlisted", "Every Lua source except the manifest must declare exactly one module."};
            return false;
        }

        std::map<std::string, int, std::less<>> VisitState;
        std::function<bool(const std::string&)> Visit = [&](const std::string& ModuleId)
        {
            int& StateValue = VisitState[ModuleId];
            if (StateValue == 1)
            {
                OutFault = {"LuaModuleDependencyCycle", "Module dependency graph contains a cycle at: " + ModuleId};
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
            ReadLuaError(State, "LuaModuleLoadError", "Lua module failed to compile.", OutFault);
            return false;
        }
        if (lua_pcall(State, 0, 1, ErrorHandler) != LUA_OK)
        {
            ReadLuaError(State, "LuaModuleLoadError", "Lua module failed to initialize.", OutFault);
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

    bool LoadModules(const std::vector<FRuntimeSource>& Sources, FRuntimeFault& OutFault)
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
    const std::vector<FRuntimeSource>& Sources,
    FRuntimeFault& OutFault)
{
    Stop();
    OutFault = {};
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

    if (!Impl->OpenEnvironment(OutFault))
    {
        Stop();
        return false;
    }
    if (!Impl->LoadModules(Sources, OutFault))
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
}
