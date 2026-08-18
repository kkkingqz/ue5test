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
    "definitions",
}

M.canonical_sections = CANONICAL_SECTIONS

local reference_fields = {}
local registered_sections = {}
local is_frozen = false

function M.register_section(section_name)
    if is_frozen then
        error("StateSectionRegistryFrozen: cannot register state section after register phase", 2)
    end
    if type(section_name) ~= "string" or section_name == "" then
        error("InvalidStateSection: section_name must be a non-empty string", 2)
    end
    registered_sections[section_name] = true
end

function M.register_reference_field(field_name, expected_kind)
    if is_frozen then
        error("StateReferenceFieldRegistryFrozen: cannot register reference field after register phase", 2)
    end
    if type(field_name) ~= "string" or field_name == "" then
        error("InvalidStateReferenceField: field_name must be a non-empty string", 2)
    end
    if type(expected_kind) ~= "string" or expected_kind == "" then
        error("InvalidStateReferenceFieldKind: expected_kind must be a non-empty string", 2)
    end
    if reference_fields[field_name] ~= nil and reference_fields[field_name] ~= expected_kind then
        error("StateReferenceFieldConflict: field '" .. field_name .. "' is already registered with kind '" .. tostring(reference_fields[field_name]) .. "'", 2)
    end
    reference_fields[field_name] = expected_kind
end

function M.freeze_reference_fields()
    is_frozen = true
end

function M.freeze()
    is_frozen = true
end

function M.with_isolated_state(fn)
    local prev_ref_fields = reference_fields
    local prev_sections = registered_sections
    local prev_frozen = is_frozen

    reference_fields = {}
    for k, v in pairs(prev_ref_fields) do reference_fields[k] = v end
    registered_sections = {}
    for k, v in pairs(prev_sections) do registered_sections[k] = v end
    is_frozen = false
    M.definition_reference_fields = reference_fields

    local ok, res_or_err = pcall(fn)

    reference_fields = prev_ref_fields
    registered_sections = prev_sections
    is_frozen = prev_frozen
    M.definition_reference_fields = reference_fields

    if not ok then
        error(res_or_err, 0)
    end
    return res_or_err
end

function M.clear_for_test()
    reference_fields = {}
    registered_sections = {}
    is_frozen = false
    M.definition_reference_fields = reference_fields
end

function M.is_frozen()
    return is_frozen
end

function M.get_reference_fields()
    local copy = {}
    for k, v in pairs(reference_fields) do
        copy[k] = v
    end
    return copy
end

M.definition_reference_fields = reference_fields

function M.get_canonical_sections()
    local sections = {}
    for _, sec in ipairs(CANONICAL_SECTIONS) do
        sections[#sections + 1] = sec
    end
    for sec, _ in pairs(registered_sections) do
        sections[#sections + 1] = sec
    end
    return sections
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
    if registered_sections[name] then
        return true
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

        -- SAV-14 / CBM-10: any other field declared in reference_fields,
        -- wherever it appears in the tree.
        for field_name, expected_kind in pairs(reference_fields) do
            local ref_value = val[field_name]
            if ref_value ~= nil then
                if type(ref_value) ~= "string" then
                    error("LuaStateValidationInvalid: " .. field_name .. " at " .. path .. " must be a string")
                end
                if not stable_id.is_kind(ref_value, expected_kind) then
                    error("LuaStateValidationInvalid: invalid " .. field_name .. " '" .. ref_value .. "' at " .. path
                        .. " (must be a Stable ID of kind '" .. expected_kind .. "')")
                end
                if game and game.repository and game.repository.exists then
                    if not game.repository.exists(ref_value) then
                        error("LuaStateValidationInvalid: dangling " .. field_name .. " '" .. ref_value .. "' at " .. path
                            .. " (not found in pinned repository)")
                    end
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

    -- Definition reference fields (grammar, kind, existence) are checked
    -- generically by validate_node against the registry filled by gameplay
    -- packages (SAV-14, CBM-10), wherever such a field appears in the tree.

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

    -- Validate definitions section (DLA-14)
    if tree.definitions ~= nil then
        if type(tree.definitions) ~= "table" then
            error("LuaStateValidationInvalid: section state.definitions must be a table")
        end
        for def_id, def_state in pairs(tree.definitions) do
            if type(def_id) ~= "string" then
                error("LuaStateValidationInvalid: key in state.definitions must be a string definition_id")
            end
            local ns, kind, rest = def_id:match("^([a-z][a-z0-9_]*):([a-z][a-z0-9_]*)%.([a-z0-9_.]+)$")
            if not ns or not kind or not rest then
                error("LuaStateValidationInvalid: invalid definition_id grammar '" .. def_id .. "' in state.definitions")
            end
            if game and game.repository and game.repository.exists then
                if not game.repository.exists(def_id) then
                    error("LuaStateValidationInvalid: definition_id '" .. def_id .. "' in state.definitions not found in pinned repository")
                end
            end
            if type(def_state) ~= "table" then
                error("LuaStateValidationInvalid: definition state at state.definitions['" .. def_id .. "'] must be a table")
            end
        end
    end

    return true
end

return M
