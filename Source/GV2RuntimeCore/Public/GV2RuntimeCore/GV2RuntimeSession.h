#pragma once

#include "GV2RuntimeCore/GV2RuntimeCoreAPI.h"
#include "GV2ContentCore/RepositorySnapshot.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace GV2RuntimeCore
{
struct FValue
{
    using FArray = std::vector<FValue>;
    using FObject = std::map<std::string, FValue, std::less<>>;
    using FStorage = std::variant<std::monostate, bool, std::int64_t, double, std::string, FArray, FObject>;

    FStorage Data;

    FValue() = default;
    explicit FValue(bool Value) : Data(Value) {}
    explicit FValue(std::int64_t Value) : Data(Value) {}
    explicit FValue(double Value) : Data(Value) {}
    explicit FValue(const char* Value) : Data(std::string(Value != nullptr ? Value : "")) {}
    explicit FValue(std::string Value) : Data(std::move(Value)) {}
    explicit FValue(FArray Value) : Data(std::move(Value)) {}
    explicit FValue(FObject Value) : Data(std::move(Value)) {}

    bool operator==(const FValue&) const = default;
};

struct FRuntimeFault
{
    std::string Code;
    std::string Message;
};

struct FCommandRequest
{
    std::string CommandId;
    FValue::FObject Args;
    std::int64_t Sequence = 0;
};

struct FSemanticInput
{
    std::int32_t SessionGeneration = 0;
    std::string UiInstanceId;
    std::int64_t Revision = 0;
    std::int64_t Sequence = 0;
    std::vector<std::string> NodeKeyPath;
    std::string ElementId;
    std::string CommandId;
    FValue::FObject Args;
};

struct FTextSpec
{
    std::string TextId;
    FValue::FObject Args;
    std::string Style;
};

struct FResourceReference
{
    std::string ResourceId;
};

struct FScreenField
{
    std::string FieldId;
    std::string SchemaId;
    FValue Value;
};

struct FScreenRequest
{
    std::string ScreenId;
    std::vector<FScreenField> Fields;
};

struct FRuntimeSource
{
    std::string Name;
    std::string Text;
};

struct FLuaSpecCaseResult
{
    std::string CaseId;
    bool Success = false;
    std::string ErrorMessage;
};

class GV2_PORTABLE_API FRuntimeSession
{
public:
    FRuntimeSession();
    ~FRuntimeSession();

    FRuntimeSession(const FRuntimeSession&) = delete;
    FRuntimeSession& operator=(const FRuntimeSession&) = delete;

    bool Start(
        std::int32_t InSessionGeneration,
        const GV2ContentCore::FRepositoryReadHandle& PinnedRepository,
        const std::vector<FRuntimeSource>& Sources,
        FRuntimeFault& OutFault);
    bool Stop(FRuntimeFault* OutFault = nullptr);

    bool CheckScripts(
        std::int32_t InSessionGeneration,
        const GV2ContentCore::FRepositoryReadHandle& PinnedRepository,
        const std::vector<FRuntimeSource>& Sources,
        std::size_t* OutModuleCount,
        FRuntimeFault& OutFault);

    // TAS-02: loads SpecSource as a Lua chunk (NOT via the module loader —
    // the spec never enters LoadedModulesRegistryKey / Scripts/ module
    // tree). The chunk must return a table mapping case name -> zero-arg
    // function (Tests/Lua spec format, BuildAndTooling.md). Requires the
    // session to already be started. OutFault is populated only for FORMAT
    // violations of the spec itself (chunk fails to compile/execute, return
    // value is not a table, a value is not a function, or the table is
    // empty) — a failing CASE is not a fault, it is recorded in
    // OutCaseResults with Success=false so remaining cases still run.
    // Case order is deterministic (ascending by CaseId), independent of
    // Lua's internal table iteration order. While the chunk and every case
    // run, `require()` is permitted for any module already loaded by the
    // session's production bootstrap.
    bool RunLuaSpec(
        const std::string& SpecChunkName,
        const std::string& SpecSource,
        std::vector<FLuaSpecCaseResult>& OutCaseResults,
        FRuntimeFault& OutFault);

    bool DispatchSemanticInput(const FSemanticInput& Input, FRuntimeFault& OutFault);
    bool DispatchCommand(const FCommandRequest& Request, FRuntimeFault& OutFault);
    bool TakePendingScreen(
        std::optional<FScreenRequest>& OutRequest,
        FRuntimeFault& OutFault);

    bool IsStarted() const;
    bool IsExecuting() const;
    std::int32_t GetSessionGeneration() const;
    const GV2ContentCore::FRepositoryReadHandle& GetPinnedRepository() const;
    std::string GetCanonicalStateHash(FRuntimeFault* OutFault = nullptr) const;

    static constexpr std::int32_t LuaReleaseNumber = 50408;

private:
    struct FImpl;
    std::unique_ptr<FImpl> Impl;
};
}
