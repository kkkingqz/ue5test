#include "GV2RuntimeCore/Testing/GV2RegistryLifecycleConformance.h"

#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentCore/RepositoryBuilder.h"
#include "GV2ContentCore/RepositorySnapshot.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"

#include <string>
#include <vector>

namespace GV2RuntimeCore::Testing
{
namespace
{
GV2ContentCore::FRepositoryReadHandle MakeEmptyRepository()
{
    const GV2ContentCore::FPackageDescriptor EmptyCore("core", "core", 0u, {}, {});
    GV2ContentCore::FBuildOptions Options;
    GV2ContentCore::FBuildResult Result = GV2ContentCore::BuildRepository({EmptyCore}, Options);
    if (Result.IsFailure())
    {
        return {};
    }
    return Result.GetCandidate().GetReadHandle();
}

const std::vector<std::string> ContractExpectedOrder = {
    "authoring",
    "services",
    "actions",
    "entity_extensions",
    "commands.validators",
    "commands.handlers",
    "events.subscribers",
    "events",
    "instances.actors",
    "instances",
    "presentation",
    "state_validator",
};

std::string BuildRegistryLifecycleSource(const std::string& Mode, const std::string& TargetParticipant)
{
    std::string Src = R"lua(
local M = {
    id = "core:module.bootstrap.registry_lifecycle",
}

local PARTICIPANTS = {
    "authoring",
    "services",
    "actions",
    "entity_extensions",
    "commands.validators",
    "commands.handlers",
    "events.subscribers",
    "events",
    "instances.actors",
    "instances",
    "presentation",
    "state_validator",
}

local mode = ")lua" + Mode + R"lua("
local target = ")lua" + TargetParticipant + R"lua("

function M.install()
    if not _G.game then _G.game = {} end
    _G.game.debug = _G.game.debug or {}
    _G.game.debug.installed_registries = {}
    _G.game.debug.seal_order = {}

    for _, name in ipairs(PARTICIPANTS) do
        if mode == "missing_participant" and name == target then
            -- omit creation
        else
            local reg = {
                _frozen = false,
                is_frozen = function(self)
                    if mode == "not_frozen" and name == target then
                        return false
                    end
                    if mode == "non_bool_frozen" and name == target then
                        return "invalid_type"
                    end
                    if mode == "throwing_is_frozen" and name == target then
                        error("INJECTED_IS_FROZEN_THROW")
                    end
                    return self._frozen == true
                end,
            }
            if not (mode == "missing_seal" and name == target) then
                reg.freeze = function(self)
                    if mode == "throw" and name == target then
                        error("INJECTED_FREEZE_THROW")
                    end
                    self._frozen = true
                end
            end
            if mode == "throwing_resolve" and name == target then
                reg.resolve = function(self)
                    error("INJECTED_RESOLVE_THROW")
                end
            end

            _G.game.debug.installed_registries[name] = reg
        end
    end
end

function M.seal()
    _G.game.debug = _G.game.debug or {}
    local order = {}
    _G.game.debug.seal_order = order

    for _, name in ipairs(PARTICIPANTS) do
        local target_obj = _G.game.debug.installed_registries and _G.game.debug.installed_registries[name]
        if target_obj == nil then
            return false, {
                phase = "SealingRegistries",
                registry_path = name,
                error = "MissingParticipant: registry object for '" .. name .. "' is missing",
            }
        end

        if target_obj.resolve then
            local ok_res, err_res = pcall(target_obj.resolve, target_obj)
            if not ok_res then
                return false, {
                    phase = "SealingRegistries",
                    registry_path = name,
                    error = "ResolveFailed: " .. tostring(err_res),
                }
            end
        end

        local freeze_fn = target_obj.freeze or target_obj.seal
        if type(freeze_fn) ~= "function" then
            return false, {
                phase = "SealingRegistries",
                registry_path = name,
                error = "MissingSealMethod: seal operation for '" .. name .. "' is missing or not a function",
            }
        end

        local ok_seal, err_seal = pcall(freeze_fn, target_obj)
        if not ok_seal then
            return false, {
                phase = "SealingRegistries",
                registry_path = name,
                error = "SealFailed: " .. tostring(err_seal),
            }
        end

        local is_frozen_fn = target_obj.is_frozen
        if type(is_frozen_fn) ~= "function" then
            return false, {
                phase = "SealingRegistries",
                registry_path = name,
                error = "MissingIsFrozenMethod: is_frozen predicate for '" .. name .. "' is missing or not a function",
            }
        end

        local ok_f, is_f = pcall(is_frozen_fn, target_obj)
        if not ok_f then
            return false, {
                phase = "SealingRegistries",
                registry_path = name,
                error = "IsFrozenCheckFailed: " .. tostring(is_f),
            }
        end

        if is_f ~= true then
            return false, {
                phase = "SealingRegistries",
                registry_path = name,
                error = "RegistryNotFrozen: registry '" .. name .. "' was not frozen (expected true, got " .. tostring(is_f) .. ")",
            }
        end

        table.insert(order, name)
    end

    return true
end

return M
)lua";
    return Src;
}

const char* StateCompositionStubSource = R"lua(
local M = {
    id = "core:module.runtime.state_composition",
}

function M.compose_default_state(ctx, modules)
    local tree = {}
    for _, entry in ipairs(modules or {}) do
        local hook = entry.create_default_state or (entry.module and entry.module.create_default_state)
        if hook then
            local contrib = hook(ctx)
            if contrib then
                for k, v in pairs(contrib) do
                    tree[k] = v
                end
            end
        end
    end
    return tree, nil
end

return M
)lua";

const char* MainModuleSource = R"lua(
local lifecycle = require("core:module.bootstrap.registry_lifecycle")
lifecycle.install()

local M = {
    id = "core:module.bootstrap.main",
}

function M.register(ctx)
    game.debug = game.debug or {}
    game.debug.register_called = true
    game.runtime = game.runtime or {}
    game.runtime.get_canonical_state_hash = function()
        return "conformance_canonical_state_hash"
    end
end

function M.create_default_state(ctx)
    game.debug = game.debug or {}
    game.debug.state_build_called = true
    return nil
end

function M.start(ctx)
    game.debug = game.debug or {}
    game.debug.start_called = true
end

return M
)lua";

std::vector<FRuntimeSource> MakeSources(const std::string& Mode, const std::string& TargetParticipant, bool bIncludeLifecycle = true)
{
    std::string Manifest;
    if (bIncludeLifecycle)
    {
        Manifest = R"lua(
return {
    entry_module_id = "core:module.bootstrap.main",
    modules = {
        {
            module_id = "core:module.bootstrap.registry_lifecycle",
            source = "bootstrap/registry_lifecycle.lua",
            dependencies = {},
        },
        {
            module_id = "core:module.runtime.state_composition",
            source = "runtime/state_composition.lua",
            dependencies = {},
        },
        {
            module_id = "core:module.bootstrap.main",
            source = "bootstrap/main.lua",
            dependencies = {
                "core:module.bootstrap.registry_lifecycle",
                "core:module.runtime.state_composition",
            },
        },
    },
}
)lua";
        return {
            {"@core/bootstrap/manifest.lua", Manifest},
            {"@core/bootstrap/registry_lifecycle.lua", BuildRegistryLifecycleSource(Mode, TargetParticipant)},
            {"@core/runtime/state_composition.lua", StateCompositionStubSource},
            {"@core/bootstrap/main.lua", MainModuleSource},
        };
    }
    else
    {
        Manifest = R"lua(
return {
    entry_module_id = "core:module.bootstrap.main",
    modules = {
        {
            module_id = "core:module.bootstrap.main",
            source = "bootstrap/main.lua",
            dependencies = {},
        },
    },
}
)lua";
        return {
            {"@core/bootstrap/manifest.lua", Manifest},
            {"@core/bootstrap/main.lua", R"lua(
local M = { id = "core:module.bootstrap.main" }
return M
)lua"},
        };
    }
}
} // namespace

std::string RunRegistryLifecycleConformance()
{
    const GV2ContentCore::FRepositoryReadHandle RepoHandle = MakeEmptyRepository();
    if (!RepoHandle.IsValid())
    {
        return "registry_lifecycle_conformance.empty_repository_build_failed";
    }

    // 1. Missing registry_lifecycle module: must fail with RegistryLifecycleMissing
    {
        FRuntimeSession Session;
        FRuntimeFault Fault;
        const std::vector<FRuntimeSource> Sources = MakeSources("valid", "", false);
        const bool bStarted = Session.Start(1, RepoHandle, Sources, Fault);
        if (bStarted)
        {
            Session.Stop();
            return "registry_lifecycle_conformance.missing_lifecycle_module_unexpected_success";
        }
        if (Fault.Code != "RegistryLifecycleMissing")
        {
            return "registry_lifecycle_conformance.missing_lifecycle_wrong_code: " + Fault.Code + ": " + Fault.Message;
        }
        if (Fault.Message.find("SealingRegistries") == std::string::npos)
        {
            return "registry_lifecycle_conformance.missing_lifecycle_missing_phase: " + Fault.Message;
        }
    }

    // 2. Fault matrix over actual participant inventory:
    // For every participant in ContractExpectedOrder, verify failure causes Start() to fail
    // and OutFault to carry phase=SealingRegistries and the participant's path.
    for (const std::string& Participant : ContractExpectedOrder)
    {
        // 2a. Throwing freeze (reproduction of RUNTIME-AF-01)
        {
            FRuntimeSession Session;
            FRuntimeFault Fault;
            const std::vector<FRuntimeSource> Sources = MakeSources("throw", Participant);
            const bool bStarted = Session.Start(1, RepoHandle, Sources, Fault);
            if (bStarted)
            {
                Session.Stop();
                return "registry_lifecycle_conformance.throw_unexpected_success: " + Participant;
            }
            if (Fault.Code != "RegistrySealingFailed")
            {
                return "registry_lifecycle_conformance.throw_wrong_code: " + Participant + " got " + Fault.Code;
            }
            if (Fault.Message.find("SealingRegistries") == std::string::npos)
            {
                return "registry_lifecycle_conformance.throw_missing_phase: " + Participant + " message: " + Fault.Message;
            }
            if (Fault.Message.find(Participant) == std::string::npos)
            {
                return "registry_lifecycle_conformance.throw_missing_participant: " + Participant + " message: " + Fault.Message;
            }
            if (Fault.Message.find("INJECTED_FREEZE_THROW") == std::string::npos)
            {
                return "registry_lifecycle_conformance.throw_missing_detail: " + Participant + " message: " + Fault.Message;
            }
            if (!Session.GetCanonicalStateHash().empty())
            {
                return "registry_lifecycle_conformance.throw_state_hash_not_empty: " + Participant;
            }
            Session.Stop();
        }

        // 2b. is_frozen returning false
        {
            FRuntimeSession Session;
            FRuntimeFault Fault;
            const std::vector<FRuntimeSource> Sources = MakeSources("not_frozen", Participant);
            const bool bStarted = Session.Start(1, RepoHandle, Sources, Fault);
            if (bStarted)
            {
                Session.Stop();
                return "registry_lifecycle_conformance.not_frozen_unexpected_success: " + Participant;
            }
            if (Fault.Code != "RegistrySealingFailed")
            {
                return "registry_lifecycle_conformance.not_frozen_wrong_code: " + Participant + " got " + Fault.Code;
            }
            if (Fault.Message.find(Participant) == std::string::npos)
            {
                return "registry_lifecycle_conformance.not_frozen_missing_participant: " + Participant + " message: " + Fault.Message;
            }
            if (Fault.Message.find("RegistryNotFrozen") == std::string::npos)
            {
                return "registry_lifecycle_conformance.not_frozen_missing_detail: " + Participant + " message: " + Fault.Message;
            }
            Session.Stop();
        }

        // 2c. is_frozen returning non-boolean
        {
            FRuntimeSession Session;
            FRuntimeFault Fault;
            const std::vector<FRuntimeSource> Sources = MakeSources("non_bool_frozen", Participant);
            const bool bStarted = Session.Start(1, RepoHandle, Sources, Fault);
            if (bStarted)
            {
                Session.Stop();
                return "registry_lifecycle_conformance.non_bool_unexpected_success: " + Participant;
            }
            if (Fault.Code != "RegistrySealingFailed")
            {
                return "registry_lifecycle_conformance.non_bool_wrong_code: " + Participant + " got " + Fault.Code;
            }
            if (Fault.Message.find(Participant) == std::string::npos)
            {
                return "registry_lifecycle_conformance.non_bool_missing_participant: " + Participant + " message: " + Fault.Message;
            }
            Session.Stop();
        }

        // 2d. Missing participant object
        {
            FRuntimeSession Session;
            FRuntimeFault Fault;
            const std::vector<FRuntimeSource> Sources = MakeSources("missing_participant", Participant);
            const bool bStarted = Session.Start(1, RepoHandle, Sources, Fault);
            if (bStarted)
            {
                Session.Stop();
                return "registry_lifecycle_conformance.missing_participant_unexpected_success: " + Participant;
            }
            if (Fault.Code != "RegistrySealingFailed")
            {
                return "registry_lifecycle_conformance.missing_participant_wrong_code: " + Participant + " got " + Fault.Code;
            }
            if (Fault.Message.find("MissingParticipant") == std::string::npos)
            {
                return "registry_lifecycle_conformance.missing_participant_missing_detail: " + Participant + " message: " + Fault.Message;
            }
            Session.Stop();
        }

        // 2e. Missing seal / freeze method
        {
            FRuntimeSession Session;
            FRuntimeFault Fault;
            const std::vector<FRuntimeSource> Sources = MakeSources("missing_seal", Participant);
            const bool bStarted = Session.Start(1, RepoHandle, Sources, Fault);
            if (bStarted)
            {
                Session.Stop();
                return "registry_lifecycle_conformance.missing_seal_unexpected_success: " + Participant;
            }
            if (Fault.Code != "RegistrySealingFailed")
            {
                return "registry_lifecycle_conformance.missing_seal_wrong_code: " + Participant + " got " + Fault.Code;
            }
            if (Fault.Message.find("MissingSealMethod") == std::string::npos)
            {
                return "registry_lifecycle_conformance.missing_seal_missing_detail: " + Participant + " message: " + Fault.Message;
            }
            Session.Stop();
        }
    }

    // 3. Positive verification:
    // When sealing succeeds, Start() succeeds, state build and start hooks are executed,
    // and participants were sealed in the exact contract order.
    {
        FRuntimeSession Session;
        FRuntimeFault Fault;
        const std::vector<FRuntimeSource> Sources = MakeSources("valid", "");
        const bool bStarted = Session.Start(1, RepoHandle, Sources, Fault);
        if (!bStarted)
        {
            return "registry_lifecycle_conformance.valid_session_start_failed: " + Fault.Code + ": " + Fault.Message;
        }

        if (Session.GetCanonicalStateHash().empty())
        {
            Session.Stop();
            return "registry_lifecycle_conformance.valid_state_hash_empty";
        }

        std::vector<FLuaSpecCaseResult> CaseResults;
        const bool bSpecOk = Session.RunLuaSpec(
            "@registry_lifecycle_conformance_spec",
            R"lua(
return {
    state_and_start_hooks_fired = function()
        assert(game.debug.register_called == true, "register hook must have fired")
        assert(game.debug.state_build_called == true, "create_default_state hook must have fired")
        assert(game.debug.start_called == true, "start hook must have fired")
    end,

    contract_order_matches = function()
        local actual = game.debug.seal_order
        assert(type(actual) == "table", "seal_order must be a table")
        assert(#actual == 12, "expected 12 sealed participants, got " .. tostring(#actual))

        local expected = {
            "authoring",
            "services",
            "actions",
            "entity_extensions",
            "commands.validators",
            "commands.handlers",
            "events.subscribers",
            "events",
            "instances.actors",
            "instances",
            "presentation",
            "state_validator",
        }

        for i, exp in ipairs(expected) do
            assert(actual[i] == exp, "participant order mismatch at index " .. i .. ": expected " .. exp .. ", got " .. tostring(actual[i]))
        end
    end,
}
)lua",
            CaseResults,
            Fault);

        Session.Stop();

        if (!bSpecOk)
        {
            return "registry_lifecycle_conformance.valid_spec_failed: " + Fault.Code + ": " + Fault.Message;
        }

        for (const auto& Res : CaseResults)
        {
            if (!Res.Success)
            {
                return "registry_lifecycle_conformance.case_failed: " + Res.CaseId + ": " + Res.ErrorMessage;
            }
        }
    }

    return "";
}
} // namespace GV2RuntimeCore::Testing
