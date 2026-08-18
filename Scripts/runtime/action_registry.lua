-- Canonical Semantic Action Registry (TSL-11)
-- Manages registration, late-binding, and lifecycle freezing of semantic actions to commands.

local stable_id = require("core:module.runtime.stable_id")

local M = {
    id = "core:module.runtime.action_registry",
}

function M.create_registry()
    local bindings_by_id = {}
    local is_frozen = false

    local registry = {}

    function registry.bind(action_id, command_binding)
        if is_frozen then
            error("ActionRegistryFrozen: cannot bind action '" .. tostring(action_id) .. "' after register phase / freeze", 2)
        end
        if type(action_id) ~= "string" or not stable_id.is_kind(action_id, "action") then
            error("InvalidActionId: expected Stable ID of kind 'action', got '" .. tostring(action_id) .. "'", 2)
        end
        if bindings_by_id[action_id] ~= nil then
            error("DuplicateActionBinding: action '" .. action_id .. "' is already bound", 2)
        end

        local cmd_id = nil
        local default_args = {}

        if type(command_binding) == "string" then
            cmd_id = command_binding
        elseif type(command_binding) == "table" then
            cmd_id = command_binding.command_id or command_binding.__command_id
            if command_binding.args and type(command_binding.args) == "table" then
                default_args = command_binding.args
            end
        else
            error("InvalidActionBinding: expected command ID string or binding table, got " .. type(command_binding), 2)
        end

        if type(cmd_id) ~= "string" or not stable_id.is_kind(cmd_id, "command") then
            error("InvalidActionTargetCommand: target command must be a Stable ID of kind 'command', got '" .. tostring(cmd_id) .. "'", 2)
        end

        local record = {
            command_id = cmd_id,
            args = default_args,
        }
        bindings_by_id[action_id] = record
        return record
    end

    registry.register = registry.bind

    function registry.get(action_id)
        if type(action_id) ~= "string" or action_id == "" then
            return nil
        end
        return bindings_by_id[action_id]
    end

    function registry.require(action_id)
        local binding = registry.get(action_id)
        if binding == nil then
            error("ActionNotBound: action '" .. tostring(action_id) .. "' is not bound to any command", 2)
        end
        return binding
    end

    function registry.resolve(action_id, opt_args)
        local binding = registry.require(action_id)
        local merged_args = {}
        if binding.args then
            for k, v in pairs(binding.args) do
                merged_args[k] = v
            end
        end
        if opt_args and type(opt_args) == "table" then
            for k, v in pairs(opt_args) do
                merged_args[k] = v
            end
        end
        return {
            command_id = binding.command_id,
            args = merged_args,
        }
    end

    function registry.exists(action_id)
        if type(action_id) ~= "string" or action_id == "" then
            return false
        end
        return bindings_by_id[action_id] ~= nil
    end

    function registry.list()
        local result = {}
        for id, _ in pairs(bindings_by_id) do
            table.insert(result, id)
        end
        table.sort(result)
        return result
    end

    function registry.freeze()
        is_frozen = true
    end

    function registry.is_frozen()
        return is_frozen
    end

    function registry.clear_for_test()
        bindings_by_id = {}
        is_frozen = false
    end

    function registry.with_isolated_actions(fn)
        local prev_bindings = bindings_by_id
        local prev_frozen = is_frozen
        bindings_by_id = {}
        for k, v in pairs(prev_bindings) do
            bindings_by_id[k] = v
        end
        is_frozen = false

        local ok, res_or_err = pcall(fn)
        bindings_by_id = prev_bindings
        is_frozen = prev_frozen

        if not ok then
            error(res_or_err, 0)
        end
        return res_or_err
    end

    local public_facade = {}
    local mt = {
        __index = function(_, k)
            if registry[k] ~= nil then
                return registry[k]
            end
            return bindings_by_id[k]
        end,
        __newindex = function(_, _k, _v)
            error("ActionRegistryDirectAssignmentDisallowed: use game.actions.bind(action_id, command_binding) to bind an action", 2)
        end,
        __tostring = function(_)
            return "SemanticActionRegistry"
        end,
    }
    setmetatable(public_facade, mt)
    return public_facade
end

local default_registry = nil

function M.register(_ctx)
    if not game then
        game = {}
    end
    default_registry = M.create_registry()
    game.actions = default_registry
end

return M
