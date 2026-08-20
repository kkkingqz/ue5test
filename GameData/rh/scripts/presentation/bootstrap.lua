-- Session bootstrap for rh package.
-- Dispatches the NewGame command at session start so a fresh session has a
-- player and an initial location screen instead of an empty viewport.

local M = {
    id = "rh:module.presentation.bootstrap",
}

function M.start(_ctx)
    if game and game.runtime and game.runtime.dispatch_command then
        game.runtime.dispatch_command({
            command_id = "rh:command.start_game",
            args = {},
        })
    end
end

return M
