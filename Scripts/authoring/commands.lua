-- DLA-05, DLA-06, DLA-09: Designer Commands Proxy & Descriptors (ADR-0027, Commands.md)
-- Provides the designer-facing `commands` proxy table, command descriptor object with
-- `:run()` and `:later()` methods, and the `action()` semantic action constructor.
-- Enforces:
--   - __newindex accepts only functions, disallows duplicates, disallows declaration after freeze.
--   - __index on undeclared keys after freeze/runtime throws UnknownCommandKey (never returns nil).
--   - :run() disallows nested synchronous calls from within active command handlers.
--   - :later() canonicalizes args and enqueues to the deferred command queue.

local stable_id = require("core:module.runtime.stable_id")
local mutation_window = require("core:module.runtime.mutation_window")
local tagged_ref = require("core:module.authoring.tagged_ref")

local M = {
    id = "core:module.authoring.commands",
}

local function canonicalize_command_id(package_id, key)
    if stable_id.is_kind(key, "command") then
        return key
    end

    if type(key) ~= "string" or key == "" then
        error("InvalidCommandKey: expected non-empty string key, got " .. tostring(key), 3)
    end

    local canonical = package_id .. ":command." .. key
    if not stable_id.is_valid(canonical) then
        error("InvalidCommandKey: cannot construct valid command Stable ID from key '" .. key .. "' (constructed '" .. canonical .. "')", 3)
    end

    return canonical
end

local CommandDescriptorMethods = {}

function CommandDescriptorMethods:run(...)
    if mutation_window.is_open() then
        error("AuthoringNestedRunDisallowed: cannot call :run() synchronously from inside an active command handler. Use :later() instead.", 2)
    end

    local canonical_args = tagged_ref.canonicalize_args(...)
    local seq = game.runtime.dispatch_command({
        command_id = self.command_id,
        args = canonical_args,
    })

    return game.runtime.last_command_result, seq
end

function CommandDescriptorMethods:later(...)
    local ok_ctx, ctx_mod = pcall(require, "core:module.authoring.context")
    if ok_ctx and ctx_mod and ctx_mod.guard_validator_side_effect then
        ctx_mod.guard_validator_side_effect("commands.*:later")
    end

    local canonical_args = tagged_ref.canonicalize_args(...)
    return game.commands.enqueue({
        command_id = self.command_id,
        args = canonical_args,
    })
end

local function create_command_descriptor(package_id, key, command_id)
    local desc = {
        __is_command_descriptor = true,
        package_id = package_id,
        key = key,
        command_id = command_id,
    }
    return setmetatable(desc, {
        __index = CommandDescriptorMethods,
        __tostring = function(self)
            return "CommandDescriptor(" .. self.command_id .. ")"
        end,
    })
end

function M.action(cmd_desc, ...)
    local command_id
    if type(cmd_desc) == "table" and cmd_desc.command_id then
        command_id = cmd_desc.command_id
    elseif type(cmd_desc) == "string" then
        command_id = cmd_desc
    else
        error("InvalidActionCommand: expected CommandDescriptor or command_id string, got " .. type(cmd_desc), 2)
    end

    local canonical_args = tagged_ref.canonicalize_args(...)
    return {
        command_id = command_id,
        args = canonical_args,
    }
end

function M.create_commands_proxy(package_id)
    local state = {
        package_id = package_id,
        declared_handlers = {}, -- key -> { key, command_id, handler }
        declared_order = {},    -- array of keys
        descriptors = {},       -- key -> CommandDescriptor
        frozen = false,
    }

    local proxy = {}
    local mt = {
        __newindex = function(_, key, fn)
            if state.frozen then
                error("CommandDeclarationAfterFreeze: cannot declare command '" .. tostring(key) .. "' after register phase", 2)
            end

            if type(fn) ~= "function" then
                error("InvalidCommandHandler: expected function for command '" .. tostring(key) .. "', got " .. type(fn), 2)
            end

            local command_id = canonicalize_command_id(package_id, key)

            -- Ключуем по каноническому ID, а не по ключу: короткая форма и полная
            -- дают один и тот же command_id, и проверка по ключу их не различала бы.
            for existing_key, declared in pairs(state.declared_handlers) do
                if declared.command_id == command_id then
                    local via = (existing_key == key) and "" or (" (declared as '" .. tostring(existing_key) .. "')")
                    error("CommandAlreadyDefined: command '" .. command_id .. "' is already declared in this module" .. via, 2)
                end
            end
            state.declared_handlers[key] = {
                key = key,
                command_id = command_id,
                handler = fn,
            }
            table.insert(state.declared_order, key)

            if not state.descriptors[key] then
                state.descriptors[key] = create_command_descriptor(package_id, key, command_id)
            end
        end,

        __index = function(_, key)
            if type(key) ~= "string" then
                error("InvalidCommandKey: command key must be string, got " .. type(key), 2)
            end

            if state.descriptors[key] then
                return state.descriptors[key]
            end

            if state.frozen then
                -- At runtime after freeze: unknown key throws error, never nil!
                error("UnknownCommandKey: command '" .. tostring(key) .. "' is not defined in this module", 2)
            end

            -- During module load (before freeze): create stable descriptor
            local command_id = canonicalize_command_id(package_id, key)
            local desc = create_command_descriptor(package_id, key, command_id)
            state.descriptors[key] = desc
            return desc
        end,
    }

    local proxy_table = setmetatable(proxy, mt)

    local function get_declarations()
        return state.declared_handlers, state.declared_order
    end

    local function freeze()
        state.frozen = true
    end

    local function is_frozen()
        return state.frozen
    end

    return proxy_table, {
        get_declarations = get_declarations,
        freeze = freeze,
        is_frozen = is_frozen,
    }
end

return M
