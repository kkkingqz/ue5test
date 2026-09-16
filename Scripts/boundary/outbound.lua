local screens = require("core:module.presentation.screen_requests")
local state_hasher = require("core:module.runtime.state_hasher")
local save = require("core:module.runtime.save")
local load = require("core:module.runtime.load")
local command_dispatcher = require("core:module.runtime.command_dispatcher")

local M = {
    id = "core:module.boundary.outbound",
}

local MAX_QUEUE_SIZE = 64
local committed_requests = {}
local staged_requests = nil

-- PEP-03 (ADR-0047): one-shot presentation effects. Separate queue from
-- committed_requests/staged_requests above (save/load control requests are a
-- different concept), but the SAME staged-during-command/committed-on-success
-- lifecycle -- an effect published mid-command must not survive that command's own
-- rollback (ADR-0041), same as a save/load request must not.
local committed_effects = {}
local staged_effects = nil

local function is_valid_slot_id(slot_id)
    if type(slot_id) ~= "string" or slot_id == "" then
        return false
    end
    return string.match(slot_id, "^[a-z][a-z0-9_]*$") ~= nil
end

function M.request_save(slot_id)
    if not is_valid_slot_id(slot_id) then
        return false, "InvalidSaveSlotId"
    end

    local req = {
        kind = "save",
        slot_id = slot_id,
    }

    if staged_requests ~= nil then
        if #staged_requests >= MAX_QUEUE_SIZE then
            return false, "ControlQueueFull"
        end
        table.insert(staged_requests, req)
    else
        if #committed_requests >= MAX_QUEUE_SIZE then
            return false, "ControlQueueFull"
        end
        table.insert(committed_requests, req)
    end
    return true
end

function M.request_load(slot_id, revision)
    if not is_valid_slot_id(slot_id) then
        return false, "InvalidSaveSlotId"
    end
    if revision ~= "current" and revision ~= "previous" then
        return false, "InvalidSaveSlotRevision"
    end

    local req = {
        kind = "load",
        slot_id = slot_id,
        revision = revision,
    }

    if staged_requests ~= nil then
        if #staged_requests >= MAX_QUEUE_SIZE then
            return false, "ControlQueueFull"
        end
        table.insert(staged_requests, req)
    else
        if #committed_requests >= MAX_QUEUE_SIZE then
            return false, "ControlQueueFull"
        end
        table.insert(committed_requests, req)
    end
    return true
end

-- PEP-01/ADR-0047: identity carries no source field, but Lua-published effects still
-- need the SAME schema-validation-and-deep-copy discipline publish_snapshot's own
-- binding already has. Deep validation (effect_id grammar, target/args shape, unknown
-- fields) happens host-side in ReadPresentationEffect when the effect is actually
-- pulled -- this is only the light shape check every publish_* helper in this file
-- performs before queuing, matching request_save/request_load's own is_valid_slot_id
-- precedent.
function M.publish_effect(effect)
    if type(effect) ~= "table" then
        return false, "InvalidPresentationEffect"
    end
    if type(effect.effect_id) ~= "string" or effect.effect_id == "" then
        return false, "InvalidPresentationEffect"
    end

    if staged_effects ~= nil then
        if #staged_effects >= MAX_QUEUE_SIZE then
            return false, "EffectQueueFull"
        end
        table.insert(staged_effects, effect)
    else
        if #committed_effects >= MAX_QUEUE_SIZE then
            return false, "EffectQueueFull"
        end
        table.insert(committed_effects, effect)
    end
    return true
end

function M.begin_command_context()
    staged_requests = {}
    staged_effects = {}
end

function M.commit_command_context()
    if staged_requests ~= nil then
        for _, req in ipairs(staged_requests) do
            if #committed_requests < MAX_QUEUE_SIZE then
                table.insert(committed_requests, req)
            end
        end
        staged_requests = nil
    end
    if staged_effects ~= nil then
        for _, effect in ipairs(staged_effects) do
            if #committed_effects < MAX_QUEUE_SIZE then
                table.insert(committed_effects, effect)
            end
        end
        staged_effects = nil
    end
end

function M.rollback_command_context()
    staged_requests = nil
    staged_effects = nil
end

function M.discard_command_context()
    staged_requests = nil
    staged_effects = nil
end

function M.take_pending_requests()
    if #committed_requests == 0 then
        return nil
    end
    local result = committed_requests
    committed_requests = {}
    return result
end

-- PEP-03: reconciler is never invoked here -- this is a pure Lua table hand-off,
-- mirroring screens.take_pending()/take_pending_requests() above exactly. Sequence
-- assignment happens host-side (GV2RuntimeSession::StampAndEnqueueEffect), never here.
function M.take_pending_effects()
    if #committed_effects == 0 then
        return nil
    end
    local result = committed_effects
    committed_effects = {}
    return result
end

function M.clear_queue()
    committed_requests = {}
    staged_requests = nil
    committed_effects = {}
    staged_effects = nil
end

function M.get_queue_length()
    return #committed_requests
end

function M.get_staged_length()
    return staged_requests and #staged_requests or 0
end

function M.get_effect_queue_length()
    return #committed_effects
end

function M.get_staged_effect_length()
    return staged_effects and #staged_effects or 0
end

function M.register(_ctx)
    command_dispatcher.set_outbound(M)
    game.ui.take_pending_screen = screens.take_pending
    game.ui.publish_effect = M.publish_effect
    game.ui.take_pending_effects = M.take_pending_effects
    game.runtime.get_canonical_state_hash = function()
        if type(game.state) ~= "table" then
            return ""
        end
        return state_hasher.hash_state(game.state)
    end

    if game.bridge then
        game.bridge.request_save = M.request_save
        game.bridge.request_load = M.request_load
        game.bridge.take_pending_requests = M.take_pending_requests
    end

    game.runtime.save_to_slot = save.save
    game.runtime.preflight_save_bytes = load.preflight_bytes
end

return M
