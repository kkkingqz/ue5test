-- Time Command Handlers for rh package (SAS-10..13)
-- Handles resting / waiting 1 day in the tavern, restoring stamina.

local authoring = require("core:module.authoring.context")

local M = authoring.gameplay("rh")
M.id = "rh:module.gameplay.time"

M.commands["time.wait_day"] = function()
    local tavern = M.location("rh:location.city.tavern")
    M.player:require_location(tavern)

    M.player:add_stamina(10)

    return {
        stamina_gained = 10,
        current_stamina = M.player.stamina,
    }
end

return M
