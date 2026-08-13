local M = {}

local function validate_request(request)
    assert(type(request) == "table", "command request must be a table")
    assert(type(request.command_id) == "string" and request.command_id ~= "", "command_id is required")
    assert(type(request.args) == "table", "args table is required")
    assert(type(request.sequence) == "number", "sequence is required")
end

function M.new(handlers)
    assert(type(handlers) == "table", "command handlers must be a table")

    local dispatcher = {}

    function dispatcher.dispatch(request)
        validate_request(request)

        for _, handler in ipairs(handlers) do
            if handler.handle_command(request) then
                break
            end
        end

        game.runtime.last_sequence = request.sequence
        game.runtime.command_count = (game.runtime.command_count or 0) + 1
        return request.sequence
    end

    return dispatcher
end

return M
