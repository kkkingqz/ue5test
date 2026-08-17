-- Event Bus Core (GEW-07, GEW-08, GEW-09, GEW-10, GEW-11)
-- Manages gameplay events lifecycle: post-commit delivery, context isolation,
-- subscriber priority ordering, deterministic breadth-first queue pumping,
-- runtime phases, pump iteration limits, and subscriber registration
-- (CommandsAndEvents.md "Gameplay EventBus").

local event_envelope = require("core:module.runtime.event_envelope")
local stable_id = require("core:module.runtime.stable_id")
local subscriber_registry = require("core:module.runtime.subscriber_registry")

local M = {
    id = "core:module.runtime.event_bus",
}

local DEFAULT_PUMP_LIMIT = 1000
local pump_limit = DEFAULT_PUMP_LIMIT

local current_context = nil
local is_pumping = false
local published_events = {}
local event_queue = {}
local subscribers_by_event = {}
local registration_seq = 0

-- Review fix: the only holder of subscriber_registry's `admin.clear()`
-- capability (see subscriber_registry.lua) — never assigned onto `game`,
-- so nothing outside this module can reach it.
local subscriber_registry_admin = nil

function M.get_pump_limit()
    return pump_limit
end

function M.set_pump_limit(limit)
    if type(limit) ~= "number" or math.type(limit) ~= "integer" or limit < 1 then
        error("InvalidPumpLimit: limit must be a positive integer", 2)
    end
    pump_limit = limit
end

function M.reset_pump_limit()
    pump_limit = DEFAULT_PUMP_LIMIT
end

function M.has_active_context()
    return current_context ~= nil or is_pumping
end

function M.is_pumping()
    return is_pumping
end

-- SAV-09: lets a safe-point check (core:module.runtime.save) confirm the
-- event queue is drained without depending on is_pumping alone.
function M.get_queue_length()
    return #event_queue
end

function M.get_current_context()
    return current_context
end

function M.begin_command_context(ctx_info)
    if current_context ~= nil then
        error("EventBusReentrantContext: nested command context is not supported", 2)
    end
    assert(type(ctx_info) == "table", "command context info must be a table")
    assert(type(ctx_info.command_id) == "string", "command_id is required for command context")

    current_context = {
        command_id = ctx_info.command_id,
        sequence = ctx_info.sequence or 0,
        correlation_id = ctx_info.correlation_id,
        pending_events = {},
    }
end

function M.subscribe(arg1, arg2, arg3, arg4)
    if game and game.events and game.events.subscribers and stable_id.is_kind(arg1, "subscriber") then
        return game.events.subscribers.register(arg1, arg2, arg3, arg4)
    end

    local event_id = arg1
    local handler = arg2
    local options = arg3

    if type(event_id) ~= "string" or not stable_id.is_kind(event_id, "event") then
        error("InvalidSubscriber: event_id must be a canonical Stable ID of kind 'event', got '" .. tostring(event_id) .. "'", 2)
    end
    if type(handler) ~= "function" and not (type(handler) == "table" and type(handler.handle_event) == "function") then
        error("InvalidSubscriber: handler must be a function or table with handle_event method", 2)
    end

    local handler_fn = type(handler) == "function" and handler or function(env) return handler.handle_event(env) end

    local priority = 0
    if options ~= nil then
        if type(options) ~= "table" then
            error("InvalidSubscriber: options must be a table if provided", 2)
        end
        if options.priority ~= nil then
            if type(options.priority) ~= "number" or math.type(options.priority) ~= "integer" then
                error("InvalidSubscriber: options.priority must be an integer", 2)
            end
            priority = options.priority
        end
    end

    registration_seq = registration_seq + 1
    if not subscribers_by_event[event_id] then
        subscribers_by_event[event_id] = {}
    end

    local entry = {
        event_id = event_id,
        handler = handler_fn,
        priority = priority,
        order = registration_seq,
    }
    table.insert(subscribers_by_event[event_id], entry)
    return entry
end

function M.get_subscribers(event_id)
    local all_subs = {}

    if game and game.events and game.events.subscribers then
        local registered = game.events.subscribers.get_subscribers_for_event(event_id)
        for _, entry in ipairs(registered) do
            table.insert(all_subs, entry)
        end
    end

    local list = subscribers_by_event[event_id]
    if list then
        for _, entry in ipairs(list) do
            table.insert(all_subs, entry)
        end
    end

    if #all_subs == 0 then
        return {}
    end

    table.sort(all_subs, function(a, b)
        local pa = a.priority or 0
        local pb = b.priority or 0
        if pa ~= pb then
            return pa < pb
        end
        local oa = a.sequence or a.order or 0
        local ob = b.sequence or b.order or 0
        return oa < ob
    end)

    return all_subs
end

-- Isolated subscriber scope for Tests/Lua spec cases.
--
-- A spec that needs its own subscribers has to get past the freeze, and the
-- only way to do that is to unfreeze the registry — which also drops the
-- subscribers packages registered during the "register" phase. Left cleared,
-- that breaks whichever spec runs next, and the failure is reported there
-- rather than at its cause.
--
-- So clearing is only available inside a scope that always puts the previous
-- set back, frozen flag included. There is deliberately no operation that
-- clears without restoring.
--
-- Deliberately not exposed on game.events (see M.register below): a gameplay
-- module or mod would have to explicitly require this module and call it by
-- name — an auditable surface declared in the manifest.
function M.with_isolated_subscribers(body)
    assert(type(body) == "function", "with_isolated_subscribers expects a function")

    local previous_by_event = subscribers_by_event
    local previous_seq = registration_seq
    local previous_registry = subscriber_registry_admin and subscriber_registry_admin.snapshot() or nil

    subscribers_by_event = {}
    registration_seq = 0
    if subscriber_registry_admin then
        subscriber_registry_admin.clear()
    end

    local ok, result = pcall(body)

    subscribers_by_event = previous_by_event
    registration_seq = previous_seq
    if previous_registry ~= nil then
        subscriber_registry_admin.restore(previous_registry)
    end

    if not ok then
        error(result, 0)
    end
    return result
end

function M.enqueue(spec)
    if current_context == nil and not is_pumping then
        error("EventEnqueueOutsideCommandContext: cannot enqueue event outside of active command execution or event pump context", 2)
    end

    local env
    if event_envelope.is_envelope(spec) then
        env = spec
    else
        if type(spec) ~= "table" then
            error("InvalidEventEnvelope: spec must be a table", 2)
        end

        local prepared_spec = {
            event_id = spec.event_id,
            schema_version = spec.schema_version,
            payload = spec.payload,
            correlation_id = spec.correlation_id or (current_context and current_context.correlation_id),
            causation_id = spec.causation_id or (current_context and current_context.command_id),
            sequence = spec.sequence or (current_context and current_context.sequence),
            timestamp = spec.timestamp,
            source = spec.source or (current_context and {
                kind = "command",
                command_id = current_context.command_id,
                sequence = current_context.sequence,
            }),
        }

        env = event_envelope.create(prepared_spec)
    end

    if current_context ~= nil then
        table.insert(current_context.pending_events, env)
    else
        table.insert(event_queue, env)
    end

    return env
end

M.emit = M.enqueue

function M.commit_command_context()
    if current_context == nil then
        return 0
    end

    local pending = current_context.pending_events
    current_context = nil

    for _, env in ipairs(pending) do
        table.insert(event_queue, env)
    end

    -- Process queue FIFO / breadth-first
    local count = 0
    if not is_pumping then
        is_pumping = true
        if game and game.runtime then
            game.runtime.phase = "pumping_events"
        end

        local ok, err = pcall(function()
            local iterations = 0
            while #event_queue > 0 do
                iterations = iterations + 1
                if iterations > pump_limit then
                    if game and game.runtime then
                        game.runtime.phase = "failed"
                    end
                    error("EventPumpLimitExceeded: event pump limit of " .. tostring(pump_limit) .. " exceeded", 0)
                end

                local env = table.remove(event_queue, 1)
                table.insert(published_events, env)
                count = count + 1

                local subs = M.get_subscribers(env.event_id)
                for _, sub in ipairs(subs) do
                    sub.handler(env)
                end
            end
        end)
        is_pumping = false

        if not ok then
            event_queue = {}
            if game and game.runtime then
                game.runtime.phase = "failed"
            end
            error(err, 0)
        end
    end

    return count
end

function M.rollback_command_context()
    if current_context ~= nil then
        current_context.pending_events = {}
        current_context = nil
    end
end

function M.discard_command_context()
    if current_context ~= nil then
        current_context.pending_events = {}
        current_context = nil
    end
    event_queue = {}
    is_pumping = false
end

function M.get_published_events()
    local copy = {}
    for i, env in ipairs(published_events) do
        copy[i] = env
    end
    return copy
end

function M.clear_published_events()
    published_events = {}
end

function M.freeze()
    if game and game.events and game.events.subscribers then
        game.events.subscribers.freeze()
    end
end

function M.register(_ctx)
    if not game then
        game = {}
    end
    if not game.runtime then
        game.runtime = {}
    end
    if not game.runtime.phase then
        game.runtime.phase = "idle"
    end
    if not game.events then
        game.events = {}
    end

    local registry, admin = subscriber_registry.create_registry()
    subscriber_registry_admin = admin
    game.events.subscribers = registry
    game.events.enqueue = M.enqueue
    game.events.emit = M.emit
    game.events.subscribe = registry.register
    game.events.freeze = M.freeze
    game.events.get_published_events = M.get_published_events
    game.events.clear_published_events = M.clear_published_events
    -- Review fix: clear_subscribers is deliberately NOT exposed on
    -- game.events (production facade) — it can unfreeze the subscriber
    -- registry, violating "Registries freeze после registration"
    -- (BootstrapAndSessionLifecycle.md) if reachable by any gameplay
    -- module/mod. Tests/Lua specs call event_bus.clear_subscribers()
    -- directly (they already require() the module) — see M.clear_subscribers above.
    game.events.set_pump_limit = M.set_pump_limit
    game.events.get_pump_limit = M.get_pump_limit
    game.events.reset_pump_limit = M.reset_pump_limit
    game.events.get_queue_length = M.get_queue_length
    game.events.is_envelope = event_envelope.is_envelope
end

return M
