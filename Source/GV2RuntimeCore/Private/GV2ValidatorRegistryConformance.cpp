#include "GV2RuntimeCore/Testing/GV2ValidatorRegistryConformance.h"

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
// GameDataRepositoryContract.md "Empty core descriptor создаёт допустимый
// пустой snapshot" - no source provider is needed since no sources are
// declared. This suite only exercises the Lua-side validator registry, not
// content, so an empty pinned repository is sufficient.
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

// Verbatim copy of Scripts/runtime/stable_id.lua (validator_registry.lua's
// only dependency); kept in sync manually like the other embedded module
// copies in this Testing/ suite (e.g. GV2LuaLifecycleConformance.cpp).
const char* StableIdScriptSource = R"lua(
local M = {}

local function is_segment(value)
    return type(value) == "string"
        and #value >= 1
        and #value <= 64
        and value:match("^[a-z][a-z0-9_]*$") ~= nil
end

function M.is_kind(value, expected_kind)
    if type(value) ~= "string" or #value > 192 or not is_segment(expected_kind) then
        return false
    end

    local namespace, kind, path = value:match("^([^:]+):([^.]+)%.(.+)$")
    if not is_segment(namespace)
        or kind ~= expected_kind
        or path == nil
        or path:sub(1, 1) == "."
        or path:sub(-1) == "."
        or path:find("..", 1, true) ~= nil
    then
        return false
    end

    local consumed = 0
    for segment in path:gmatch("[^.]+") do
        if not is_segment(segment) then
            return false
        end
        consumed = consumed + #segment
    end

    local separators = select(2, path:gsub("%.", ""))
    return consumed + separators == #path
end

return M
)lua";

// Verbatim copy of Scripts/runtime/validator_registry.lua (GEW-01).
const char* ValidatorRegistryScriptSource = R"lua(
local stable_id = require("core:module.runtime.stable_id")

local M = {
    id = "core:module.runtime.validator_registry",
}

function M.create_registry()
    local entries = {}
    local entries_by_id = {}
    local is_frozen = false
    local next_sequence = 1

    local registry = {}

    function registry.register(id, validator_impl, options)
        if is_frozen then
            error("ValidatorRegistryFrozen: cannot register validator '" .. tostring(id) .. "' after register phase / freeze", 2)
        end
        if type(id) ~= "string" or not stable_id.is_kind(id, "validator") then
            error("InvalidValidatorId: validator id must be a canonical Stable ID of kind 'validator', got '" .. tostring(id) .. "'", 2)
        end
        if entries_by_id[id] ~= nil then
            error("ValidatorDuplicateRegistration: validator '" .. id .. "' is already registered", 2)
        end
        if type(validator_impl) ~= "table" then
            error("InvalidValidatorImpl: validator implementation must be a table", 2)
        end

        local priority = 0
        if options ~= nil then
            if type(options) ~= "table" then
                error("InvalidValidatorOptions: options must be a table", 2)
            end
            if options.priority ~= nil then
                if type(options.priority) ~= "number" or math.type(options.priority) ~= "integer" then
                    error("InvalidValidatorPriority: priority must be an integer", 2)
                end
                priority = options.priority
            end
        end

        local entry = {
            id = id,
            impl = validator_impl,
            priority = priority,
            sequence = next_sequence,
        }
        next_sequence = next_sequence + 1
        entries_by_id[id] = entry
        table.insert(entries, entry)
        return validator_impl
    end

    function registry.get(id)
        local entry = entries_by_id[id]
        return entry and entry.impl or nil
    end

    function registry.exists(id)
        return type(id) == "string" and entries_by_id[id] ~= nil
    end

    function registry.freeze()
        if is_frozen then
            return
        end
        table.sort(entries, function(a, b)
            if a.priority ~= b.priority then
                return a.priority < b.priority
            end
            return a.sequence < b.sequence
        end)
        is_frozen = true
    end

    function registry.is_frozen()
        return is_frozen
    end

    function registry.ordered()
        local result = {}
        for i, entry in ipairs(entries) do
            result[i] = { id = entry.id, impl = entry.impl }
        end
        return result
    end

    local public_facade = {}
    local mt = {
        __index = function(_, k)
            if registry[k] ~= nil then
                return registry[k]
            end
            local entry = entries_by_id[k]
            return entry and entry.impl or nil
        end,
        __newindex = function(_, k, v)
            error("ValidatorRegistryDirectAssignmentDisallowed: use game.commands.validators.register(id, validator, options) to register a validator", 2)
        end,
        __tostring = function(_)
            return "GameplayValidatorRegistry"
        end,
    }
    setmetatable(public_facade, mt)
    return public_facade
end

function M.run_conformance()
    local registry = M.create_registry()

    registry.register("core:validator.test.high_priority", {}, { priority = 5 })
    registry.register("core:validator.test.tie_first", {}, { priority = 1 })
    registry.register("core:validator.test.tie_second", {}, { priority = 1 })
    registry.register("core:validator.test.default_priority", {})

    registry.freeze()

    local ordered = registry.ordered()
    local expected_order = {
        "core:validator.test.default_priority",
        "core:validator.test.tie_first",
        "core:validator.test.tie_second",
        "core:validator.test.high_priority",
    }
    if #ordered ~= #expected_order then
        return "validator_registry.order_length_mismatch"
    end
    for i, expected_id in ipairs(expected_order) do
        if ordered[i].id ~= expected_id then
            return "validator_registry.order_mismatch"
        end
    end

    local late_ok = pcall(function()
        registry.register("core:validator.test.late", {})
    end)
    if late_ok then
        return "validator_registry.late_registration_not_rejected"
    end

    local fresh = M.create_registry()
    local invalid_id_ok = pcall(function()
        fresh.register("core:item.not_a_validator", {})
    end)
    if invalid_id_ok then
        return "validator_registry.invalid_id_not_rejected"
    end

    local duplicate_ok = pcall(function()
        fresh.register("core:validator.test.dup", {})
        fresh.register("core:validator.test.dup", {})
    end)
    if duplicate_ok then
        return "validator_registry.duplicate_not_rejected"
    end

    return ""
end

function M.register(_ctx)
    if not game then
        game = {}
    end
    if not game.commands then
        game.commands = {}
    end
    game.commands.validators = M.create_registry()
end

return M
)lua";

// Driver module: exercises the real run_conformance() on a throwaway
// registry during "register", then verifies the live game.commands.validators
// facade wiring and freeze enforcement during "start" (i.e. after the
// host's register-phase freeze step has already run for every module).
const char* DriverScriptSource = R"lua(
local validator_registry = require("core:module.runtime.validator_registry")

local M = {
    id = "core:module.test.validator_registry_driver",
}

function M.register(ctx)
    local failure = validator_registry.run_conformance()
    assert(failure == "", "validator_registry conformance failed: " .. tostring(failure))
end

function M.start(ctx)
    assert(type(game.commands) == "table", "game.commands must exist")
    assert(type(game.commands.validators) == "table", "game.commands.validators must exist")
    assert(game.commands.validators.is_frozen() == true, "game.commands.validators must be frozen after the register phase")

    local late_ok = pcall(function()
        game.commands.validators.register("core:validator.test.after_freeze", {})
    end)
    assert(not late_ok, "registering into the live game.commands.validators after freeze must fail")
end

return M
)lua";
} // namespace

std::string RunValidatorRegistryConformance()
{
    const GV2ContentCore::FRepositoryReadHandle RepoHandle = MakeEmptyRepository();
    if (!RepoHandle.IsValid())
    {
        return "validator_registry_conformance.empty_repository_build_failed";
    }

    const std::vector<FRuntimeSource> Sources = {
        {
            "@Scripts/bootstrap/manifest.lua",
            R"lua(return {
                entry_module_id = "core:module.test.validator_registry_driver",
                modules = {
                    {
                        module_id = "core:module.runtime.stable_id",
                        source = "runtime/stable_id.lua",
                        dependencies = {},
                    },
                    {
                        module_id = "core:module.runtime.validator_registry",
                        source = "runtime/validator_registry.lua",
                        dependencies = { "core:module.runtime.stable_id" },
                    },
                    {
                        module_id = "core:module.test.validator_registry_driver",
                        source = "test/validator_registry_driver.lua",
                        dependencies = { "core:module.runtime.validator_registry" },
                    },
                },
            })lua"
        },
        {"@Scripts/runtime/stable_id.lua", StableIdScriptSource},
        {"@Scripts/runtime/validator_registry.lua", ValidatorRegistryScriptSource},
        {"@Scripts/test/validator_registry_driver.lua", DriverScriptSource},
    };

    FRuntimeSession Session;
    FRuntimeFault Fault;
    if (!Session.Start(1, RepoHandle, Sources, Fault))
    {
        return "validator_registry_conformance.session_start_failed: " + Fault.Code + ": " + Fault.Message;
    }
    if (!Session.IsStarted())
    {
        Session.Stop();
        return "validator_registry_conformance.session_not_started";
    }

    Session.Stop();
    return "";
}
} // namespace GV2RuntimeCore::Testing
