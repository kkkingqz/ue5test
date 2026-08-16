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
class ISaveSlotStorage;

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

    // SAV-12 (plan SaveAndLoad, M4 Cold Start Load): starts the session
    // from a save slot instead of module defaults. Reads the slot through
    // Storage BEFORE any Lua VM is created — a missing or unreadable slot
    // fails as "SaveSlotNotFound"/"SaveSlotUnreadable" at zero VM cost. On
    // success, the whole preflight/decode/reference-rewrite pipeline (SAV-
    // 13/14/15/16) runs inside Lua (core:module.runtime.load) before any
    // canonical state is assigned; failure at any stage leaves the session
    // unstarted, exactly like a failed Start() above — never a partially
    // loaded state. NewGame's Start() above is entirely unaffected.
    bool StartFromSave(
        std::int32_t InSessionGeneration,
        const GV2ContentCore::FRepositoryReadHandle& PinnedRepository,
        const std::vector<FRuntimeSource>& Sources,
        ISaveSlotStorage& Storage,
        const std::string& SaveSlotId,
        FRuntimeFault& OutFault);

    bool Stop(FRuntimeFault* OutFault = nullptr);

    // SAV-05/06/10: wires the host's slot-scoped save storage into
    // game.save_slots for this session (composition root's job, per
    // GV2HostServices.h). Optional and safe to never call — Lua's
    // core:module.runtime.save.save() reports a typed
    // "SaveSlotStorageUnavailable" error if no storage was set, exactly as
    // its absence never blocks Start() above. Storage must outlive the
    // session; ownership stays with the caller.
    void SetSaveSlotStorage(ISaveSlotStorage* Storage);

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
