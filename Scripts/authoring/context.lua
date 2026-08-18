-- DLA-05, DLA-06, DLA-08: Designer Authoring Module Context (ADR-0027)
-- Provides the `gameplay(package_id)` factory that produces module descriptors `M`:
--   - M.commands: proxy table collecting declared command handlers during module load
--   - M.fail(key, params): typed refusal returning <package_id>:error.<key>, enforces
--     the mutation rule (throws AuthoringFailAfterMutation if write_revision increased)
--   - M.action(cmd_desc, ...): semantic action constructor
--   - M.player: dynamic accessor proxy to game.instances.actors.player()
--   - M.world: dynamic accessor proxy to game.instances.world()
--   - M.actor(name): unique instance accessor (0 -> ActorInstanceNotFound, 1 -> wrapper, 2+ -> ActorInstanceAmbiguous)
--   - M.actors(name): list accessor for multiple instances
--   - M.register(ctx): lifecycle hook registering declared commands with deterministic ordering

local stable_id = require("core:module.runtime.stable_id")
local mutation_window = require("core:module.runtime.mutation_window")
local portable_value = require("core:module.runtime.portable_value")
local tagged_ref = require("core:module.authoring.tagged_ref")
local commands_module = require("core:module.authoring.commands")
local properties_module = require("core:module.authoring.properties")
local presentation_module = require("core:module.authoring.presentation")

local M = {
    id = "core:module.authoring.context",
}

local active_command_context = nil
local FAIL_SENTINEL = setmetatable({}, { __tostring = function() return "AuthoringFailRefusal" end })

local function is_fail_result(val)
    return type(val) == "table" and val.__gv2_fail == FAIL_SENTINEL
end

local function canonicalize_error_id(package_id, key)
    if stable_id.is_kind(key, "error") then
        return key
    end

    if type(key) ~= "string" or key == "" then
        error("InvalidErrorKey: expected non-empty string error key, got " .. tostring(key), 3)
    end

    local canonical = package_id .. ":error." .. key
    if not stable_id.is_valid(canonical) then
        error("InvalidErrorKey: cannot construct valid error Stable ID from key '" .. key .. "' (constructed '" .. canonical .. "')", 3)
    end

    return canonical
end

local function canonicalize_actor_def_id(package_id, name)
    if stable_id.is_kind(name, "actor") then
        return name
    end

    if type(name) ~= "string" or name == "" then
        error("InvalidActorName: expected non-empty string actor name, got " .. tostring(name), 3)
    end

    local canonical = package_id .. ":actor." .. name
    if not stable_id.is_valid(canonical) then
        error("InvalidActorName: cannot construct valid actor definition Stable ID from name '" .. name .. "' (constructed '" .. canonical .. "')", 3)
    end

    return canonical
end

local function canonicalize_event_id(package_id, name)
    if stable_id.is_kind(name, "event") then
        return name
    end

    if type(name) ~= "string" or name == "" then
        error("InvalidEventName: expected non-empty string event name, got " .. tostring(name), 3)
    end

    local canonical = package_id .. ":event." .. name
    if not stable_id.is_valid(canonical) then
        error("InvalidEventName: cannot construct valid event Stable ID from name '" .. name .. "' (constructed '" .. canonical .. "')", 3)
    end

    return canonical
end

function M.create_player_proxy()
    return setmetatable({}, {
        __index = function(_, key)
            if not (game and game.instances and game.instances.actors and game.instances.actors.player) then
                error("PlayerActorNotFound: actors registry is not available", 2)
            end
            local player = game.instances.actors.player()
            if not player then
                error("PlayerActorNotFound: player actor does not exist in state.actors", 2)
            end
            return player[key]
        end,
        __newindex = function(_, key, val)
            if not (game and game.instances and game.instances.actors and game.instances.actors.player) then
                error("PlayerActorNotFound: actors registry is not available", 2)
            end
            local player = game.instances.actors.player()
            if not player then
                error("PlayerActorNotFound: player actor does not exist in state.actors", 2)
            end
            player[key] = val
        end,
    })
end

function M.create_world_proxy()
    return setmetatable({}, {
        __index = function(_, key)
            if not (game and game.instances and game.instances.world) then
                error("WorldInstanceNotFound: world registry is not available", 2)
            end
            local w = game.instances.world()
            if not w then
                error("WorldInstanceNotFound: game.instances.world() returned nil", 2)
            end
            return w[key]
        end,
        __newindex = function(_, key, val)
            if not (game and game.instances and game.instances.world) then
                error("WorldInstanceNotFound: world registry is not available", 2)
            end
            local w = game.instances.world()
            if not w then
                error("WorldInstanceNotFound: game.instances.world() returned nil", 2)
            end
            w[key] = val
        end,
    })
end

function M.gameplay(package_id)
    if type(package_id) ~= "string" or package_id == "" then
        error("InvalidPackageId: package_id must be non-empty string, got " .. tostring(package_id), 2)
    end

    local commands_proxy, proxy_ctrl = commands_module.create_commands_proxy(package_id)
    local subscribers = {}

    local mod = {}

    mod.commands = commands_proxy
    mod.action = presentation_module.create_action_helper(package_id)
    mod.button = presentation_module.create_button_helper(package_id)
    mod.text = presentation_module.create_text_helper(package_id)
    mod.show_screen = presentation_module.create_show_screen_helper(package_id)
    mod.player = M.create_player_proxy()
    mod.world = M.create_world_proxy()

    local def_proxy = setmetatable({}, {
        __index = function(_, kind)
            return function(name)
                local full_id
                if stable_id.is_valid(name) then
                    full_id = name
                else
                    full_id = package_id .. ":" .. kind .. "." .. name
                end
                return properties_module.wrap_definition(full_id)
            end
        end,
    })
    mod.def = def_proxy

    function mod.location(name)
        return def_proxy.location(name)
    end

    function mod.emit(event_name, payload)
        local canonical_event_id = canonicalize_event_id(package_id, event_name)
        local canonical_payload = tagged_ref.canonicalize_arg(payload or {})
        portable_value.validate(canonical_payload, "event_payload")

        if game and game.events and game.events.enqueue then
            game.events.enqueue({
                event_id = canonical_event_id,
                payload = canonical_payload,
            })
        elseif game and game.events and game.events.publish then
            game.events.publish({
                event_id = canonical_event_id,
                payload = canonical_payload,
            })
        else
            error("EventsNotAvailable: game.events is not available", 2)
        end
    end

    function mod.on(event_name, handler_fn)
        if type(handler_fn) ~= "function" then
            error("InvalidEventHandler: handler must be a function, got " .. type(handler_fn), 2)
        end
        local canonical_event_id = canonicalize_event_id(package_id, event_name)
        table.insert(subscribers, {
            event_name = event_name,
            event_id = canonical_event_id,
            handler = handler_fn,
        })
    end

    function mod.fail(key, params)
        if active_command_context == nil then
            error("AuthoringFailOutsideCommand: fail() can only be called from within an active command handler", 2)
        end

        local cur_rev = mutation_window.write_revision()
        if cur_rev > active_command_context.initial_write_revision then
            error("AuthoringFailAfterMutation: command '" .. tostring(active_command_context.command_id)
                .. "' attempted to fail() after state mutation (write_revision " .. cur_rev
                .. " > " .. active_command_context.initial_write_revision
                .. "). Move precondition checks before state mutations.", 2)
        end

        local canonical_code = canonicalize_error_id(package_id, key)
        local canonical_params = tagged_ref.canonicalize_arg(params or {}, { allow_plain_id = true })
        portable_value.validate(canonical_params, "fail_params")

        return {
            ok = false,
            error = {
                code = canonical_code,
                params = canonical_params,
            },
            __gv2_fail = FAIL_SENTINEL,
        }
    end

    function mod.actor(name)
        local canonical_def_id = canonicalize_actor_def_id(package_id, name)
        if not (game and game.instances and game.instances.actors and game.instances.actors.ids) then
            error("ActorInstanceNotFound: actors registry is not available", 2)
        end

        local all_ids = game.instances.actors.ids()
        local matched = {}
        for _, id in ipairs(all_ids) do
            local inst = game.instances.actors.get(id)
            if inst and inst.definition_id == canonical_def_id then
                table.insert(matched, inst)
            end
        end

        if #matched == 0 then
            error("ActorInstanceNotFound: no instance found for actor definition '" .. canonical_def_id .. "'", 2)
        elseif #matched == 1 then
            return matched[1]
        else
            error("ActorInstanceAmbiguous: multiple instances (" .. #matched .. ") found for actor definition '" .. canonical_def_id .. "'. Use M.actors() instead.", 2)
        end
    end

    function mod.actors(name)
        local canonical_def_id = canonicalize_actor_def_id(package_id, name)
        if not (game and game.instances and game.instances.actors and game.instances.actors.ids) then
            return {}
        end

        local all_ids = game.instances.actors.ids()
        local matched = {}
        for _, id in ipairs(all_ids) do
            local inst = game.instances.actors.get(id)
            if inst and inst.definition_id == canonical_def_id then
                table.insert(matched, inst)
            end
        end
        return matched
    end

    function mod.register(_ctx)
        local declared_handlers, declared_order = proxy_ctrl.get_declarations()

        -- Deterministic sorted registration
        local sorted_keys = {}
        for _, key in ipairs(declared_order) do
            table.insert(sorted_keys, key)
        end
        table.sort(sorted_keys, function(a, b)
            return declared_handlers[a].command_id < declared_handlers[b].command_id
        end)

        for _, key in ipairs(sorted_keys) do
            local decl = declared_handlers[key]
            local cmd_id = decl.command_id
            local raw_handler = decl.handler

            local function wrapped_handler(request)
                local initial_rev = mutation_window.write_revision()
                local prev_ctx = active_command_context
                active_command_context = {
                    command_id = cmd_id,
                    initial_write_revision = initial_rev,
                }

                local raw_args = request.args or {}
                local rehydrated = tagged_ref.rehydrate_args(raw_args)

                local is_array = (type(raw_args) == "table" and #raw_args > 0 and raw_args[1] ~= nil)
                local ok, res
                if is_array then
                    local args_list = {}
                    for i = 1, #rehydrated do
                        args_list[i] = rehydrated[i]
                    end
                    ok, res = pcall(raw_handler, table.unpack(args_list, 1, #rehydrated))
                elseif type(raw_args) == "table" and next(raw_args) == nil then
                    ok, res = pcall(raw_handler)
                else
                    ok, res = pcall(raw_handler, rehydrated)
                end

                active_command_context = prev_ctx

                if not ok then
                    error(res, 0)
                end

                -- SAS-04: Implicit Command Success & Return Normalization
                if res == nil then
                    return { ok = true }
                elseif is_fail_result(res) then
                    return {
                        ok = false,
                        error = res.error,
                    }
                elseif type(res) == "table" and res.ok == true and res.value ~= nil and res.__gv2_fail == nil then
                    return res
                else
                    return {
                        ok = true,
                        value = res,
                    }
                end
            end

            if game and game.commands and game.commands.handlers and game.commands.handlers.register then
                local exists = (game.commands.handlers.exists and game.commands.handlers.exists(cmd_id)) or false
                game.commands.handlers.register(cmd_id, wrapped_handler, { override = exists })
            end
        end

        -- Register accumulated event subscribers
        for i, sub in ipairs(subscribers) do
            local clean_name = sub.event_name:gsub("[^a-zA-Z0-9_]", "_"):lower()
            local sub_id = package_id .. ":subscriber.authoring." .. clean_name .. ".s" .. tostring(i)
            local raw_handler = sub.handler
            local function wrapped_subscriber(env)
                local raw_payload = env.payload or {}
                local rehydrated = tagged_ref.rehydrate_arg(raw_payload)
                raw_handler(rehydrated, env)
            end

            if game and game.events and game.events.subscribers and game.events.subscribers.register then
                game.events.subscribers.register(sub_id, sub.event_id, wrapped_subscriber)
            end
        end

        proxy_ctrl.freeze()
    end

    return mod
end

function M.create_authoring_environment(package_id)
    local mod = M.gameplay(package_id)
    local env = {
        commands = mod.commands,
        player = mod.player,
        world = mod.world,
        def = mod.def,
        location = mod.location,
        actor = mod.actor,
        actors = mod.actors,
        fail = mod.fail,
        emit = mod.emit,
        on = mod.on,
        text = mod.text,
        button = mod.button,
        action = mod.action,
        show_screen = mod.show_screen,
    }

    setmetatable(env, {
        __index = _G,
        __newindex = function(_, key, _)
            error("AuthoringGlobalWriteDisallowed: cannot assign global variable '" .. tostring(key) .. "' in authoring module", 2)
        end,
    })

    return mod, env
end

return M
