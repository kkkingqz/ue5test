local screens = require("core:module.presentation.screen_requests")

local M = {}

function M.install()
    game.ui.take_pending_screen = screens.take_pending
end

return M
