-- Registry Lifecycle Module (CFC-05)
-- Owns the closed descriptor of all engine registries and provides:
-- 1. install(): creates all engine registries and places them in read-only facade slots before module register hooks.
-- 2. seal(): validates, resolves, and freezes all registries in strict contract order, requiring is_frozen() == true.

local service_registry = require("core:module.runtime.service_registry")
local action_registry = require("core:module.runtime.action_registry")
local entity_extension_registry = require("core:module.runtime.entity_extension_registry")
local validator_registry = require("core:module.runtime.validator_registry")
local handler_registry = require("core:module.runtime.handler_registry")
local subscriber_registry = require("core:module.runtime.subscriber_registry")
local event_bus = require("core:module.runtime.event_bus")
local actor_registry = require("core:module.runtime.actor_registry")
local instance_registry = require("core:module.runtime.instance_registry")
local presentation_source = require("core:module.runtime.presentation_source")
local authoring_context = require("core:module.authoring.context")
local authoring_properties = require("core:module.authoring.properties")
local state_validator = require("core:module.runtime.state_validator")
local command_dispatcher = require("core:module.runtime.command_dispatcher")
local event_envelope = require("core:module.runtime.event_envelope")
local world = require("core:module.runtime.world")
local random = require("core:module.runtime.random")

local M = {
    id = "core:module.bootstrap.registry_lifecycle",
}

local is_sealed = false

local function resolve_facade_path(path)
    if not _G.game or type(_G.game) ~= "table" then
        return nil
    end
    local cur = _G.game
    for _, seg in ipairs(path) do
        if type(cur) ~= "table" then
            return nil
        end
        cur = cur[seg]
    end
    return cur
end

-- Contract order per Docs/Architecture/RuntimeFacadeAndRegistries.md#host-side-freeze-sequence:
-- 1. Authoring adapters (register declarations, verify targets)
-- 2. services, actions, entity_extensions
-- 3. commands.validators, commands.handlers
-- 4. events.subscribers, events (EventBus registration surface)
-- 5. instances.actors, instances
-- 6. presentation source registry
-- 7. state_validator (reference-field/schema registries canonical state)
local DESCRIPTOR = {
    {
        facade_path = "authoring",
        get_target = function()
            return {
                context = authoring_context,
                properties = authoring_properties,
            }
        end,
        resolve = function(target)
            if target.context and type(target.context.verify_validator_targets) == "function" then
                target.context.verify_validator_targets()
            end
            if target.context and type(target.context.verify_service_targets) == "function" then
                target.context.verify_service_targets()
            end
        end,
        seal = function(target)
            if target.context and type(target.context.freeze) == "function" then
                target.context.freeze()
            end
            if target.properties and type(target.properties.freeze) == "function" then
                target.properties.freeze()
            end
        end,
        is_frozen = function(target)
            local ctx_frozen = target.context and type(target.context.is_frozen) == "function" and target.context.is_frozen()
            local prop_frozen = target.properties and type(target.properties.is_frozen) == "function" and target.properties.is_frozen()
            return (ctx_frozen == true) and (prop_frozen == true)
        end,
    },
    {
        facade_path = "services",
        path = { "services" },
        factory = function()
            return service_registry.create_registry()
        end,
        seal = function(reg)
            return reg.freeze()
        end,
        is_frozen = function(reg)
            return reg.is_frozen()
        end,
    },
    {
        facade_path = "actions",
        path = { "actions" },
        factory = function()
            return action_registry.create_registry()
        end,
        seal = function(reg)
            return reg.freeze()
        end,
        is_frozen = function(reg)
            return reg.is_frozen()
        end,
    },
    {
        facade_path = "entity_extensions",
        path = { "entity_extensions" },
        factory = function()
            return entity_extension_registry.create_registry()
        end,
        seal = function(reg)
            return reg.freeze()
        end,
        is_frozen = function(reg)
            return reg.is_frozen()
        end,
    },
    {
        facade_path = "commands.validators",
        path = { "commands", "validators" },
        factory = function()
            return validator_registry.create_registry()
        end,
        seal = function(reg)
            return reg.freeze()
        end,
        is_frozen = function(reg)
            return reg.is_frozen()
        end,
    },
    {
        facade_path = "commands.handlers",
        path = { "commands", "handlers" },
        factory = function()
            return handler_registry.create_registry()
        end,
        seal = function(reg)
            return reg.freeze()
        end,
        is_frozen = function(reg)
            return reg.is_frozen()
        end,
    },
    {
        facade_path = "events.subscribers",
        path = { "events", "subscribers" },
        seal = function(reg)
            return reg.freeze()
        end,
        is_frozen = function(reg)
            return reg.is_frozen()
        end,
    },
    {
        facade_path = "events",
        path = { "events" },
        seal = function(reg)
            return reg.freeze()
        end,
        is_frozen = function(reg)
            return reg.is_frozen()
        end,
    },
    {
        facade_path = "instances.actors",
        path = { "instances", "actors" },
        factory = function()
            return actor_registry.create_registry()
        end,
        seal = function(reg)
            return reg.freeze()
        end,
        is_frozen = function(reg)
            return reg.is_frozen()
        end,
    },
    {
        facade_path = "instances",
        path = { "instances" },
        seal = function(reg)
            return reg.freeze()
        end,
        is_frozen = function(reg)
            return reg.is_frozen()
        end,
    },
    {
        facade_path = "presentation",
        path = { "presentation" },
        factory = function()
            return presentation_source.create_registry()
        end,
        seal = function(reg)
            return reg.freeze()
        end,
        is_frozen = function(reg)
            return reg.is_frozen()
        end,
    },
    {
        facade_path = "state_validator",
        get_target = function()
            return state_validator
        end,
        seal = function(target)
            if type(target.freeze_reference_fields) == "function" then
                target.freeze_reference_fields()
            end
            if type(target.freeze) == "function" then
                target.freeze()
            end
        end,
        is_frozen = function(target)
            if type(target.is_frozen) == "function" then
                return target.is_frozen()
            end
            return false
        end,
    },
}

M.descriptor = DESCRIPTOR

local function protect_facade_table(tbl, table_name, allowed_subtables)
    if getmetatable(tbl) ~= nil then
        return tbl
    end
    local storage = {}
    for k, v in pairs(tbl) do
        storage[k] = v
        tbl[k] = nil
    end

    local mt = {
        __index = storage,
        __newindex = function(t, k, v)
            if table_name == "game" then
                if k == "state" or k == "runtime" then
                    storage[k] = v
                    return
                end
                if allowed_subtables and allowed_subtables[k] then
                    error("FacadeSlotAssignmentDisallowed: cannot overwrite registry slot '" .. tostring(k) .. "' on game facade", 2)
                end
                error("FacadeSlotAssignmentDisallowed: cannot add ad hoc slot '" .. tostring(k) .. "' to game facade", 2)
            else
                error("FacadeSlotAssignmentDisallowed: cannot assign or overwrite slot '" .. tostring(k) .. "' on " .. table_name, 2)
            end
        end,
        __pairs = function()
            return next, storage, nil
        end,
        __metatable = false,
    }
    setmetatable(tbl, mt)
    return tbl
end

function M.install()
    if not _G.game then
        _G.game = {}
    end

    local game_ref = _G.game

    local runtime = game_ref.runtime
    if not runtime then
        runtime = {}
        rawset(game_ref, "runtime", runtime)
    end
    if not runtime.phase then
        rawset(runtime, "phase", "idle")
    end

    -- 1. Services
    if not game_ref.services then
        rawset(game_ref, "services", service_registry.create_registry())
    end

    -- 2. Actions
    if not game_ref.actions then
        rawset(game_ref, "actions", action_registry.create_registry())
    end

    -- 3. Entity extensions
    if not game_ref.entity_extensions then
        rawset(game_ref, "entity_extensions", entity_extension_registry.create_registry())
    end

    -- 4. Commands (validators, handlers)
    local commands = game_ref.commands
    if not commands then
        commands = {}
        rawset(game_ref, "commands", commands)
    end
    if not commands.validators then
        rawset(commands, "validators", validator_registry.create_registry())
    end
    if not commands.handlers then
        rawset(commands, "handlers", handler_registry.create_registry())
    end
    if not commands.enqueue then
        rawset(commands, "enqueue", command_dispatcher.enqueue)
        rawset(commands, "clear_queue", command_dispatcher.clear_queue)
        rawset(commands, "get_queue_length", command_dispatcher.get_queue_length)
        rawset(commands, "drain_queue", command_dispatcher.drain_queue)
    end
    protect_facade_table(commands, "game.commands")

    -- 5. Events (subscribers, event bus)
    local events = game_ref.events
    if not events then
        events = {}
        rawset(game_ref, "events", events)
    end
    if not events.subscribers then
        local sub_reg, admin = subscriber_registry.create_registry()
        rawset(events, "subscribers", sub_reg)
        if event_bus.set_subscriber_admin then
            event_bus.set_subscriber_admin(admin)
        end
    end
    if not events.enqueue then
        rawset(events, "enqueue", event_bus.enqueue)
        rawset(events, "emit", event_bus.emit)
        rawset(events, "subscribe", events.subscribers.register)
        rawset(events, "freeze", event_bus.freeze)
        rawset(events, "is_frozen", event_bus.is_frozen)
        rawset(events, "get_published_events", event_bus.get_published_events)
        rawset(events, "clear_published_events", event_bus.clear_published_events)
        rawset(events, "set_pump_limit", event_bus.set_pump_limit)
        rawset(events, "get_pump_limit", event_bus.get_pump_limit)
        rawset(events, "reset_pump_limit", event_bus.reset_pump_limit)
        rawset(events, "get_queue_length", event_bus.get_queue_length)
        rawset(events, "is_envelope", event_envelope.is_envelope)
    end
    protect_facade_table(events, "game.events")

    -- 6. Instances (actors, general instances, world wrapper)
    local instances = game_ref.instances
    if not instances then
        instances = {}
        rawset(game_ref, "instances", instances)
    end
    if not instances.actors then
        rawset(instances, "actors", actor_registry.create_registry())
    end
    if not instances.world then
        rawset(instances, "world", world.get_world)
    end
    if not instances.register_kind then
        local inst_reg = instance_registry.get_default_registry()
        rawset(instances, "register_kind", inst_reg.register_kind)
        rawset(instances, "is_registered_kind", inst_reg.is_registered_kind)
        rawset(instances, "get_section_name", inst_reg.get_section_name)
        rawset(instances, "create", inst_reg.create)
        rawset(instances, "freeze", inst_reg.freeze)
        rawset(instances, "is_frozen", inst_reg.is_frozen)
        rawset(instances, "kinds", inst_reg.kinds)
        rawset(instances, "clear_for_test", inst_reg.clear_for_test)
    end
    protect_facade_table(instances, "game.instances")

    -- 7. Presentation
    if not game_ref.presentation then
        rawset(game_ref, "presentation", presentation_source.create_registry())
    end

    -- 8. Random (CFC-07A)
    local random_facade = game_ref.random
    if not random_facade then
        random_facade = {
            next_u32 = random.next_u32,
            next_unit = random.next_unit,
            next_int = random.next_int,
        }
        rawset(game_ref, "random", random_facade)
    end
    protect_facade_table(random_facade, "game.random")

    -- Protect top-level game table from ad hoc slot additions and registry replacement
    local allowed_registry_slots = {
        services = true,
        actions = true,
        entity_extensions = true,
        commands = true,
        events = true,
        instances = true,
        presentation = true,
        random = true,
    }
    protect_facade_table(game_ref, "game", allowed_registry_slots)
end

function M.seal()
    for _, entry in ipairs(DESCRIPTOR) do
        local target = nil
        if entry.get_target then
            local ok_tgt, res_tgt = pcall(entry.get_target)
            if not ok_tgt or res_tgt == nil then
                return false, {
                    phase = "SealingRegistries",
                    registry_path = entry.facade_path,
                    error = "MissingParticipant: registry object for '" .. entry.facade_path .. "' is missing",
                }
            end
            target = res_tgt
        elseif entry.path then
            target = resolve_facade_path(entry.path)
            if target == nil then
                return false, {
                    phase = "SealingRegistries",
                    registry_path = entry.facade_path,
                    error = "MissingParticipant: registry object for '" .. entry.facade_path .. "' is missing",
                }
            end
        end

        if entry.resolve then
            local ok_res, err_res = pcall(entry.resolve, target)
            if not ok_res then
                return false, {
                    phase = "SealingRegistries",
                    registry_path = entry.facade_path,
                    error = "ResolveFailed: " .. tostring(err_res),
                }
            end
        end

        local seal_fn = entry.seal
        if type(seal_fn) ~= "function" then
            return false, {
                phase = "SealingRegistries",
                registry_path = entry.facade_path,
                error = "MissingSealMethod: seal operation for '" .. entry.facade_path .. "' is missing or not a function",
            }
        end

        local ok_seal, err_seal = pcall(seal_fn, target)
        if not ok_seal then
            return false, {
                phase = "SealingRegistries",
                registry_path = entry.facade_path,
                error = "SealFailed: " .. tostring(err_seal),
            }
        end

        local is_frozen_fn = entry.is_frozen
        if type(is_frozen_fn) ~= "function" then
            return false, {
                phase = "SealingRegistries",
                registry_path = entry.facade_path,
                error = "MissingIsFrozenMethod: is_frozen predicate for '" .. entry.facade_path .. "' is missing or not a function",
            }
        end

        local ok_f, is_f = pcall(is_frozen_fn, target)
        if not ok_f then
            return false, {
                phase = "SealingRegistries",
                registry_path = entry.facade_path,
                error = "IsFrozenCheckFailed: " .. tostring(is_f),
            }
        end

        if is_f ~= true then
            return false, {
                phase = "SealingRegistries",
                registry_path = entry.facade_path,
                error = "RegistryNotFrozen: registry '" .. entry.facade_path .. "' was not frozen (expected true, got " .. tostring(is_f) .. ")",
            }
        end
    end

    is_sealed = true
    return true
end

function M.is_sealed()
    return is_sealed
end

function M.clear_for_test()
    is_sealed = false
end

return M
