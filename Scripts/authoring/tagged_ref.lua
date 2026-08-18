-- DLA-04, DLA-07: Tagged Reference & Argument Canonicalization Primitive (ADR-0027)
-- Canonicalizes high-level runtime and definition objects into portable tagged references:
--   { __gv2_ref = "instance", id = "actor@42" }
--   { __gv2_ref = "definition", id = "pkg:location.city.tavern" }
-- Rehydrates tagged references back into fresh runtime wrappers or definition objects
-- on the receiving boundary of command handlers and event subscribers.
-- Leaves plain strings, numbers, and booleans unchanged (preserving string IDs).

local stable_id = require("core:module.runtime.stable_id")
local portable_value = require("core:module.runtime.portable_value")

local M = {
    id = "core:module.authoring.tagged_ref",
}

local function is_actor_wrapper(val)
    if type(val) ~= "table" then
        return false
    end
    if val.__is_actor_wrapper == true then
        return true
    end
    if type(val.instance_id) == "string" and (type(val.definition_id) == "string" or val.get_state ~= nil) then
        return true
    end
    return false
end

local function is_definition_handle(val)
    if type(val) ~= "table" then
        return false
    end
    if val.__is_definition_handle == true then
        return true
    end
    if type(val.definition_id) == "string" and stable_id.is_valid(val.definition_id) then
        return true
    end
    return false
end

function M.is_tagged_ref(val)
    if type(val) ~= "table" then
        return false
    end
    local ref_kind = val.__gv2_ref
    if ref_kind == "instance" or ref_kind == "definition" then
        return type(val.id) == "string" and val.id ~= ""
    end
    return false
end

local function canonicalize_value_internal(val, options, visited)
    if val == nil then
        return nil
    end

    local val_type = type(val)
    if val_type == "boolean" or val_type == "number" or val_type == "string" then
        return val
    end

    if val_type ~= "table" then
        error("InvalidAuthoringArgument: non-portable type '" .. val_type .. "' cannot be passed across authoring boundary", 3)
    end

    -- Check for cycles
    if visited[val] then
        error("InvalidAuthoringArgument: cyclic table reference detected", 3)
    end

    -- 1. Actor wrapper -> tagged instance ref
    if is_actor_wrapper(val) then
        if options and options.allow_plain_id then
            return val.instance_id
        end
        return {
            __gv2_ref = "instance",
            id = val.instance_id,
        }
    end

    -- 2. Definition handle -> tagged definition ref
    if is_definition_handle(val) then
        local def_id = val.definition_id or val.id
        if options and options.allow_plain_id then
            return def_id
        end
        return {
            __gv2_ref = "definition",
            id = def_id,
        }
    end

    -- 3. Already a tagged reference
    if M.is_tagged_ref(val) then
        if options and options.allow_plain_id then
            return val.id
        end
        return {
            __gv2_ref = val.__gv2_ref,
            id = val.id,
        }
    end

    -- 4. General table (array or map)
    visited[val] = true
    local copy = {}

    -- Check if table has array-like or map-like keys
    local is_array = true
    local count = 0
    for k, _ in pairs(val) do
        count = count + 1
        if type(k) ~= "number" or k < 1 or math.floor(k) ~= k then
            is_array = false
            break
        end
    end

    if is_array and count > 0 then
        for i = 1, #val do
            copy[i] = canonicalize_value_internal(val[i], options, visited)
        end
    else
        for k, v in pairs(val) do
            if type(k) ~= "string" or k == "" then
                visited[val] = nil
                error("InvalidAuthoringArgument: table keys must be non-empty strings, got '" .. tostring(k) .. "'", 3)
            end
            copy[k] = canonicalize_value_internal(v, options, visited)
        end
    end

    visited[val] = nil
    return copy
end

function M.canonicalize_arg(val, options)
    local visited = {}
    return canonicalize_value_internal(val, options, visited)
end

function M.canonicalize_args(...)
    local num_args = select("#", ...)
    if num_args == 0 then
        return {}
    end

    if num_args == 1 then
        local first = select(1, ...)
        if type(first) == "table" and not is_actor_wrapper(first) and not is_definition_handle(first) and not M.is_tagged_ref(first) then
            -- Single table passed as named/structured args or array
            local canonical_tbl = M.canonicalize_arg(first)
            portable_value.validate(canonical_tbl, "args")
            return canonical_tbl
        else
            -- Single scalar or single entity wrapper passed as positional argument
            local canonical_val = M.canonicalize_arg(first)
            local result = { canonical_val }
            portable_value.validate(result, "args")
            return result
        end
    end

    -- Multiple arguments passed positionally
    local result = {}
    for i = 1, num_args do
        local val = select(i, ...)
        result[i] = M.canonicalize_arg(val)
    end

    portable_value.validate(result, "args")
    return result
end

local function rehydrate_value_internal(val, visited)
    if val == nil then
        return nil
    end

    local val_type = type(val)
    if val_type ~= "table" then
        return val
    end

    if visited[val] then
        return val
    end

    -- Check if it's a tagged reference
    if M.is_tagged_ref(val) then
        if val.__gv2_ref == "instance" then
            if game and game.instances and game.instances.actors and game.instances.actors.get then
                local instance = game.instances.actors.get(val.id)
                return instance
            end
            return val.id
        elseif val.__gv2_ref == "definition" then
            if game and game.repository and game.repository.get then
                local def = game.repository.get(val.id)
                return def
            end
            return val.id
        end
    end

    -- Recursively rehydrate tables
    visited[val] = true
    local rehydrated = {}
    local is_array = true
    local count = 0

    for k, _ in pairs(val) do
        count = count + 1
        if type(k) ~= "number" or k < 1 or math.floor(k) ~= k then
            is_array = false
            break
        end
    end

    if is_array and count > 0 then
        for i = 1, #val do
            rehydrated[i] = rehydrate_value_internal(val[i], visited)
        end
    else
        for k, v in pairs(val) do
            rehydrated[k] = rehydrate_value_internal(v, visited)
        end
    end

    visited[val] = nil
    return rehydrated
end

function M.rehydrate_arg(val)
    local visited = {}
    return rehydrate_value_internal(val, visited)
end

function M.rehydrate_args(args)
    if args == nil then
        return {}
    end
    local visited = {}
    return rehydrate_value_internal(args, visited)
end

return M
