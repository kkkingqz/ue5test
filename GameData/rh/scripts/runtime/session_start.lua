-- RH session bootstrap: create the initial gameplay state through the
-- registered command path. Presentation resolves from the resulting state.

require("rh:module.authoring.gameplay")
local location_presenter = require("textsystem:module.presentation.location_presenter")
require("core:module.runtime.presentation_source")

local M = {
    id = "rh:module.runtime.session_start",
}

function M.start(_ctx)
    if game.instances.actors.player() ~= nil then
        return
    end

    game.runtime.dispatch_command({
        command_id = "rh:command.start_game",
        args = {},
        source = "session_start",
    })
end

return M
