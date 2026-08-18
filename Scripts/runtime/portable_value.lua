-- Portable Value Primitive (DLA-04, ADR-0027)
-- Unified validator and deep-copy primitive for portable data across GV2 boundaries
-- (event payloads, command arguments, deferred command queue, run manifest).
-- Enforces:
--   - Scalars: nil, boolean, string, finite number (no NaN, +inf, -inf)
--   - Special: game.null
--   - Containers: dense arrays (keys 1..N) or non-empty string-keyed maps
--   - Rejections: metatables, functions, userdata, threads, cycles, sparse arrays, mixed keys.

local M = {
    id = "core:module.runtime.portable_value",
}

local function is_finite_number(n)
    return n == n and n ~= math.huge and n ~= -math.huge
end

local function freeze_table(raw_tbl, path)
    local proxy = {}
    local mt = {
        __index = raw_tbl,
        __newindex = function(_, k, _)
            error("EventEnvelopeImmutable: attempt to modify read-only event payload at " .. path .. "." .. tostring(k), 2)
        end,
        __pairs = function(_) return pairs(raw_tbl) end,
        __ipairs = function(_) return ipairs(raw_tbl) end,
        __len = function(_) return #raw_tbl end,
        __tostring = function(_) return "EventPayloadProxy(" .. path .. ")" end,
        __metatable = false,
    }
    return setmetatable(proxy, mt)
end

function M.validate(val, path, error_prefix)
    path = path or "root"
    local prefix = error_prefix or "InvalidPortableValue"
    local visited = {}

    local function validate_node(node, node_path)
        local t = type(node)

        if t == "nil" or t == "boolean" or t == "string" then
            return true
        end

        if t == "number" then
            if not is_finite_number(node) then
                error(prefix .. ": non-finite number at " .. node_path, 0)
            end
            return true
        end

        if t == "table" then
            if game and game.null and node == game.null then
                return true
            end

            local mt = getmetatable(node)
            if mt ~= nil then
                error(prefix .. ": runtime object or wrapper with metatable is not allowed at " .. node_path, 0)
            end

            if visited[node] then
                error(prefix .. ": cyclic or shared table reference detected at " .. node_path, 0)
            end
            visited[node] = true

            local count = 0
            local has_int = false
            local has_str = false
            for k, _ in pairs(node) do
                count = count + 1
                if type(k) == "number" then
                    if math.type(k) ~= "integer" or k < 1 then
                        error(prefix .. ": non-positive integer key at " .. node_path, 0)
                    end
                    has_int = true
                elseif type(k) == "string" then
                    if k == "" then
                        error(prefix .. ": empty string key at " .. node_path, 0)
                    end
                    has_str = true
                else
                    error(prefix .. ": unsupported key type '" .. type(k) .. "' at " .. node_path, 0)
                end
            end

            if has_int and has_str then
                error(prefix .. ": mixed array and map keys at " .. node_path, 0)
            end

            if has_int then
                for i = 1, count do
                    if node[i] == nil then
                        error(prefix .. ": sparse array key gap at " .. node_path .. "[" .. i .. "]", 0)
                    end
                    validate_node(node[i], node_path .. "[" .. i .. "]")
                end
            else
                for k, v in pairs(node) do
                    validate_node(v, node_path .. "." .. k)
                end
            end

            visited[node] = nil
            return true
        end

        error(prefix .. ": unsupported value type '" .. t .. "' at " .. node_path, 0)
    end

    return validate_node(val, path)
end

function M.deep_copy_and_freeze(val, path, visited)
    path = path or "root"
    visited = visited or {}
    local t = type(val)

    if t == "nil" or t == "boolean" or t == "string" then
        return val
    end

    if t == "number" then
        if not is_finite_number(val) then
            error("InvalidEventPayload: non-finite number at " .. path, 0)
        end
        return val
    end

    if t == "table" then
        if game and game.null and val == game.null then
            return val
        end

        local mt = getmetatable(val)
        if mt ~= nil then
            error("InvalidEventPayload: runtime object or wrapper with metatable is not allowed at " .. path, 0)
        end

        if visited[val] then
            error("InvalidEventPayload: cyclic or shared table reference detected at " .. path, 0)
        end
        visited[val] = true

        local count = 0
        local has_int = false
        local has_str = false
        for k, _ in pairs(val) do
            count = count + 1
            if type(k) == "number" then
                if math.type(k) ~= "integer" or k < 1 then
                    error("InvalidEventPayload: non-positive integer key at " .. path, 0)
                end
                has_int = true
            elseif type(k) == "string" then
                if k == "" then
                    error("InvalidEventPayload: empty string key at " .. path, 0)
                end
                has_str = true
            else
                error("InvalidEventPayload: unsupported key type '" .. type(k) .. "' at " .. path, 0)
            end
        end

        if has_int and has_str then
            error("InvalidEventPayload: mixed array and map keys at " .. path, 0)
        end

        local copy = {}
        if has_int then
            for i = 1, count do
                if val[i] == nil then
                    error("InvalidEventPayload: sparse array key gap at " .. path .. "[" .. i .. "]", 0)
                end
                copy[i] = M.deep_copy_and_freeze(val[i], path .. "[" .. i .. "]", visited)
            end
        else
            for k, v in pairs(val) do
                copy[k] = M.deep_copy_and_freeze(v, path .. "." .. k, visited)
            end
        end

        visited[val] = nil
        return freeze_table(copy, path)
    end

    error("InvalidEventPayload: unsupported value type '" .. t .. "' at " .. path, 0)
end

return M
