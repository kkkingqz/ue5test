-- Schema Property Model and Validation (DLA-10, DLA-11, DLA-12, DLA-13, ADR-0027)
-- Validates property writes against schema constraints before state mutation.
-- Provides collection wrappers with per-element validation and managed field checks.

local stable_id = require("core:module.runtime.stable_id")

local M = {
    id = "core:module.authoring.properties",
}

local registered_schemas = {}
local registered_def_decorators = {}
local is_frozen = false

function M.register_schema(discriminator_or_type, schema_def)
    if is_frozen then
        error("SchemaRegistryFrozen: cannot register schema after register phase", 2)
    end
    if type(discriminator_or_type) ~= "string" or discriminator_or_type == "" then
        error("InvalidSchemaTarget: target must be a non-empty string", 2)
    end
    if type(schema_def) ~= "table" or type(schema_def.fields) ~= "table" then
        error("InvalidSchemaDefinition: schema_def must be a table with a fields map", 2)
    end
    registered_schemas[discriminator_or_type] = schema_def
end

function M.register_definition_type(kind_or_discriminator, decorator_fn)
    if is_frozen then
        error("DefinitionTypeRegistryFrozen: cannot register definition type after register phase", 2)
    end
    if type(kind_or_discriminator) ~= "string" or kind_or_discriminator == "" then
        error("InvalidDefinitionTypeTarget: target must be a non-empty string", 2)
    end
    if type(decorator_fn) ~= "function" then
        error("InvalidDefinitionDecorator: decorator must be a function", 2)
    end
    if registered_def_decorators[kind_or_discriminator] then
        local existing = registered_def_decorators[kind_or_discriminator]
        registered_def_decorators[kind_or_discriminator] = function(base)
            local d1 = existing(base)
            local d2 = decorator_fn(d1 or base)
            if type(d2) == "table" and type(d1) == "table" then
                return setmetatable(d2, { __index = d1 })
            end
            return d2 or d1 or base
        end
    else
        registered_def_decorators[kind_or_discriminator] = decorator_fn
    end
end

function M.get_schema(discriminator_or_type)
    if not discriminator_or_type then
        return nil
    end
    return registered_schemas[discriminator_or_type]
end

function M.schemas()
    local result = {}
    for k, v in pairs(registered_schemas) do
        result[k] = v
    end
    return result
end

function M.freeze()
    is_frozen = true
end

function M.is_frozen()
    return is_frozen
end

function M.with_isolated_state(fn)
    local prev_schemas = registered_schemas
    local prev_decorators = registered_def_decorators
    local prev_frozen = is_frozen

    registered_schemas = {}
    for k, v in pairs(prev_schemas) do registered_schemas[k] = v end
    registered_def_decorators = {}
    for k, v in pairs(prev_decorators) do registered_def_decorators[k] = v end
    is_frozen = false

    local ok, res_or_err = pcall(fn)

    registered_schemas = prev_schemas
    registered_def_decorators = prev_decorators
    is_frozen = prev_frozen

    if not ok then
        error(res_or_err, 0)
    end
    return res_or_err
end

function M.clear_for_test()
    registered_schemas = {}
    registered_def_decorators = {}
    is_frozen = false
end

function M.canonicalize_value(val)
    if type(val) == "table" then
        if val.__gv2_ref == "instance" and val.id then
            return val.id
        elseif val.__gv2_ref == "definition" and val.id then
            return val.id
        elseif val.instance_id then
            return val.instance_id
        elseif val.definition_id then
            return val.definition_id
        end
    end
    return val
end

function M.validate_field_value(field_name, field_spec, raw_value)
    local value = M.canonicalize_value(raw_value)

    if value == nil then
        if field_spec.nullable or not field_spec.required then
            return true, nil
        else
            error("FieldValidationNullDisallowed: field '" .. tostring(field_name) .. "' cannot be nil", 2)
        end
    end

    local kind = field_spec.kind or "string"

    if kind == "bool" or kind == "boolean" then
        if type(value) ~= "boolean" then
            error("FieldValidationTypeMismatch: field '" .. tostring(field_name) .. "' expected boolean, got " .. type(value), 2)
        end
    elseif kind == "int64" or kind == "integer" then
        if type(value) ~= "number" or math.type(value) ~= "integer" then
            error("FieldValidationTypeMismatch: field '" .. tostring(field_name) .. "' expected integer, got " .. type(value), 2)
        end
        if field_spec.min ~= nil and value < field_spec.min then
            error("FieldValidationConstraintFailed: field '" .. tostring(field_name) .. "' value " .. tostring(value) .. " is below min " .. tostring(field_spec.min), 2)
        end
        if field_spec.max ~= nil and value > field_spec.max then
            error("FieldValidationConstraintFailed: field '" .. tostring(field_name) .. "' value " .. tostring(value) .. " exceeds max " .. tostring(field_spec.max), 2)
        end
    elseif kind == "number" or kind == "double" then
        if type(value) ~= "number" then
            error("FieldValidationTypeMismatch: field '" .. tostring(field_name) .. "' expected number, got " .. type(value), 2)
        end
        if field_spec.min ~= nil and value < field_spec.min then
            error("FieldValidationConstraintFailed: field '" .. tostring(field_name) .. "' value " .. tostring(value) .. " is below min " .. tostring(field_spec.min), 2)
        end
        if field_spec.max ~= nil and value > field_spec.max then
            error("FieldValidationConstraintFailed: field '" .. tostring(field_name) .. "' value " .. tostring(value) .. " exceeds max " .. tostring(field_spec.max), 2)
        end
    elseif kind == "string" then
        if type(value) ~= "string" then
            error("FieldValidationTypeMismatch: field '" .. tostring(field_name) .. "' expected string, got " .. type(value), 2)
        end
        if field_spec.min_length ~= nil and #value < field_spec.min_length then
            error("FieldValidationConstraintFailed: field '" .. tostring(field_name) .. "' length is below min_length " .. tostring(field_spec.min_length), 2)
        end
        if field_spec.max_length ~= nil and #value > field_spec.max_length then
            error("FieldValidationConstraintFailed: field '" .. tostring(field_name) .. "' length exceeds max_length " .. tostring(field_spec.max_length), 2)
        end
    elseif kind == "enum" then
        local found = false
        if type(field_spec.values) == "table" then
            for _, v in ipairs(field_spec.values) do
                if v == value then
                    found = true
                    break
                end
            end
        end
        if not found then
            error("FieldValidationEnumMismatch: field '" .. tostring(field_name) .. "' value '" .. tostring(value) .. "' is not in allowed enum values", 2)
        end
    elseif kind == "ref" or kind == "ref_definition" then
        if type(value) ~= "string" then
            error("FieldValidationTypeMismatch: field '" .. tostring(field_name) .. "' expected definition Stable ID string or definition handle, got " .. type(value), 2)
        end
        if field_spec.target_kind and not stable_id.is_kind(value, field_spec.target_kind) then
            error("FieldValidationTargetKindMismatch: field '" .. tostring(field_name) .. "' expected Stable ID of kind '" .. tostring(field_spec.target_kind) .. "', got '" .. tostring(value) .. "'", 2)
        end
    elseif kind == "ref_instance" then
        if type(value) ~= "string" then
            error("FieldValidationTypeMismatch: field '" .. tostring(field_name) .. "' expected instance ID string or ActorWrapper, got " .. type(value), 2)
        end
        if field_spec.target_kind then
            local prefix = field_spec.target_kind .. "@"
            if not string.find(value, "@", 1, true) and not stable_id.is_valid(value) then
                error("FieldValidationInvalidInstanceId: field '" .. tostring(field_name) .. "' expected valid instance ID, got '" .. tostring(value) .. "'", 2)
            end
        end
    elseif kind == "text_id" then
        if type(value) ~= "string" or not stable_id.is_kind(value, "text") then
            error("FieldValidationTypeMismatch: field '" .. tostring(field_name) .. "' expected Stable ID of kind 'text', got '" .. tostring(value) .. "'", 2)
        end
    elseif kind == "array" then
        if type(value) ~= "table" then
            error("FieldValidationTypeMismatch: field '" .. tostring(field_name) .. "' expected array table, got " .. type(value), 2)
        end
        if field_spec.items then
            for i, item in ipairs(value) do
                M.validate_field_value(field_name .. "[" .. tostring(i) .. "]", field_spec.items, item)
            end
        end
    end

    return true, value
end

function M.wrap_collection(container_state, field_name, field_spec)
    if container_state[field_name] == nil then
        container_state[field_name] = {}
    end
    local raw_list = container_state[field_name]
    local item_spec = field_spec.items or { kind = "string" }

    local methods = {}

    function methods.add(item)
        local ok, canonical_val = M.validate_field_value(field_name .. "[]", item_spec, item)
        table.insert(raw_list, canonical_val)
        return true
    end

    function methods.remove(item)
        local canonical_val = M.canonicalize_value(item)
        for i, v in ipairs(raw_list) do
            if v == canonical_val or v == item then
                table.remove(raw_list, i)
                return true
            end
        end
        return false
    end

    function methods.clear()
        for i = #raw_list, 1, -1 do
            raw_list[i] = nil
        end
    end

    function methods.count()
        return #raw_list
    end

    local wrapper = {}
    local mt = {
        __index = function(_, k)
            if methods[k] ~= nil then
                return methods[k]
            end
            if type(k) == "number" then
                local elem = raw_list[k]
                if elem ~= nil and item_spec.kind == "ref_instance" then
                    if game and game.instances and game.instances.actors and game.instances.actors.get then
                        return game.instances.actors.get(elem)
                    end
                elseif elem ~= nil and (item_spec.kind == "ref" or item_spec.kind == "ref_definition") then
                    if game and game.repository and game.repository.get then
                        return game.repository.get(elem)
                    end
                end
                return elem
            end
            return nil
        end,
        __newindex = function(_, k, v)
            if type(k) == "number" then
                local _, canonical_val = M.validate_field_value(field_name .. "[" .. tostring(k) .. "]", item_spec, v)
                raw_list[k] = canonical_val
            else
                error("CollectionWrapperInvalidKey: numeric index expected, got " .. tostring(k), 2)
            end
        end,
        __len = function(_)
            return #raw_list
        end,
        __pairs = function(_)
            return pairs(raw_list)
        end,
        __ipairs = function(_)
            return ipairs(raw_list)
        end,
        __tostring = function(_)
            return "AuthoringCollectionWrapper(" .. tostring(field_name) .. ", size=" .. tostring(#raw_list) .. ")"
        end,
    }

    setmetatable(wrapper, mt)
    return wrapper
end

function M.wrap_definition(def_id)
    if type(def_id) ~= "string" or def_id == "" then
        error("DefinitionWrapperError: def_id must be a non-empty string", 2)
    end
    local ns, kind, rest = def_id:match("^([a-z][a-z0-9_]*):([a-z][a-z0-9_]*)%.([a-z0-9_.]+)$")
    if not ns or not kind or not rest then
        error("DefinitionWrapperError: invalid definition Stable ID grammar '" .. def_id .. "'", 2)
    end

    local function get_def_data()
        if game and game.repository and game.repository.get then
            return game.repository.get(def_id)
        end
        return nil
    end

    local base_wrapper = {
        __gv2_ref = "definition",
        id = def_id,
        definition_id = def_id,
    }

    local methods = {}

    function methods.get_def()
        return get_def_data()
    end

    function methods.reset(self_or_field, maybe_field)
        local field_name = (type(self_or_field) == "table" and maybe_field) or self_or_field
        if not field_name or type(field_name) ~= "string" then
            error("DefinitionResetError: field_name must be a non-empty string", 2)
        end
        if game and game.state and game.state.definitions and game.state.definitions[def_id] then
            game.state.definitions[def_id][field_name] = nil
        end
    end

    local mt = {
        __index = function(_, k)
            if methods[k] ~= nil then
                return methods[k]
            end
            if k == "id" or k == "definition_id" or k == "__gv2_ref" then
                return base_wrapper[k]
            end

            if game and game.entity_extensions and game.entity_extensions.get_method then
                local pascal_kind = (type(kind) == "string" and kind ~= "") and (kind:sub(1,1):upper() .. kind:sub(2)) or kind
                local ext_fn = game.entity_extensions.get_method(pascal_kind, k) or game.entity_extensions.get_method(kind, k)
                if ext_fn ~= nil then
                    return ext_fn
                end
            end

            local def = get_def_data()
            local discriminator = nil
            if def then
                if def.discriminator then
                    discriminator = def.discriminator
                elseif def.data and def.data.discriminator then
                    discriminator = def.data.discriminator
                else
                    discriminator = kind
                end
            else
                discriminator = kind
            end

            local schema = M.get_schema(discriminator) or M.get_schema(kind)
            local field_spec = schema and schema.fields and schema.fields[k]

            if field_spec then
                local storage = field_spec.storage or "definition"
                if storage == "runtime_state" then
                    local runtime_val = nil
                    if game and game.state and game.state.definitions and game.state.definitions[def_id] then
                        runtime_val = game.state.definitions[def_id][k]
                    end
                    if runtime_val ~= nil then
                        if field_spec.kind == "ref_instance" then
                            if game and game.instances and game.instances.actors and game.instances.actors.get then
                                return game.instances.actors.get(runtime_val)
                            end
                        elseif field_spec.kind == "ref" or field_spec.kind == "ref_definition" then
                            if game and game.repository and game.repository.get then
                                return game.repository.get(runtime_val)
                            end
                        elseif field_spec.kind == "array" or field_spec.kind == "map" then
                            return M.wrap_collection(game.state.definitions[def_id], k, field_spec)
                        end
                        return runtime_val
                    else
                        return field_spec.default
                    end
                else
                    if def then
                        if def.data and def.data[k] ~= nil then
                            return def.data[k]
                        elseif def[k] ~= nil then
                            return def[k]
                        end
                    end
                    return field_spec.default
                end
            else
                if def then
                    if def.data and def.data[k] ~= nil then
                        return def.data[k]
                    elseif def[k] ~= nil then
                        return def[k]
                    end
                end
                return nil
            end
        end,

        __newindex = function(_, k, v)
            local def = get_def_data()
            local discriminator = nil
            if def then
                if def.discriminator then
                    discriminator = def.discriminator
                elseif def.data and def.data.discriminator then
                    discriminator = def.data.discriminator
                else
                    discriminator = kind
                end
            else
                discriminator = kind
            end

            local schema = M.get_schema(discriminator) or M.get_schema(kind)
            local field_spec = schema and schema.fields and schema.fields[k]

            if not field_spec then
                error("Cannot modify definition field '" .. tostring(k) .. "' (no runtime_state schema defined)", 2)
            end

            local storage = field_spec.storage or "definition"
            if storage == "definition" then
                error("Cannot modify definition field '" .. tostring(k) .. "'", 2)
            end

            local write_policy = field_spec.write_policy or "plain"
            if write_policy == "read_only" then
                error("Cannot modify read_only field '" .. tostring(k) .. "'", 2)
            elseif write_policy == "managed" then
                local ops_str = ""
                if type(field_spec.operations) == "table" and #field_spec.operations > 0 then
                    ops_str = " (use domain operations: " .. table.concat(field_spec.operations, ", ") .. ")"
                end
                error("Cannot assign directly to managed field '" .. tostring(k) .. "'" .. ops_str, 2)
            end

            local _, canonical_val = M.validate_field_value(k, field_spec, v)

            if not game or not game.state or not game.state.definitions then
                error("DefinitionStateError: game.state.definitions is not available", 2)
            end

            if game.state.definitions[def_id] == nil then
                game.state.definitions[def_id] = {}
            end

            game.state.definitions[def_id][k] = canonical_val
        end,

        __tostring = function(_)
            return "DefinitionWrapper(" .. def_id .. ")"
        end,
    }

    setmetatable(base_wrapper, mt)

    local def = get_def_data()
    local discriminator = def and (def.discriminator or (def.data and def.data.discriminator)) or kind
    local dec = registered_def_decorators[discriminator] or registered_def_decorators[kind]

    if dec then
        local decorated = dec(base_wrapper)
        if type(decorated) == "table" then
            setmetatable(decorated, {
                __index = base_wrapper,
                __newindex = function(_, k, v)
                    base_wrapper[k] = v
                end,
                __tostring = function(_)
                    return "DecoratedDefinitionWrapper(" .. def_id .. ")"
                end,
            })
            return decorated
        end
    end

    return base_wrapper
end

return M
