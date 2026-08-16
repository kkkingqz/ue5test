local dispatcher_factory = require("core:module.runtime.command_dispatcher")

local M = {
    id = "core:module.boundary.ingress",
}

function M.register(_ctx)
    local dispatcher = dispatcher_factory.new()

    game.runtime.dispatch_command = dispatcher.dispatch
    game.runtime.dispatch_semantic_input = function(input)
        assert(type(input) == "table", "semantic input must be a table")
        assert(input.session_generation == game.runtime.session_generation, "session generation mismatch")
        return dispatcher.dispatch({
            command_id = input.command_id,
            args = input.args,
            sequence = input.sequence,
            source = "semantic_input",
        })
    end
end

return M
