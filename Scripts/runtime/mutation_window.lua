local M = {
    id = "core:module.runtime.mutation_window",
}

local window_open = false

-- ADR-0050: the mutation-window journal lives with the command, not the window.
-- `execute_in_window` (below) stays self-determining for its 96 existing call sites
-- (specs, one C++ save test) — it commits on normal return and rolls back on a
-- thrown error, with no journal involved. The deferred entry point below
-- (begin_transaction/commit_transaction/rollback_transaction) is for
-- command_dispatcher alone, which is the only caller that knows the command's
-- outcome only after the window has already closed (fail() is caught by the
-- handler wrapper *inside* the window and returns normally).
local current_txn = nil

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

-- ADR-0050 §2.2: the transaction a command opens must be resolved (commit or
-- rollback) before the next one begins. A forgotten resolution would otherwise
-- look like working code and diverge from fact silently (fail-closed).
function M.begin_transaction()
    if current_txn then
        error("MutationWindowTransactionUnresolved: previous transaction was not committed or rolled back before a new one began", 2)
    end
    current_txn = {
        journal = {},
        recorded = setmetatable({}, { __mode = "k" }),
    }
    window_open = true
    return current_txn
end

local function require_active_transaction(caller)
    if not current_txn then
        error("MutationWindowNoActiveTransaction: " .. caller .. " called without an active transaction", 2)
    end
end

-- Resolution (commit or rollback) clears current_txn entirely rather than
-- merely flagging it resolved: has_mutations() and begin_transaction()'s
-- fail-closed check both key off "is there a transaction right now", and a
-- resolved-but-retained journal would otherwise go on reporting mutations
-- that no longer describe anything open.
function M.commit_transaction()
    require_active_transaction("commit_transaction")
    current_txn = nil
    window_open = false
end

-- ADR-0050 §2.1: applies the journal in reverse; a key absent before the
-- command is restored absent (plain `tbl[key] = nil` removes it), not
-- present-with-nil, which Lua tables cannot represent anyway. Writes go
-- directly to the raw table so rollback itself is not journaled and does not
-- advance write_revision.
function M.rollback_transaction()
    require_active_transaction("rollback_transaction")
    local journal = current_txn.journal
    for i = #journal, 1, -1 do
        local entry = journal[i]
        if entry.had_value then
            entry.tbl[entry.key] = entry.old_value
        else
            entry.tbl[entry.key] = nil
        end
    end
    current_txn = nil
    window_open = false
end

-- Used by fail() (Scripts/authoring/context.lua) to detect a completed
-- mutation: a non-empty slice of the current transaction's journal for the
-- given category, replacing the write_revision comparison (ADR-0050 §2.3).
function M.has_mutations(category)
    if not current_txn then
        return false
    end
    for _, entry in ipairs(current_txn.journal) do
        if entry.category == category then
            return true
        end
    end
    return false
end

local raw_tables = setmetatable({}, { __mode = "k" })
local proxy_cache = setmetatable({}, { __mode = "k" })

local function unwrap(val)
    if type(val) == "table" and raw_tables[val] then
        return raw_tables[val]
    end
    return val
end

local function record_journal_entry(raw_tbl, key, category)
    if not current_txn then
        return
    end
    local recorded_for_tbl = current_txn.recorded[raw_tbl]
    if not recorded_for_tbl then
        recorded_for_tbl = {}
        current_txn.recorded[raw_tbl] = recorded_for_tbl
    end
    if recorded_for_tbl[key] then
        return
    end
    recorded_for_tbl[key] = true

    local had_value = raw_tbl[key] ~= nil
    local old_value = nil
    if had_value then
        old_value = raw_tbl[key]
    end
    table.insert(current_txn.journal, {
        tbl = raw_tbl,
        key = key,
        had_value = had_value,
        old_value = old_value,
        category = category,
    })
end

local function wrap(raw_tbl, category)
    category = category or "canonical"
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
                return wrap(val, category)
            end
            return val
        end,
        __newindex = function(_, key, val)
            if not window_open then
                error("MutationWindowClosed: cannot mutate canonical state outside of active command handler", 2)
            end
            record_journal_entry(raw_tbl, key, category)
            -- ADR-0050 §2.3: write_revision serves AuthoringFailAfterMutation's
            -- base re-read and nothing else in production; it grows only for
            -- the canonical category, never for transient mode state.
            if category == "canonical" then
                write_revision = write_revision + 1
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
                        return next_k, wrap(next_v, category)
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
