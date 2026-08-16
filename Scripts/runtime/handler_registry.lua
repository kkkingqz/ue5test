-- Command Handler Registry (CHR-01)
-- Registers command handlers keyed by command_id during the "register"
-- lifecycle phase and freezes them together with the other registries.

local stable_id = require("core:module.runtime.stable_id")

local M = {
    id = "core:module.runtime.handler_registry",
}

function M.create_registry()
    local entries = {}
    local entries_by_id = {}
    local is_frozen = false
    local next_sequence = 1

    local registry = {}

    function registry.register(command_id, handler_fn, options)
        if is_frozen then
            error("CommandHandlerRegistryFrozen: cannot register command handler '" .. tostring(command_id) .. "' after register phase / freeze", 2)
        end
        if type(command_id) ~= "string" or not stable_id.is_kind(command_id, "command") then
            error("InvalidCommandId: command id must be a canonical Stable ID of kind 'command', got '" .. tostring(command_id) .. "'", 2)
        end
        if type(handler_fn) ~= "function" then
            error("InvalidCommandHandler: handler must be a function, got " .. type(handler_fn), 2)
        end

        local is_override = false
        if options ~= nil then
            if type(options) ~= "table" then
                error("InvalidCommandHandlerOptions: options must be a table", 2)
            end
            if options.override ~= nil then
                if type(options.override) ~= "boolean" then
                    error("InvalidCommandHandlerOptions: options.override must be a boolean", 2)
                end
                is_override = options.override
            end
        end

        if entries_by_id[command_id] ~= nil then
            if not is_override then
                error("CommandHandlerDuplicateRegistration: command handler for '" .. command_id .. "' is already registered", 2)
            end
            entries_by_id[command_id].handler = handler_fn
        else
            if is_override then
                error("CommandHandlerOverrideMissing: cannot override unregistered command '" .. command_id .. "'", 2)
            end
            local entry = {
                id = command_id,
                handler = handler_fn,
                sequence = next_sequence,
            }
            next_sequence = next_sequence + 1
            entries_by_id[command_id] = entry
            table.insert(entries, entry)
        end

        return handler_fn
    end

    function registry.get(command_id)
        local entry = entries_by_id[command_id]
        return entry and entry.handler or nil
    end

    function registry.exists(command_id)
        return type(command_id) == "string" and entries_by_id[command_id] ~= nil
    end

    function registry.ids()
        local result = {}
        for i, entry in ipairs(entries) do
            result[i] = entry.id
        end
        return result
    end

    function registry.freeze()
        if is_frozen then
            return
        end
        table.sort(entries, function(a, b)
            return a.sequence < b.sequence
        end)
        is_frozen = true
    end

    function registry.is_frozen()
        return is_frozen
    end

    local public_facade = {}
    local mt = {
        __index = function(_, k)
            if registry[k] ~= nil then
                return registry[k]
            end
            local entry = entries_by_id[k]
            return entry and entry.handler or nil
        end,
        __newindex = function(_, _k, _v)
            error("CommandHandlerRegistryDirectAssignmentDisallowed: use game.commands.handlers.register(id, handler, options) to register a command handler", 2)
        end,
        __tostring = function(_)
            return "GameplayCommandHandlerRegistry"
        end,
    }
    setmetatable(public_facade, mt)
    return public_facade
end

function M.register(_ctx)
    if not game then
        game = {}
    end
    if not game.commands then
        game.commands = {}
    end
    game.commands.handlers = M.create_registry()
end

return M
