local screens = require("core:module.presentation.screen_requests")
local state_hasher = require("core:module.runtime.state_hasher")

local M = {
    id = "core:module.boundary.outbound",
}

function M.register(_ctx)
    game.ui.take_pending_screen = screens.take_pending
    game.runtime.get_canonical_state_hash = function()
        if type(game.state) ~= "table" then
            return ""
        end
        return state_hasher.hash_state(game.state)
    end
end

return M
