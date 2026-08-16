-- Event Subscriber Registry (GEW-10)
-- Registers event subscribers during the "register" lifecycle phase and freezes
-- them together with the other registries (CommandsAndEvents.md "Gameplay EventBus").

local stable_id = require("core:module.runtime.stable_id")

local M = {
    id = "core:module.runtime.subscriber_registry",
}

function M.create_registry()
    local entries = {}
    local entries_by_id = {}
    local subscribers_by_event = {}
    local is_frozen = false
    local next_sequence = 1

    local registry = {}

    function registry.register(id, event_id, handler, options)
        if is_frozen then
            error("SubscriberRegistryFrozen: cannot register subscriber '" .. tostring(id) .. "' after register phase / freeze", 2)
        end
        if type(id) ~= "string" or not stable_id.is_kind(id, "subscriber") then
            error("InvalidSubscriberId: subscriber id must be a canonical Stable ID of kind 'subscriber', got '" .. tostring(id) .. "'", 2)
        end
        if type(event_id) ~= "string" or not stable_id.is_kind(event_id, "event") then
            error("InvalidEventId: event_id must be a canonical Stable ID of kind 'event', got '" .. tostring(event_id) .. "'", 2)
        end
        if entries_by_id[id] ~= nil then
            error("SubscriberDuplicateRegistration: subscriber '" .. id .. "' is already registered", 2)
        end

        local handler_fn = nil
        if type(handler) == "function" then
            handler_fn = handler
        elseif type(handler) == "table" and type(handler.handle_event) == "function" then
            handler_fn = function(env) return handler.handle_event(env) end
        elseif type(handler) == "table" and type(handler.on_event) == "function" then
            handler_fn = function(env) return handler.on_event(env) end
        else
            error("InvalidSubscriberHandler: handler must be a function or a table with handle_event/on_event method", 2)
        end

        local priority = 0
        if options ~= nil then
            if type(options) ~= "table" then
                error("InvalidSubscriberOptions: options must be a table", 2)
            end
            if options.priority ~= nil then
                if type(options.priority) ~= "number" or math.type(options.priority) ~= "integer" then
                    error("InvalidSubscriberPriority: priority must be an integer", 2)
                end
                priority = options.priority
            end
        end

        local entry = {
            id = id,
            event_id = event_id,
            handler = handler_fn,
            raw_handler = handler,
            priority = priority,
            sequence = next_sequence,
        }
        next_sequence = next_sequence + 1
        entries_by_id[id] = entry
        table.insert(entries, entry)

        if not subscribers_by_event[event_id] then
            subscribers_by_event[event_id] = {}
        end
        table.insert(subscribers_by_event[event_id], entry)

        return handler
    end

    function registry.get(id)
        local entry = entries_by_id[id]
        return entry and (entry.raw_handler or entry.handler) or nil
    end

    function registry.exists(id)
        return type(id) == "string" and entries_by_id[id] ~= nil
    end

    function registry.freeze()
        if is_frozen then
            return
        end
        for _, list in pairs(subscribers_by_event) do
            table.sort(list, function(a, b)
                if a.priority ~= b.priority then
                    return a.priority < b.priority
                end
                return a.sequence < b.sequence
            end)
        end
        is_frozen = true
    end

    function registry.is_frozen()
        return is_frozen
    end

    function registry.get_subscribers_for_event(event_id)
        local list = subscribers_by_event[event_id]
        if not list or #list == 0 then
            return {}
        end

        local result = {}
        for i, entry in ipairs(list) do
            result[i] = entry
        end

        if not is_frozen then
            table.sort(result, function(a, b)
                if a.priority ~= b.priority then
                    return a.priority < b.priority
                end
                return a.sequence < b.sequence
            end)
        end

        return result
    end

    setmetatable(registry, {
        __newindex = function(_t, _k, _v)
            error("SubscriberRegistryDirectAssignmentDisallowed: use game.events.subscribers.register(id, event_id, handler, options) to register a subscriber", 2)
        end,
    })

    -- Review fix (BootstrapAndSessionLifecycle "Registries freeze после
    -- registration", LuaRuntimeContract "Late registration после registry
    -- freeze запрещена"): `clear()` — the one operation that can unfreeze
    -- a registry — is deliberately NOT a method on `registry` above, so it
    -- is not reachable through `game.events.subscribers` at all, by any
    -- gameplay module or mod. It is returned separately as `admin`, held
    -- only by event_bus.lua (a closure-captured module-local upvalue,
    -- never assigned onto `game`), which is the sole caller
    -- (`event_bus.clear_subscribers()`, used by Tests/Lua specs as
    -- setup/teardown between cases — never gameplay code).
    local admin = {
        clear = function()
            entries = {}
            entries_by_id = {}
            subscribers_by_event = {}
            is_frozen = false
            next_sequence = 1
        end,
    }

    return registry, admin
end

return M
