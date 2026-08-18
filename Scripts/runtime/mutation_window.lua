local M = {
    id = "core:module.runtime.mutation_window",
}

local window_open = false

-- Monotonic mutation revision counter (DLA-02, ADR-0027).
-- Increments on every write to the guarded canonical state tree, whether direct,
-- via domain method, or through a gameplay service.
--
-- Single-command exclusivity justification:
-- 1. Synchronous nested command dispatch is forbidden (CommandDispatchReentrant).
-- 2. EventBus subscribers execute with a closed mutation window (post-commit).
-- 3. Deferred commands from the queue execute sequentially in their own separate
--    mutation windows.
-- Therefore, comparing write_revision before and after any block inside an active
-- command handler deterministically indicates whether canonical state was mutated.
local write_revision = 0

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
    local results = table.pack(pcall(fn, ...))
    window_open = prev
    if not results[1] then
        error(results[2], 0)
    end
    return table.unpack(results, 2, results.n)
end

function M.write_revision()
    return write_revision
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
            write_revision = write_revision + 1
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
M.is_guarded = function(tbl)
    return type(tbl) == "table" and raw_tables[tbl] ~= nil
end

-- DLA-03: unwrap is deliberately isolated from M public exports.
-- Neither gameplay modules nor mods can retrieve raw canonical state.
function M.create_controller()
    local controller = {
        is_open = M.is_open,
        open = M.open,
        close = M.close,
        execute_in_window = M.execute_in_window,
        write_revision = M.write_revision,
        guard_state = wrap,
        is_guarded = M.is_guarded,
    }
    local admin = {
        unwrap = unwrap,
    }
    return controller, admin
end

return M
