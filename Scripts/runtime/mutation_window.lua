local M = {
    id = "core:module.runtime.mutation_window",
}

local window_open = false

function M.is_open()
    return window_open
end

function M.open()
    window_open = true
end

function M.close()
    window_open = false
end

function M.execute_in_window(fn, ...)
    local prev = window_open
    window_open = true
    local ok, res_or_err = pcall(fn, ...)
    window_open = prev
    if not ok then
        error(res_or_err, 0)
    end
    return res_or_err
end

local raw_tables = setmetatable({}, { __mode = "k" })
local proxy_cache = setmetatable({}, { __mode = "k" })

local function unwrap(val)
    if type(val) == "table" and raw_tables[val] then
        return raw_tables[val]
    end
    return val
end

local function wrap(raw_tbl)
    if type(raw_tbl) ~= "table" then
        return raw_tbl
    end
    if game and game.null and raw_tbl == game.null then
        return raw_tbl
    end
    if raw_tables[raw_tbl] then
        return raw_tbl
    end
    if proxy_cache[raw_tbl] then
        return proxy_cache[raw_tbl]
    end

    local proxy = {}
    raw_tables[proxy] = raw_tbl
    proxy_cache[raw_tbl] = proxy

    local mt = {
        __index = function(_, key)
            local val = raw_tbl[key]
            if type(val) == "table" then
                return wrap(val)
            end
            return val
        end,
        __newindex = function(_, key, val)
            if not window_open then
                error("MutationWindowClosed: cannot mutate canonical state outside of active command handler", 2)
            end
            raw_tbl[key] = unwrap(val)
        end,
        __len = function(_)
            return #raw_tbl
        end,
        __pairs = function(_)
            local function stateless_iter(_, k)
                local next_k, next_v = next(raw_tbl, k)
                if next_k ~= nil then
                    if type(next_v) == "table" then
                        return next_k, wrap(next_v)
                    end
                    return next_k, next_v
                end
                return nil
            end
            return stateless_iter, nil, nil
        end,
        __metatable = false,
    }
    setmetatable(proxy, mt)
    return proxy
end

M.guard_state = wrap
M.unwrap_state = unwrap
M.is_guarded = function(tbl)
    return type(tbl) == "table" and raw_tables[tbl] ~= nil
end

return M
