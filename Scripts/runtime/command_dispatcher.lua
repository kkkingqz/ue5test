-- Command Dispatcher (Architecture/CommandsAndEvents.md, GEW-01, GEW-02, GEW-03, GEW-07, GEW-09, GEW-12)
-- Dispatches command requests synchronously, enforces read-only validators,
-- controls mutation window lifecycle, and manages deferred command queue.

local mutation_window = require("core:module.runtime.mutation_window")
local stable_id = require("core:module.runtime.stable_id")
local event_bus = require("core:module.runtime.event_bus")

local M = {
    id = "core:module.runtime.command_dispatcher",
}

local MAX_COMMAND_QUEUE_SIZE = 100
local deferred_command_queue = {}
local active_dispatcher = nil

local function validate_request(request)
    if type(request) ~= "table" then
        error("InvalidCommandRequest: request must be a table", 2)
    end
    if type(request.command_id) ~= "string" or not stable_id.is_kind(request.command_id, "command") then
        error("InvalidCommandRequest: command_id must be a canonical Stable ID of kind 'command', got '" .. tostring(request.command_id) .. "'", 2)
    end
    if request.args ~= nil and type(request.args) ~= "table" then
        error("InvalidCommandRequest: args must be a table if present", 2)
    end
    if request.sequence ~= nil and (type(request.sequence) ~= "number" or math.type(request.sequence) ~= "integer") then
        error("InvalidCommandRequest: sequence must be an integer if present", 2)
    end
end

local function normalize_refusal(refusal)
    if refusal == nil then
        return { code = "core:error.command.validation_refused", params = {} }
    end
    if type(refusal) ~= "table" then
        error("InvalidValidatorRefusal: refusal must be a table or nil, got " .. type(refusal), 0)
    end
    if type(refusal.code) ~= "string" or not stable_id.is_kind(refusal.code, "error") then
        error("InvalidValidatorRefusal: refusal code must be a canonical Stable ID of kind 'error', got '" .. tostring(refusal.code) .. "'", 0)
    end
    local params = refusal.params
    if params == nil then
        params = {}
    elseif type(params) ~= "table" then
        error("InvalidValidatorRefusal: refusal params must be a table if present, got " .. type(params), 0)
    end
    return { code = refusal.code, params = params }
end

local function run_validators(request)
    if not game or not game.commands or not game.commands.validators then
        return true, nil
    end

    local ctx = {
        state = game.state,
        repository = game.repository,
        payload = request.args or {},
        command_id = request.command_id,
        correlation_id = request.correlation_id,
        causation_id = request.causation_id,
        source = request.source,
    }

    for _, entry in ipairs(game.commands.validators.ordered()) do
        local validator = entry.impl
        local fn = type(validator) == "function" and validator or (type(validator) == "table" and validator.validate)
        if type(fn) == "function" then
            local allowed, refusal = fn(ctx)
            if allowed == false then
                return false, normalize_refusal(refusal)
            end
        end
    end

    return true, nil
end

function M.enqueue(request)
    if #deferred_command_queue >= MAX_COMMAND_QUEUE_SIZE then
        error("CommandQueueFull: command queue capacity of " .. tostring(MAX_COMMAND_QUEUE_SIZE) .. " exceeded", 2)
    end
    validate_request(request)

    local queued = {
        command_id = request.command_id,
        args = request.args or {},
        correlation_id = request.correlation_id,
        causation_id = request.causation_id,
        source = request.source,
    }
    table.insert(deferred_command_queue, queued)
    return true
end

function M.clear_queue()
    deferred_command_queue = {}
end

function M.get_queue_length()
    return #deferred_command_queue
end

function M.new(handlers)
    assert(type(handlers) == "table", "command handlers must be a table")

    local dispatcher = {}
    local is_dispatching = false
    local is_processing_queue = false

    function dispatcher.dispatch(request)
        if game and game.runtime then
            if game.runtime.phase == "failed" then
                error("SessionStateFailed: cannot dispatch command in failed session state", 2)
            elseif game.runtime.phase == "pumping_events" then
                error("CommandDispatchDuringEventPump: commands cannot be dispatched synchronously during event pump", 2)
            elseif game.runtime.phase == "executing_command" or is_dispatching then
                error("CommandDispatchReentrant: nested command dispatch is not supported", 2)
            end
        elseif is_dispatching then
            error("CommandDispatchReentrant: nested command dispatch is not supported", 2)
        end
        validate_request(request)

        is_dispatching = true
        if game and game.runtime then
            game.runtime.phase = "executing_command"
        end
        local command_result = nil

        local ok, err = pcall(function()
            local allowed, refusal = run_validators(request)
            if not allowed then
                -- refusal is always a normalized { code, params } table here
                -- (run_validators() only returns via normalize_refusal()).
                command_result = { ok = false, error = refusal }
                return
            end

            event_bus.begin_command_context(request)

            mutation_window.execute_in_window(function()
                for _, handler in ipairs(handlers) do
                    local result = handler.handle_command(request)
                    if result then
                        command_result = result
                        break
                    end
                end
            end)

            -- GEW-07: Deliver events only after successful commit.
            -- If command handler succeeded (result is nil/not provided, or ok ~= false, or boolean true),
            -- commit the command context and deliver pending facts.
            -- If command refused (ok == false or boolean false), discard/rollback all pending facts.
            local is_success = true
            if type(command_result) == "table" then
                is_success = (command_result.ok ~= false)
            elseif type(command_result) == "boolean" then
                is_success = command_result
            end

            if is_success then
                event_bus.commit_command_context()
            else
                event_bus.rollback_command_context()
            end
        end)
        is_dispatching = false

        if not ok then
            event_bus.discard_command_context()
            if game and game.runtime and game.runtime.phase == "executing_command" then
                game.runtime.phase = "idle"
            end
            error(err, 0)
        end

        if game and game.runtime and game.runtime.phase ~= "failed" then
            game.runtime.phase = "idle"
        end

        if request.sequence ~= nil and game and game.runtime then
            game.runtime.last_sequence = request.sequence
        end
        if game and game.runtime then
            game.runtime.command_count = (game.runtime.command_count or 0) + 1
            game.runtime.last_command_result = command_result
        end

        -- GEW-12: Process deferred commands queue sequentially
        if not is_processing_queue and #deferred_command_queue > 0 and (not game or not game.runtime or game.runtime.phase ~= "failed") then
            is_processing_queue = true
            while #deferred_command_queue > 0 do
                local next_req = table.remove(deferred_command_queue, 1)
                local next_seq = (game and game.runtime and game.runtime.last_sequence or 0) + 1
                next_req.sequence = next_seq
                dispatcher.dispatch(next_req)
            end
            is_processing_queue = false
        end

        return request.sequence
    end

    active_dispatcher = dispatcher
    return dispatcher
end

function M.register(_ctx)
    if not game then
        game = {}
    end
    if not game.commands then
        game.commands = {}
    end
    game.commands.enqueue = M.enqueue
    game.commands.clear_queue = M.clear_queue
    game.commands.get_queue_length = M.get_queue_length
end

return M
