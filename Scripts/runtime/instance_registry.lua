-- Generic Instance Creation and Kind Registry (ADR-0032, RAS-05..07, RAS-16)

local instance_allocator = require("core:module.runtime.instance_allocator")
local mutation_window = require("core:module.runtime.mutation_window")
local properties = require("core:module.authoring.properties")
local stable_id = require("core:module.runtime.stable_id")

local M = {
    id = "core:module.runtime.instance_registry",
}

function M.create_registry()
    local registry = {}
    local registered_kinds = {}
    local is_frozen = false

    function registry.register_kind(kind, opt_spec)
        if is_frozen then
            error("InstanceKindRegistryFrozen: cannot register instance kind '" .. tostring(kind) .. "' after register phase / freeze", 2)
        end
        if type(kind) ~= "string" or kind == "" then
            error("InvalidInstanceKind: kind must be a non-empty string", 2)
        end
        if not instance_allocator.is_valid_kind(kind) then
            error("InvalidInstanceKind: kind '" .. tostring(kind) .. "' is not a valid lowercase identifier", 2)
        end
        local spec = opt_spec or {}
        local section_name = spec.section_name or (kind .. "_instances")
        registered_kinds[kind] = {
            kind = kind,
            section_name = section_name,
            spec = spec,
        }
    end

    function registry.is_registered_kind(kind)
        if kind == "actor" then
            return true
        end
        return registered_kinds[kind] ~= nil
    end

    function registry.get_section_name(kind)
        if kind == "actor" then
            return "actors"
        end
        local info = registered_kinds[kind]
        if not info then
            error("UnknownInstanceKind: instance kind '" .. tostring(kind) .. "' is not registered", 2)
        end
        return info.section_name
    end

    function registry.kinds()
        local result = {}
        for k, _ in pairs(registered_kinds) do
            table.insert(result, k)
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

    function registry.create(kind, data)
        if type(kind) ~= "string" or kind == "" then
            error("InvalidInstanceKind: expected non-empty string kind, got " .. type(kind), 2)
        end
        if type(data) ~= "table" then
            error("InvalidInstancePayload: expected table payload for instance kind '" .. tostring(kind) .. "', got " .. type(data), 2)
        end

        if kind == "actor" or kind == "Actor" then
            if game and game.instances and game.instances.actors and game.instances.actors.create then
                return game.instances.actors.create(data)
            else
                error("ActorCreationNotAvailable: game.instances.actors.create is not available", 2)
            end
        end

        if not registry.is_registered_kind(kind) then
            error("UnknownInstanceKind: instance kind '" .. tostring(kind) .. "' is not registered", 2)
        end

        if not (game and game.state) then
            error("GameStateNotAvailable: cannot create instance without active game.state", 2)
        end

        if not mutation_window.is_open() then
            error("MutationWindowClosed: cannot mutate canonical state outside of active command handler", 2)
        end

        local section_name = registry.get_section_name(kind)
        if not game.state[section_name] then
            game.state[section_name] = {}
        end

        -- Normalize definition (RAS-06)
        local raw_def = data.definition or data.definition_id
        local def_id = nil
        if raw_def ~= nil then
            if type(raw_def) == "table" then
                def_id = raw_def.id or raw_def.definition_id
            elseif type(raw_def) == "string" then
                def_id = raw_def
                if game and game.repository and game.repository.exists then
                    if not game.repository.exists(def_id) then
                        error("InstanceDefinitionNotFound: definition '" .. tostring(def_id) .. "' does not exist in repository", 2)
                    end
                end
            end
            if type(def_id) ~= "string" or def_id == "" then
                error("InvalidDefinitionId: definition ID must be a non-empty string", 2)
            end
        end

        -- Normalize owner (RAS-06)
        local raw_owner = data.owner or data.owner_id
        local owner_id = nil
        if raw_owner ~= nil then
            if type(raw_owner) == "table" then
                owner_id = raw_owner.instance_id or raw_owner.id
            elseif type(raw_owner) == "string" then
                owner_id = raw_owner
            end
            if type(owner_id) ~= "string" or owner_id == "" then
                error("InvalidOwnerId: owner ID must be a non-empty string", 2)
            end
        end

        -- Allocate instance ID (RAS-05)
        local instance_id = instance_allocator.allocate(game.state, kind)

        local instance_record = {
            instance_id = instance_id,
        }
        if def_id ~= nil then
            instance_record.definition_id = def_id
        end
        if owner_id ~= nil then
            instance_record.owner_id = owner_id
        end

        -- Copy and canonicalize other fields
        for k, v in pairs(data) do
            if k ~= "definition" and k ~= "definition_id" and k ~= "owner" and k ~= "owner_id" and k ~= "instance_id" then
                instance_record[k] = properties.canonicalize_value(v)
            end
        end

        game.state[section_name][instance_id] = instance_record

        return instance_id
    end

    function registry.clear_for_test()
        registered_kinds = {}
        is_frozen = false
    end

    return registry
end

local default_registry = nil

M.get_default_registry = function()
    if not default_registry then
        default_registry = M.create_registry()
    end
    return default_registry
end

M.register_kind = function(kind, opt_spec)
    if game and game.instances and game.instances.register_kind then
        return game.instances.register_kind(kind, opt_spec)
    end
    return M.get_default_registry().register_kind(kind, opt_spec)
end

M.is_registered_kind = function(kind)
    if game and game.instances and game.instances.is_registered_kind then
        return game.instances.is_registered_kind(kind)
    end
    return M.get_default_registry().is_registered_kind(kind)
end

M.get_section_name = function(kind)
    if game and game.instances and game.instances.get_section_name then
        return game.instances.get_section_name(kind)
    end
    return M.get_default_registry().get_section_name(kind)
end

M.kinds = function()
    if game and game.instances and game.instances.kinds then
        return game.instances.kinds()
    end
    return M.get_default_registry().kinds()
end

M.create = function(kind, data)
    if game and game.instances and game.instances.create then
        return game.instances.create(kind, data)
    end
    return M.get_default_registry().create(kind, data)
end

M.freeze = function()
    if game and game.instances and game.instances.freeze then
        return game.instances.freeze()
    end
    return M.get_default_registry().freeze()
end

function M.register(_ctx)
    if not game.instances then
        game.instances = {}
    end
    local reg = M.get_default_registry()
    game.instances.register_kind = reg.register_kind
    game.instances.is_registered_kind = reg.is_registered_kind
    game.instances.get_section_name = reg.get_section_name
    game.instances.create = reg.create
    game.instances.freeze = reg.freeze
    game.instances.kinds = reg.kinds
    game.instances.clear_for_test = reg.clear_for_test
end

return M
