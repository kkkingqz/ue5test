#pragma once

#include "GV2RuntimeCore/GV2RuntimeCoreAPI.h"
#include "GV2ContentCore/RepositorySnapshot.h"
#include "GV2ContentCore/Value.h"

#include <cstdint>
#include <functional>
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

inline GV2ContentCore::FValue RuntimeValueToContentValue(const FValue& InValue)
{
    if (std::holds_alternative<std::monostate>(InValue.Data))
    {
        return GV2ContentCore::FValue::MakeNull();
    }
    if (std::holds_alternative<bool>(InValue.Data))
    {
        return GV2ContentCore::FValue(std::get<bool>(InValue.Data));
    }
    if (std::holds_alternative<std::int64_t>(InValue.Data))
    {
        return GV2ContentCore::FValue(std::get<std::int64_t>(InValue.Data));
    }
    if (std::holds_alternative<double>(InValue.Data))
    {
        return GV2ContentCore::FValue(std::get<double>(InValue.Data));
    }
    if (std::holds_alternative<std::string>(InValue.Data))
    {
        return GV2ContentCore::FValue(std::get<std::string>(InValue.Data));
    }
    if (std::holds_alternative<FValue::FArray>(InValue.Data))
    {
        GV2ContentCore::FValue::FArray Array;
        for (const auto& Item : std::get<FValue::FArray>(InValue.Data))
        {
            Array.push_back(RuntimeValueToContentValue(Item));
        }
        return GV2ContentCore::FValue(std::move(Array));
    }
    if (std::holds_alternative<FValue::FObject>(InValue.Data))
    {
        GV2ContentCore::FValue::FObject Object;
        for (const auto& [Key, Val] : std::get<FValue::FObject>(InValue.Data))
        {
            Object.emplace_back(Key, RuntimeValueToContentValue(Val));
        }
        return GV2ContentCore::FValue(std::move(Object));
    }
    return GV2ContentCore::FValue::MakeNull();
}

struct FRuntimeFault
{
    std::string Code;
    std::string Message;
};

enum class ERuntimeLifecyclePhase
{
    Registering,
    BuildingState,
    RestoringInstances,
    Starting
};

enum class ERuntimePhaseResultKind
{
    Completed,
    Fault
};

struct FRuntimePhaseResult
{
    ERuntimePhaseResultKind Kind = ERuntimePhaseResultKind::Completed;
    FRuntimeFault Fault;

    static FRuntimePhaseResult MakeCompleted()
    {
        return {ERuntimePhaseResultKind::Completed, {}};
    }

    static FRuntimePhaseResult MakeFault(FRuntimeFault InFault)
    {
        return {ERuntimePhaseResultKind::Fault, std::move(InFault)};
    }

    bool IsCompleted() const { return Kind == ERuntimePhaseResultKind::Completed; }
    bool IsFault() const { return Kind == ERuntimePhaseResultKind::Fault; }
};

// Called once for every executed protected phase with its closed result. Returning
// false requests cancellation only after Completed; a reported Fault remains the
// terminal cause and cannot be replaced by callback cancellation.
using FPhaseCompletionCallback = std::function<bool(ERuntimeLifecyclePhase Phase, const FRuntimePhaseResult& Result)>;


struct FCommandRequest
{
    std::string CommandId;
    FValue::FObject Args;
    std::int64_t Sequence = 0;
};

struct FHostControlRequest
{
    std::string Kind;
    std::string SlotId;
    std::string Revision;
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

    bool operator==(const FScreenField&) const = default;
};

struct FScreenInstance
{
    std::string Layer;
    std::string InstanceKey;
    std::string ScreenId;
    std::vector<FScreenField> Fields;

    bool operator==(const FScreenInstance&) const = default;
};

struct FUiDocument
{
    std::string UiInstanceId;
    std::int64_t Revision = 0;
    std::optional<FScreenInstance> Route;
    std::vector<FScreenInstance> Overlays;
    std::vector<FScreenInstance> Modals;

    bool operator==(const FUiDocument&) const = default;
};

struct FScreenRequest
{
    std::string ScreenId;
    std::vector<FScreenField> Fields;

    bool operator==(const FScreenRequest&) const = default;
};

// PEP-03 (ADR-0047): one-shot presentation effect. Source (Lua-published vs host-local)
// is deliberately absent -- ADR-0047's whole point is that source does not belong in
// identity. Target fields are flattened (not a nested optional struct) to mirror
// FSemanticInput's own shape above, the established precedent at this same layer for
// "which UI instance/revision does this message belong to". bHasTarget distinguishes an
// untargeted effect (always applicable) from a targeted one pointing at generation 0/
// empty instance id, which would otherwise be indistinguishable from a genuine target.
struct FPresentationEffect
{
    std::string EffectId;
    std::int64_t Sequence = 0;
    bool bHasTarget = false;
    std::int32_t TargetSessionGeneration = 0;
    std::string TargetUiInstanceId;
    std::int64_t TargetRevision = 0;
    FValue::FObject Args;

    bool operator==(const FPresentationEffect&) const = default;
};

// PEP-03 (ADR-0047 §4): the three distinct rejection causes the contract requires --
// a bare bool would make "delivered to the wrong session" indistinguishable from
// ordinary stale-target discarding.
enum class EPresentationEffectRejectReason : std::uint8_t
{
    None,
    StaleTarget,
    WrongSessionGeneration,
    StaleRevision,
};

// Pure function, no session/Lua state: given an effect and the CURRENT session/document
// coordinates it would be applied against, decides whether it still applies. An
// untargeted effect (bHasTarget == false) always resolves None -- it names no UI
// coordinate to have gone stale against. Order of checks matters for which single
// reason a triply-wrong target reports: generation first (a completely different
// session), then instance (same session, different/torn-down UI document), then
// revision (same instance, document has since moved on).
GV2_PORTABLE_API EPresentationEffectRejectReason ResolveEffectTarget(
    const FPresentationEffect& Effect,
    std::int32_t CurrentSessionGeneration,
    const std::string& CurrentUiInstanceId,
    std::int64_t CurrentRevision);

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

struct FReplacedModuleInfo
{
    std::string ModuleId;
    std::vector<std::string> Providers;

    bool operator==(const FReplacedModuleInfo&) const = default;
};

struct GV2_PORTABLE_API FSessionStartInputs final
{
    std::int32_t SessionGeneration = 1;
    std::string SeedHex;
    std::string Mode = "NewGame";
    std::string RepositoryVersion;
    std::string RepositoryContentHash;

    FSessionStartInputs() = default;
    explicit FSessionStartInputs(std::string InSeedHex, std::int32_t InSessionGeneration = 1)
        : SessionGeneration(InSessionGeneration), SeedHex(std::move(InSeedHex))
    {
    }

    bool operator==(const FSessionStartInputs&) const = default;
};

inline bool IsValidSeedHex(std::string_view SeedHex)
{
    if (SeedHex.size() != 16)
    {
        return false;
    }
    for (char C : SeedHex)
    {
        if (!((C >= '0' && C <= '9') || (C >= 'a' && C <= 'f')))
        {
            return false;
        }
    }
    return true;
}


class GV2_PORTABLE_API FRuntimeSession
{
public:
    FRuntimeSession();
    ~FRuntimeSession();

    FRuntimeSession(const FRuntimeSession&) = delete;
    FRuntimeSession& operator=(const FRuntimeSession&) = delete;

    bool Start(
        const FSessionStartInputs& StartInputs,
        const GV2ContentCore::FRepositoryReadHandle& PinnedRepository,
        const std::vector<FRuntimeSource>& Sources,
        FRuntimeFault& OutFault);

    bool Start(
        std::int32_t InSessionGeneration,
        const std::string& InSeedHex,
        const GV2ContentCore::FRepositoryReadHandle& PinnedRepository,
        const std::vector<FRuntimeSource>& Sources,
        FRuntimeFault& OutFault);

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
        const FSessionStartInputs& StartInputs,
        const GV2ContentCore::FRepositoryReadHandle& PinnedRepository,
        const std::vector<FRuntimeSource>& Sources,
        ISaveSlotStorage& Storage,
        const std::string& SaveSlotId,
        FRuntimeFault& OutFault);

    bool StartFromSave(
        std::int32_t InSessionGeneration,
        const GV2ContentCore::FRepositoryReadHandle& PinnedRepository,
        const std::vector<FRuntimeSource>& Sources,
        ISaveSlotStorage& Storage,
        const std::string& SaveSlotId,
        FRuntimeFault& OutFault);

    // CFC-10: starts the session from pre-captured save bytes instead of reading
    // from disk storage. Used by unified session replacement where the slot was
    // already read once and preflighted in the prior active VM.
    bool StartFromSaveBytes(
        const FSessionStartInputs& StartInputs,
        const GV2ContentCore::FRepositoryReadHandle& PinnedRepository,
        const std::vector<FRuntimeSource>& Sources,
        const std::string& SaveBytes,
        FRuntimeFault& OutFault,
        const FPhaseCompletionCallback& PhaseCallback = {});

    bool StartSessionPhases(
        const FSessionStartInputs& StartInputs,
        const GV2ContentCore::FRepositoryReadHandle& PinnedRepository,
        const std::vector<FRuntimeSource>& Sources,
        const std::string* LoadContainerBytes,
        const FPhaseCompletionCallback& PhaseCallback,
        FRuntimeFault& OutFault);

    bool StartSessionPhases(
        std::int32_t InSessionGeneration,
        const GV2ContentCore::FRepositoryReadHandle& PinnedRepository,
        const std::vector<FRuntimeSource>& Sources,
        const std::string* LoadContainerBytes,
        const FPhaseCompletionCallback& PhaseCallback,
        FRuntimeFault& OutFault);

    bool Stop(FRuntimeFault* OutFault = nullptr, const std::string& Reason = "teardown");

    static std::int32_t GetLiveVmCount();

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

    bool CheckScripts(
        std::int32_t InSessionGeneration,
        const GV2ContentCore::FRepositoryReadHandle& PinnedRepository,
        const std::vector<FRuntimeSource>& Sources,
        std::size_t* OutModuleCount,
        std::string* OutScriptSetHash,
        std::vector<FReplacedModuleInfo>* OutReplacedModules,
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
    bool TakePendingDocument(
        std::optional<FUiDocument>& OutDocument,
        FRuntimeFault& OutFault);
    bool SaveToSlot(const std::string& SlotId, FRuntimeFault& OutFault);
    bool PreflightSaveBytes(const std::string& Bytes, FRuntimeFault& OutFault);
    bool TakePendingControlRequests(
        std::vector<FHostControlRequest>& OutRequests,
        FRuntimeFault& OutFault);

    // PEP-03 (ADR-0047): host-local source -- a presentation event with no gameplay
    // meaning (hover, self-dismiss timer). Assigns the next Sequence from the SAME
    // counter TakePendingEffects below uses for Lua-published effects, appends
    // immediately; Effect.Sequence on entry is ignored (overwritten), matching
    // TakePendingEffects' own assignment-at-enqueue behavior for the Lua-published half.
    bool PublishHostLocalEffect(FPresentationEffect Effect, FRuntimeFault& OutFault);

    // Pulls every effect Lua committed since the last call (game.ui.take_pending_effects,
    // mirroring take_pending_requests' staged/committed queue -- see outbound.lua),
    // assigns each the next Sequence in array order, then returns AND CLEARS every
    // pending effect -- both the ones just pulled from Lua and any queued directly via
    // PublishHostLocalEffect since the previous call. One counter, one queue, one drain
    // point: this is the only way either source's effects leave this session.
    bool TakePendingEffects(
        std::vector<FPresentationEffect>& OutEffects,
        FRuntimeFault& OutFault);

    bool IsStarted() const;
    bool IsExecuting() const;
    std::int32_t GetSessionGeneration() const;
    std::string GetSeedHex() const;
    const GV2ContentCore::FRepositoryReadHandle& GetPinnedRepository() const;
    std::string GetCanonicalStateHash(FRuntimeFault* OutFault = nullptr) const;
    std::string GetScriptSetHash() const;
    std::vector<FReplacedModuleInfo> GetReplacedModules() const;

    static constexpr std::int32_t LuaReleaseNumber = 50408;

private:
    struct FImpl;
    std::unique_ptr<FImpl> Impl;
};
}
