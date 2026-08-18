-- Time Command Handlers for rh package (DLA-21)
-- Handles resting / waiting 1 day in the tavern, restoring stamina.

local authoring = require("core:module.authoring.context")
local location_screen = require("rh:module.presentation.location_screen")

local M = authoring.gameplay("rh")
M.id = "rh:module.gameplay.time"

M.commands["time.wait_day"] = function()
    if M.player.current_location_id ~= "rh:location.city.tavern" then
        return M.fail("location.wrong_location", {
            required_location_id = "rh:location.city.tavern",
            current_location_id = M.player.current_location_id,
        })
    end

    M.player:add_stamina(10)

    if location_screen and location_screen.build_and_publish_screen then
        location_screen.build_and_publish_screen()
    end

    return {
        ok = true,
        value = {
            stamina_gained = 10,
            current_stamina = M.player.stamina,
        },
    }
end

return M
