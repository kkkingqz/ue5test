-- Time Command Handlers for rh package (SAS-10..13)
-- Handles resting / waiting 1 day in the tavern, restoring stamina.

local authoring = require("core:module.authoring.context")
local location_screen = require("rh:module.presentation.location_screen")

local M = authoring.gameplay("rh")
M.id = "rh:module.gameplay.time"

M.commands["time.wait_day"] = function()
    local tavern = M.location("rh:location.city.tavern")
    M.player:require_location(tavern)

    M.player:add_stamina(10)

    if location_screen and location_screen.build_and_publish_screen then
        location_screen.build_and_publish_screen()
    end

    return {
        stamina_gained = 10,
        current_stamina = M.player.stamina,
    }
end

return M
