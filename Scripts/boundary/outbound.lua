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

function M.begin_command_context()
    staged_requests = {}
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
end

function M.rollback_command_context()
    staged_requests = nil
end

function M.discard_command_context()
    staged_requests = nil
end

function M.take_pending_requests()
    if #committed_requests == 0 then
        return nil
    end
    local result = committed_requests
    committed_requests = {}
    return result
end

function M.clear_queue()
    committed_requests = {}
    staged_requests = nil
end

function M.get_queue_length()
    return #committed_requests
end

function M.get_staged_length()
    return staged_requests and #staged_requests or 0
end

function M.register(_ctx)
    command_dispatcher.set_outbound(M)
    game.ui.take_pending_screen = screens.take_pending
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
