local dispatcher_factory = require("core:module.runtime.command_dispatcher")
local gameplay = require("core:module.gameplay.root")
local debug_commands = require("core:module.debug.start")

local M = {}

function M.install()
    local dispatcher = dispatcher_factory.new({ gameplay, debug_commands })

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
