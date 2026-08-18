-- Schema Property Model and Validation (DLA-10, DLA-11, DLA-12, DLA-13, ADR-0027)
-- Validates property writes against schema constraints before state mutation.
-- Provides collection wrappers with per-element validation and managed field checks.

local stable_id = require("core:module.runtime.stable_id")

local M = {
    id = "core:module.authoring.properties",
}

local registered_schemas = {}
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

function M.clear_for_test()
    registered_schemas = {}
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

return M
