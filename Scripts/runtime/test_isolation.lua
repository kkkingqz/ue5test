local M = {}

local registry_lifecycle = require("core:module.bootstrap.registry_lifecycle")
local service_registry = require("core:module.runtime.service_registry")
local handler_registry = require("core:module.runtime.handler_registry")
local validator_registry = require("core:module.runtime.validator_registry")
local actor_registry = require("core:module.runtime.actor_registry")
local event_bus = require("core:module.runtime.event_bus")

-- Taken once at module load; registry_lifecycle hands the capability to no one else.
local handle = registry_lifecycle.take_isolation_handle()
if handle == nil then
    error("IsolationHandleUnavailable: core:module.runtime.test_isolation must be the sole holder of the bootstrap isolation handle", 0)
end

function M.with_isolated_services(fn)
    local fresh_registry = service_registry.create_registry()
    return registry_lifecycle.with_isolated_facade_slot(handle, game, "services", fresh_registry, fn)
end

function M.with_isolated_handlers(fn)
    local fresh_registry = handler_registry.create_registry()
    return registry_lifecycle.with_isolated_facade_slot(handle, game.commands, "handlers", fresh_registry, fn)
end

function M.with_isolated_actors(fn)
    local fresh_registry = actor_registry.create_registry()
    fresh_registry.register_type("character", function(base) return {} end)
    return registry_lifecycle.with_isolated_facade_slot(handle, game.instances, "actors", fresh_registry, fn)
end

function M.with_isolated_validators(fn)
    local authoring_context = require("core:module.authoring.context")
    local fresh_registry = validator_registry.create_registry()
    return registry_lifecycle.with_isolated_facade_slot(handle, game.commands, "validators", fresh_registry, function()
        local snap = authoring_context.snapshot_test_state and authoring_context.snapshot_test_state()
        if authoring_context.reset_test_state then
            authoring_context.reset_test_state()
        end
        local ok, err = pcall(fn)
        if authoring_context.restore_test_state then
            authoring_context.restore_test_state(snap)
        end
        if not ok then
            error(err, 0)
        end
    end)
end

function M.with_isolated_context(fn)
    local authoring_context = require("core:module.authoring.context")
    local snap = authoring_context.snapshot_test_state and authoring_context.snapshot_test_state()
    if authoring_context.reset_test_state then
        authoring_context.reset_test_state()
    end
    local ok, res = pcall(function()
        return M.with_isolated_handlers(function()
            return event_bus.with_isolated_subscribers(function()
                return M.with_isolated_services(function()
                    return M.with_isolated_validators(function()
                        local prev_phase = game and game.runtime and game.runtime.phase
                        local ok_inner, res_inner = pcall(fn)
                        if game and game.runtime and prev_phase then
                            game.runtime.phase = prev_phase
                        end
                        if not ok_inner then
                            error(res_inner, 0)
                        end
                        return res_inner
                    end)
                end)
            end)
        end)
    end)
    if authoring_context.restore_test_state then
        authoring_context.restore_test_state(snap)
    end
    if not ok then
        error(res, 0)
    end
    return res
end

return M
