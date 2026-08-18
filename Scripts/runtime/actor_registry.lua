local instance_allocator = require("core:module.runtime.instance_allocator")
local properties = require("core:module.authoring.properties")

local M = {
    id = "core:module.runtime.actor_registry",
}

local function get_discriminator(definition_id, actor_state)
    if actor_state and type(actor_state) == "table" and type(actor_state.discriminator) == "string" then
        return actor_state.discriminator
    end
    if not definition_id or type(definition_id) ~= "string" then
        return nil
    end
    if game and game.repository and game.repository.get then
        local def = game.repository.get(definition_id)
        if def and type(def) == "table" then
            if def.data and type(def.data) == "table" and def.data.discriminator ~= nil then
                return def.data.discriminator
            elseif def.discriminator ~= nil then
                return def.discriminator
            end
        end
    end
    return nil
end

function M.create_registry()
    local registry = {}
    local type_decorators = {}
    local is_frozen = false

    function registry.register_type(discriminator, decorator)
        if is_frozen then
            error("ActorTypeRegistryFrozen: cannot register actor type after register phase", 2)
        end
        if type(discriminator) ~= "string" or discriminator == "" then
            error("InvalidActorDiscriminator: discriminator must be a non-empty string", 2)
        end
        if type_decorators[discriminator] ~= nil then
            error("ActorTypeDuplicateRegistration: actor type '" .. tostring(discriminator) .. "' is already registered", 2)
        end
        if type(decorator) ~= "function" then
            error("InvalidActorDecorator: decorator for '" .. tostring(discriminator) .. "' must be a function", 2)
        end
        type_decorators[discriminator] = decorator
    end

    function registry.types()
        local result = {}
        for disc in pairs(type_decorators) do
            table.insert(result, disc)
        end
        table.sort(result)
        return result
    end

    function registry.freeze()
        is_frozen = true
        properties.freeze()

        -- Verify that for all registered schemas, any managed field's declared operations exist on the decorator (DLA-12)
        for disc, schema in pairs(properties.schemas()) do
            if schema and schema.fields then
                for field_name, fspec in pairs(schema.fields) do
                    if fspec.write_policy == "managed" and fspec.operations then
                        local decorator = type_decorators[disc]
                        if not decorator then
                            error("MissingDomainOperation: discriminator '" .. tostring(disc) .. "' has managed field '" .. tostring(field_name) .. "' but no decorator registered", 2)
                        end
                        local dummy_base = {
                            instance_id = "test@0",
                            definition_id = "core:actor.test",
                            discriminator = disc,
                            get_state = function() return {} end,
                        }
                        local dummy_decorated = decorator(dummy_base)
                        for _, op_name in ipairs(fspec.operations) do
                            if type(dummy_decorated[op_name]) ~= "function" then
                                error("MissingDomainOperation: operation '" .. tostring(op_name) .. "' declared in schema for managed field '" .. tostring(field_name) .. "' is not defined on actor type '" .. tostring(disc) .. "'", 2)
                            end
                        end
                    end
                end
            end
        end
    end

    function registry.is_frozen()
        return is_frozen
    end

    local function wrap_actor(actor_state)
        if type(actor_state) ~= "table" then
            return nil
        end

        local discriminator = get_discriminator(actor_state.definition_id, actor_state)
        local disc_key = discriminator or "unknown"
        local decorator = type_decorators[disc_key] or (discriminator and type_decorators[discriminator])

        if not decorator then
            error("ActorTypeNotRegistered: discriminator '" .. tostring(disc_key) .. "' is not registered", 2)
        end

        local base = {}
        local base_methods = {
            get_state = function() return actor_state end,
        }

        local function read_property(k, decorated_val)
            if k == "instance_id" then
                return actor_state.instance_id
            elseif k == "definition_id" then
                return actor_state.definition_id
            elseif k == "discriminator" then
                return discriminator
            end
            if decorated_val ~= nil then
                return decorated_val
            end
            local schema = properties.get_schema(discriminator) or (actor_state.definition_id and properties.get_schema(actor_state.definition_id))
            if schema and schema.fields and schema.fields[k] then
                local fspec = schema.fields[k]
                if fspec.storage == "definition" then
                    local def_data = nil
                    if game and game.repository and game.repository.get and actor_state.definition_id then
                        local def = game.repository.get(actor_state.definition_id)
                        if def and def.data then def_data = def.data end
                    end
                    local val = def_data and def_data[k]
                    if (fspec.kind == "ref" or fspec.kind == "ref_definition") and type(val) == "string" then
                        if properties and properties.wrap_definition then
                            return properties.wrap_definition(val)
                        end
                        if game and game.repository and game.repository.get then
                            return game.repository.get(val)
                        end
                    end
                    return val
                elseif fspec.kind == "ref_instance" then
                    local raw_id = actor_state[k]
                    if type(raw_id) == "string" and raw_id ~= "" then
                        if game and game.instances and game.instances.actors and game.instances.actors.get then
                            return game.instances.actors.get(raw_id)
                        end
                    end
                    return raw_id
                elseif fspec.kind == "ref" or fspec.kind == "ref_definition" then
                    local raw_id = actor_state[k] or (k == "current_location" and actor_state.current_location_id) or (k == "current_location_id" and actor_state.current_location)
                    if k == "current_location_id" then
                        return raw_id
                    end
                    if type(raw_id) == "string" and raw_id ~= "" then
                        if properties and properties.wrap_definition then
                            return properties.wrap_definition(raw_id)
                        end
                        if game and game.repository and game.repository.get then
                            return game.repository.get(raw_id)
                        end
                    end
                    return raw_id
                elseif fspec.kind == "array" then
                    return properties.wrap_collection(actor_state, k, fspec)
                end
            end
            if k == "current_location" then
                local raw_loc = actor_state.current_location or actor_state.current_location_id
                if type(raw_loc) == "string" and raw_loc ~= "" then
                    if properties and properties.wrap_definition then
                        return properties.wrap_definition(raw_loc)
                    end
                end
                return raw_loc
            elseif k == "current_location_id" then
                local raw_loc = actor_state.current_location or actor_state.current_location_id
                return raw_loc
            end
            return actor_state[k]
        end

        local function write_property(k, v)
            if k == "discriminator" or k == "instance_id" or k == "definition_id" then
                error("ActorDiscriminatorImmutable: field '" .. tostring(k) .. "' is immutable on actor wrapper", 2)
            end
            if k == "current_location" or k == "current_location_id" then
                local _, canonical_val = properties.validate_field_value(k, { kind = "ref_definition" }, v)
                actor_state.current_location = canonical_val
                actor_state.current_location_id = canonical_val
                return
            end
            local schema = properties.get_schema(discriminator) or (actor_state.definition_id and properties.get_schema(actor_state.definition_id))
            if schema and schema.fields and schema.fields[k] then
                local fspec = schema.fields[k]
                if fspec.storage == "definition" then
                    error("Cannot modify definition field: '" .. tostring(k) .. "' is stored in definition", 2)
                elseif fspec.write_policy == "read_only" then
                    error("Cannot modify read-only field: '" .. tostring(k) .. "'", 2)
                elseif fspec.write_policy == "managed" then
                    local ops_list = fspec.operations or {}
                    local ops_str = table.concat(ops_list, ", ")
                    error("Cannot assign directly to managed field '" .. tostring(k) .. "'; use domain operation(s): " .. ops_str, 2)
                else
                    -- plain write_policy: validate before writing to state
                    local ok, canonical_val = properties.validate_field_value(k, fspec, v)
                    actor_state[k] = canonical_val
                end
            else
                actor_state[k] = v
            end
        end

        local base_mt = {
            __index = function(_, k)
                if base_methods[k] ~= nil then
                    return base_methods[k]
                end
                return read_property(k, nil)
            end,
            __newindex = function(_, k, v)
                write_property(k, v)
            end,
            __tostring = function(_)
                return string.format("ActorWrapper(%s, %s)", tostring(actor_state.instance_id), tostring(discriminator))
            end,
        }
        setmetatable(base, base_mt)

        if decorator then
            local decorated = decorator(base)
            if type(decorated) ~= "table" then
                error("ActorDecoratorInvalid: decorator for '" .. tostring(disc_key) .. "' must return a table", 2)
            end

            local final_wrapper = {}
            local final_mt = {
                __index = function(_, k)
                    return read_property(k, decorated[k])
                end,
                __newindex = function(_, k, v)
                    write_property(k, v)
                end,
                __tostring = function(_)
                    return string.format("ActorWrapper(%s, %s)", tostring(actor_state.instance_id), tostring(discriminator))
                end,
            }
            setmetatable(final_wrapper, final_mt)
            return final_wrapper
        end

        return base
    end

    registry.wrap = wrap_actor

    function registry.exists(instance_id)
        if type(instance_id) ~= "string" or instance_id == "" then
            return false
        end
        if not game or not game.state or not game.state.actors then
            return false
        end
        return game.state.actors[instance_id] ~= nil
    end

    function registry.get(instance_id)
        if type(instance_id) ~= "string" or instance_id == "" then
            return nil
        end
        if not game or not game.state or not game.state.actors then
            return nil
        end
        local actor_state = game.state.actors[instance_id]
        if not actor_state then
            return nil
        end
        return wrap_actor(actor_state)
    end

    function registry.player()
        if not game or not game.state or not game.state.meta then
            return nil
        end
        local player_id = game.state.meta.player_actor_id
        if type(player_id) ~= "string" or player_id == "" then
            return nil
        end
        return registry.get(player_id)
    end

    function registry.ids()
        local result = {}
        if not game or not game.state or not game.state.actors then
            return result
        end
        for id, _ in pairs(game.state.actors) do
            if type(id) == "string" then
                table.insert(result, id)
            end
        end
        table.sort(result)
        return result
    end

    function registry.list(filter_or_discriminator)
        local sorted_ids = registry.ids()
        local result = {}
        for _, id in ipairs(sorted_ids) do
            local actor = registry.get(id)
            if actor ~= nil then
                if filter_or_discriminator == nil then
                    table.insert(result, actor)
                elseif type(filter_or_discriminator) == "function" then
                    if filter_or_discriminator(actor) then
                        table.insert(result, actor)
                    end
                elseif type(filter_or_discriminator) == "string" then
                    local matched = false
                    if actor.discriminator == filter_or_discriminator then
                        matched = true
                    elseif game and game.repository and game.repository.get then
                        local def = game.repository.get(actor.definition_id)
                        if def and type(def) == "table" and def.discriminator == filter_or_discriminator then
                            matched = true
                        end
                    end
                    if matched then
                        table.insert(result, actor)
                    end
                end
            end
        end
        return result
    end

    function registry.find_by_discriminator(discriminator)
        assert(type(discriminator) == "string", "discriminator must be a string")
        return registry.list(discriminator)
    end

    function registry.create(definition_id, overrides)
        if type(definition_id) ~= "string" or definition_id == "" then
            error("ActorCreationError: definition_id must be a non-empty string", 2)
        end
        local ns, kind, rest = definition_id:match("^([a-z][a-z0-9_]*):([a-z][a-z0-9_]*)%.([a-z0-9_.]+)$")
        if not ns or kind ~= "actor" or not rest then
            error("ActorCreationError: invalid actor definition_id grammar '" .. definition_id .. "' (kind must be 'actor')", 2)
        end
        if game and game.repository and game.repository.exists then
            if not game.repository.exists(definition_id) then
                error("ActorDefinitionNotFound: definition_id '" .. definition_id .. "' not found in repository", 2)
            end
        end
        if not game or not game.state or not game.state.actors then
            error("ActorCreationError: game.state.actors is not available", 2)
        end

        local instance_id = instance_allocator.allocate(game.state, "actor")
        local actor_state = {
            instance_id = instance_id,
            definition_id = definition_id,
        }
        if type(overrides) == "table" then
            for k, v in pairs(overrides) do
                if k ~= "instance_id" and k ~= "definition_id" then
                    actor_state[k] = v
                end
            end
        end

        game.state.actors[instance_id] = actor_state
        return wrap_actor(actor_state)
    end

    function registry.remove(instance_id)
        if type(instance_id) ~= "string" or instance_id == "" then
            return false
        end
        if not game or not game.state or not game.state.actors or game.state.actors[instance_id] == nil then
            return false
        end

        -- Check referential integrity: other actors referencing this actor (DLA-13)
        if game.state.actors then
            for other_id, other_state in pairs(game.state.actors) do
                if other_id ~= instance_id and type(other_state) == "table" then
                    for field_k, field_v in pairs(other_state) do
                        if field_v == instance_id then
                            error("ActorHasDependentReferences: cannot remove actor '" .. instance_id .. "' referenced in actor '" .. other_id .. "' field '" .. field_k .. "'", 2)
                        elseif type(field_v) == "table" then
                            for _, elem in ipairs(field_v) do
                                if elem == instance_id then
                                    error("ActorHasDependentReferences: cannot remove actor '" .. instance_id .. "' referenced in actor '" .. other_id .. "' field '" .. field_k .. "'", 2)
                                end
                            end
                        end
                    end
                end
            end
        end

        -- Check referential integrity: items owned by this actor
        if game.state.item_instances then
            for item_k, item in pairs(game.state.item_instances) do
                if type(item) == "table" and item.owner_id == instance_id then
                    error("ActorHasDependentReferences: cannot remove actor '" .. instance_id .. "' referenced as owner_id in item instance '" .. tostring(item_k) .. "'", 2)
                end
            end
        end

        -- Check referential integrity: quests targeting this actor
        if game.state.quests then
            for quest_k, quest in pairs(game.state.quests) do
                if type(quest) == "table" and (quest.target_actor_id == instance_id or quest.actor_id == instance_id) then
                    error("ActorHasDependentReferences: cannot remove actor '" .. instance_id .. "' referenced in quest '" .. tostring(quest_k) .. "'", 2)
                end
            end
        end

        if game.state.meta and game.state.meta.player_actor_id == instance_id then
            game.state.meta.player_actor_id = nil
        end
        game.state.actors[instance_id] = nil
        return true
    end

    return registry
end

local default_registry = nil

M.wrap = function(actor_state)
    if game and game.instances and game.instances.actors and game.instances.actors.wrap then
        return game.instances.actors.wrap(actor_state)
    end
    if not default_registry then
        default_registry = M.create_registry()
    end
    return default_registry.wrap(actor_state)
end

function M.register(_ctx)
    if not game.instances then
        game.instances = {}
    end
    game.instances.actors = M.create_registry()
end

return M
