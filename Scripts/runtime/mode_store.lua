-- EGC-05/ADR-0051: one `core`-owned store for the active temporary game mode
-- (D1 "Transient mode state") plus the declared mode registry (D2 contract).
-- At most one mode is active at any observed point. Its run content is
-- portable-values-only, declared by field.* descriptors the same way entity
-- prototype fields are, and written through the same guarded proxy as
-- canonical state (ADR-0050), tagged category "transient" so write_revision
-- never grows from a run write and the journal rolls run writes back with
-- the command exactly like canonical writes.
--
-- This module owns storage and the plain state machine only. The three typed
-- refusals ADR-0051 §D2/§D6 names (mode.not_active, mode.already_active,
-- mode.active) are raised by the callers that have access to M.fail
-- (Scripts/authoring/context.lua's `modes` surface, Scripts/runtime/
-- session_controls.lua's save handler) -- not here, to avoid a dependency
-- cycle (context.lua already requires this module).

local mutation_window = require("core:module.runtime.mutation_window")
local test_state_registry = require("core:module.runtime.test_state_registry")
local properties = require("core:module.authoring.properties")
local random = require("core:module.runtime.random")
local event_bus = require("core:module.runtime.event_bus")

local M = {
    id = "core:module.runtime.mode_store",
}

local declared_modes = {}
local declared_order = {}
local is_frozen = false

-- Two raw tables, each wrapped once and kept at a stable identity: `active`
-- carries only which mode (if any) is running, `run` carries the fields that
-- mode declared. Both categories are "transient" (ADR-0050 §2.3): writes here
-- never increment write_revision and never trip AuthoringFailAfterMutation.
local active_raw = { mode_id = nil }
local run_raw = {}
local active_proxy = mutation_window.guard_state(active_raw, "transient")
local run_proxy = mutation_window.guard_state(run_raw, "transient")

function M.declare(mode_id, decl)
    if is_frozen then
        error("ModeDeclarationAfterFreeze: cannot declare mode '" .. tostring(mode_id) .. "' after register phase / freeze", 2)
    end
    if declared_modes[mode_id] ~= nil then
        error("ModeDuplicateDeclaration: mode '" .. tostring(mode_id) .. "' is already declared", 2)
    end
    declared_modes[mode_id] = decl
    table.insert(declared_order, mode_id)
end

function M.freeze()
    is_frozen = true
end

function M.is_frozen()
    return is_frozen
end

function M.get_declaration(mode_id)
    return declared_modes[mode_id]
end

function M.declared_ids()
    local ids = {}
    for _, id in ipairs(declared_order) do
        table.insert(ids, id)
    end
    return ids
end

function M.active()
    return active_raw.mode_id
end

-- EGC-06/ADR-0051 §D8: both facts are enqueued here (not published directly),
-- so event_bus's existing per-command pending-events buffer is what gives
-- them "post-commit only" delivery -- a command that begins or ends a mode
-- and then fails rolls back the mode_store write via the ADR-0050 journal
-- exactly like any other transient write, and event_bus.rollback_command_context
-- discards the pending event the same way it discards any other.
function M.begin(mode_id)
    if active_raw.mode_id ~= nil then
        error("ModeStoreAlreadyActive: cannot begin mode '" .. tostring(mode_id) .. "' while '" .. tostring(active_raw.mode_id) .. "' is active", 2)
    end
    if declared_modes[mode_id] == nil then
        error("ModeStoreUnknownMode: mode '" .. tostring(mode_id) .. "' is not declared", 2)
    end
    active_proxy.mode_id = mode_id
    event_bus.enqueue({
        event_id = "core:event.mode.started",
        payload = { mode_id = mode_id },
    })
end

-- outcome: "finished" (modes.finish()) or "cancelled" (modes.cancel()).
function M.end_mode(outcome)
    if outcome ~= "finished" and outcome ~= "cancelled" then
        error("ModeStoreInvalidOutcome: outcome must be 'finished' or 'cancelled', got " .. tostring(outcome), 2)
    end
    if active_raw.mode_id == nil then
        error("ModeStoreNotActive: no mode is active", 2)
    end
    local mode_id = active_raw.mode_id
    active_proxy.mode_id = nil
    for key in pairs(run_raw) do
        run_proxy[key] = nil
    end
    event_bus.enqueue({
        event_id = "core:event.mode.finished",
        payload = { mode_id = mode_id, outcome = outcome },
    })
end

-- Raw, unvalidated run access -- used internally (seeding, draws) and by
-- context.lua's schema-checking wrapper around `modes.state()`. Writing
-- through this bypasses field declaration/write_policy checks entirely, so
-- nothing outside this module and the authoring `modes` surface should hold
-- onto it.
function M.run_proxy()
    return run_proxy
end

function M.seed_random_stream_field(field_name, seed_u32)
    run_proxy[field_name] = random.derive_run_stream(seed_u32)
end

-- Draws one u32 from a declared run-local random_stream field, validating
-- its shape first (EGC-05 Done: "a fixture with a corrupted word is rejected
-- by the descriptor, not reaching a draw"). `field_spec` is the mode's own
-- field.random_stream() descriptor, supplied by the caller (context.lua),
-- which already knows it from the active mode's declaration.
function M.draw_u32(field_name, field_spec)
    local current = run_proxy[field_name]
    properties.validate_field_value(field_name, field_spec, current)
    return random.step_xoshiro128ss(run_proxy[field_name])
end

test_state_registry.register_participant("runtime.mode_store", {
    snapshot = function()
        return {
            declared_modes = declared_modes,
            declared_order = declared_order,
            is_frozen = is_frozen,
            active_mode_id = active_raw.mode_id,
            run_snapshot = (function()
                local copy = {}
                for k, v in pairs(run_raw) do
                    copy[k] = v
                end
                return copy
            end)(),
        }
    end,
    restore = function(snap)
        if not snap then return end
        declared_modes = snap.declared_modes
        declared_order = snap.declared_order
        is_frozen = snap.is_frozen
        -- Bypasses the guarded proxy deliberately (mirrors
        -- mutation_window.rollback_transaction's own raw-table writes): test
        -- isolation runs snapshot/restore/reset between spec cases, outside
        -- any command's mutation window, so going through active_proxy/
        -- run_proxy here would raise MutationWindowClosed.
        active_raw.mode_id = snap.active_mode_id
        for key in pairs(run_raw) do
            run_raw[key] = nil
        end
        for k, v in pairs(snap.run_snapshot) do
            run_raw[k] = v
        end
    end,
    reset = function()
        declared_modes = {}
        declared_order = {}
        is_frozen = false
        active_raw.mode_id = nil
        for key in pairs(run_raw) do
            run_raw[key] = nil
        end
    end,
})

return M
