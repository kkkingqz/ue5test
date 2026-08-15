local stable_id = require("core:module.runtime.stable_id")

local M = {
    id = "core:module.runtime.state_validator",
}

local CANONICAL_SECTIONS = {
    "meta",
    "player",
    "actors",
    "item_instances",
    "world",
    "quests",
    "mods",
}

M.canonical_sections = CANONICAL_SECTIONS

function M.get_canonical_sections()
    return CANONICAL_SECTIONS
end

function M.is_canonical_section(name)
    if type(name) ~= "string" then
        return false
    end
    for _, sec in ipairs(CANONICAL_SECTIONS) do
        if sec == name then
            return true
        end
    end
    return false
end

function M.create_empty_canonical_state()
    local state = {
        meta = {
            schema_version = 1,
            save_version = 1,
            save_id = "",
            instance_counters = {},
            prng = {},
            time = {},
        },
    }
    for _, sec in ipairs(CANONICAL_SECTIONS) do
        if sec ~= "meta" then
            state[sec] = {}
        end
    end
    return state
end

local function validate_node(val, path, visited, seen_instances)
    local t = type(val)

    if t == "string" or t == "boolean" then
        return true
    end

    if t == "number" then
        if val ~= val then
            error("LuaStateValidationInvalid: non-finite number (NaN) at " .. path)
        end
        if math.abs(val) == math.huge then
            error("LuaStateValidationInvalid: non-finite number (infinity) at " .. path)
        end
        return true
    end

    if t == "table" then
        -- Check if it is game.null
        if game and game.null and val == game.null then
            return true
        end

        -- Reject metatables
        if getmetatable(val) ~= nil then
            error("LuaStateValidationInvalid: table with metatable is not allowed at " .. path)
        end

        -- Reject cycles and shared identity
        if visited[val] then
            error("LuaStateValidationInvalid: cycle or shared table reference detected at " .. path)
        end
        visited[val] = true

        -- Reject definition tables (must store definition_id string instead of raw definition table)
        if val.schema_version ~= nil and val.id ~= nil and type(val.id) == "string" then
            error("LuaStateValidationInvalid: definition table cannot be stored in state at " .. path .. "; use definition_id string instead")
        end

        -- Validate instance_id grammar and uniqueness
        if val.instance_id ~= nil then
            if type(val.instance_id) ~= "string" then
                error("LuaStateValidationInvalid: instance_id at " .. path .. " must be a string")
            end
            local kind, counter = val.instance_id:match("^([a-z][a-z0-9_]*)@([1-9][0-9]*)$")
            if not kind or not counter or #kind > 64 then
                error("LuaStateValidationInvalid: invalid instance_id grammar '" .. val.instance_id .. "' at " .. path)
            end
            if seen_instances[val.instance_id] then
                error("LuaStateValidationInvalid: duplicate instance_id '" .. val.instance_id .. "' detected at " .. path .. " (already defined at " .. seen_instances[val.instance_id] .. ")")
            end
            seen_instances[val.instance_id] = path
        end

        -- Validate definition_id grammar and pinned repository resolution
        if val.definition_id ~= nil then
            if type(val.definition_id) ~= "string" then
                error("LuaStateValidationInvalid: definition_id at " .. path .. " must be a string")
            end
            local ns, kind, rest = val.definition_id:match("^([a-z][a-z0-9_]*):([a-z][a-z0-9_]*)%.([a-z0-9_.]+)$")
            if not ns or not kind or not rest then
                error("LuaStateValidationInvalid: invalid definition_id grammar '" .. val.definition_id .. "' at " .. path)
            end
            if game and game.repository and game.repository.exists then
                local exists = game.repository.exists(val.definition_id)
                if not exists then
                    error("LuaStateValidationInvalid: definition_id '" .. val.definition_id .. "' at " .. path .. " not found in pinned repository")
                end
            end
        end

        -- Check array vs object
        local count = 0
        local has_integer_key = false
        local has_string_key = false

        for k, _ in pairs(val) do
            count = count + 1
            local kt = type(k)
            if kt == "number" then
                has_integer_key = true
                if math.type(k) ~= "integer" or k < 1 then
                    error("LuaStateValidationInvalid: invalid array index '" .. tostring(k) .. "' at " .. path)
                end
            elseif kt == "string" then
                has_string_key = true
            else
                error("LuaStateValidationInvalid: invalid key type '" .. kt .. "' at " .. path)
            end
        end

        if has_integer_key and has_string_key then
            error("LuaStateValidationInvalid: mixed array and object keys at " .. path)
        end

        if has_integer_key then
            -- Must be dense array from 1 to count
            local len = #val
            if len ~= count then
                error("LuaStateValidationInvalid: sparse array detected at " .. path .. " (length " .. tostring(len) .. " != count " .. tostring(count) .. ")")
            end
            for i = 1, len do
                if rawget(val, i) == nil then
                    error("LuaStateValidationInvalid: sparse array hole detected at " .. path .. "[" .. tostring(i) .. "]")
                end
                validate_node(rawget(val, i), path .. "[" .. tostring(i) .. "]", visited, seen_instances)
            end
        else
            -- Object with string keys
            for k, v in pairs(val) do
                validate_node(v, path .. "." .. k, visited, seen_instances)
            end
        end

        return true
    end

    -- Any other type (function, userdata, thread, etc.) is rejected
    error("LuaStateValidationInvalid: disallowed value type '" .. t .. "' at " .. path)
end

function M.validate_state_tree(tree)
    if type(tree) ~= "table" then
        error("LuaStateValidationInvalid: root state must be a table")
    end
    if getmetatable(tree) ~= nil then
        error("LuaStateValidationInvalid: root state cannot have a metatable")
    end

    for k, _ in pairs(tree) do
        if not M.is_canonical_section(k) then
            error("LuaStateValidationInvalid: invalid top-level section '" .. tostring(k) .. "' in state tree")
        end
    end

    local visited = {}
    local seen_instances = {}
    for _, sec in ipairs(CANONICAL_SECTIONS) do
        local val = rawget(tree, sec)
        if val == nil then
            error("LuaStateValidationInvalid: missing required canonical section '" .. sec .. "'")
        end
        if type(val) ~= "table" then
            error("LuaStateValidationInvalid: section '" .. sec .. "' must be a table")
        end
        validate_node(val, "state." .. sec, visited, seen_instances)
    end

    -- Validate explicit canonical structure of meta section
    local meta = tree.meta
    if meta.schema_version ~= nil then
        if type(meta.schema_version) ~= "number" or math.type(meta.schema_version) ~= "integer" or meta.schema_version < 1 then
            error("LuaStateValidationInvalid: meta.schema_version must be a positive integer")
        end
    end
    if meta.save_version ~= nil then
        if type(meta.save_version) ~= "number" or math.type(meta.save_version) ~= "integer" or meta.save_version < 1 then
            error("LuaStateValidationInvalid: meta.save_version must be a positive integer")
        end
    end
    if meta.save_id ~= nil and type(meta.save_id) ~= "string" then
        error("LuaStateValidationInvalid: meta.save_id must be a string")
    end
    if meta.instance_counters ~= nil then
        if type(meta.instance_counters) ~= "table" or getmetatable(meta.instance_counters) ~= nil then
            error("LuaStateValidationInvalid: meta.instance_counters must be a plain table")
        end
        for k, v in pairs(meta.instance_counters) do
            if type(k) ~= "string" or not k:match("^[a-z][a-z0-9_]*$") or #k > 64 then
                error("LuaStateValidationInvalid: invalid kind in meta.instance_counters: " .. tostring(k))
            end
            if type(v) ~= "number" or math.type(v) ~= "integer" or v < 1 then
                error("LuaStateValidationInvalid: invalid counter value in meta.instance_counters for kind: " .. tostring(k))
            end
        end
    end
    if meta.prng ~= nil and (type(meta.prng) ~= "table" or getmetatable(meta.prng) ~= nil) then
        error("LuaStateValidationInvalid: meta.prng must be a plain table")
    end
    if meta.time ~= nil and (type(meta.time) ~= "table" or getmetatable(meta.time) ~= nil) then
        error("LuaStateValidationInvalid: meta.time must be a plain table")
    end
    if meta.player_actor_id ~= nil then
        if type(meta.player_actor_id) ~= "string" or not meta.player_actor_id:match("^[a-z][a-z0-9_]*@[1-9][0-9]*$") then
            error("LuaStateValidationInvalid: invalid meta.player_actor_id grammar '" .. tostring(meta.player_actor_id) .. "'")
        end
        if tree.actors[meta.player_actor_id] == nil then
            error("LuaStateValidationInvalid: dangling meta.player_actor_id '" .. meta.player_actor_id .. "' (actor not found in state.actors)")
        end
    end

    -- Validate world.current_location_id grammar, kind, and pinned repository resolution (GEW-05)
    if tree.world.current_location_id ~= nil then
        if type(tree.world.current_location_id) ~= "string" then
            error("LuaStateValidationInvalid: world.current_location_id must be a string")
        end
        if not stable_id.is_kind(tree.world.current_location_id, "location") then
            error("LuaStateValidationInvalid: invalid world.current_location_id '" .. tree.world.current_location_id .. "' (must be a Stable ID of kind 'location')")
        end
        if game and game.repository and game.repository.exists then
            if not game.repository.exists(tree.world.current_location_id) then
                error("LuaStateValidationInvalid: dangling world.current_location_id '" .. tree.world.current_location_id .. "' (location not found in pinned repository)")
            end
        end
    end

    -- Validate player section does not duplicate Actor model fields
    if tree.player.instance_id ~= nil or tree.player.definition_id ~= nil then
        error("LuaStateValidationInvalid: section state.player cannot duplicate Actor model (instance_id/definition_id); player actor state belongs in state.actors")
    end

    -- Validate item instances ownership invariants
    for k, item in pairs(tree.item_instances) do
        if type(item) ~= "table" then
            error("LuaStateValidationInvalid: item instance entry at state.item_instances." .. tostring(k) .. " must be a table")
        end
        if item.instance_id == nil then
            error("LuaStateValidationInvalid: unique item instance at state.item_instances." .. tostring(k) .. " is missing required instance_id")
        end
        if item.definition_id == nil then
            error("LuaStateValidationInvalid: unique item instance at state.item_instances." .. tostring(k) .. " is missing required definition_id")
        end
        if item.owner_id == nil or type(item.owner_id) ~= "string" or item.owner_id == "" then
            error("LuaStateValidationInvalid: unique item instance at state.item_instances." .. tostring(k) .. " is missing required owner_id container reference")
        end
        if item.owner_id:match("^actor@[1-9][0-9]*$") then
            if tree.actors[item.owner_id] == nil then
                error("LuaStateValidationInvalid: dangling owner_id '" .. item.owner_id .. "' in state.item_instances." .. tostring(k) .. " (actor not found in state.actors)")
            end
        end
    end

    return true
end

return M
