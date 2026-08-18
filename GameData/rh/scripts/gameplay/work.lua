-- Work Command Handlers for rh package (SAS-10..13)
-- Handles working in the tavern (+10 gold, -2 stamina), requiring stamina > 5.

local authoring = require("core:module.authoring.context")
local location_screen = require("rh:module.presentation.location_screen")

local M = authoring.gameplay("rh")
M.id = "rh:module.gameplay.work"

M.commands["work.do_work"] = function()
    local tavern = M.location("rh:location.city.tavern")
    M.player:require_location(tavern)
    M.player:require_stamina(6, "work.insufficient_stamina")

    M.player:spend_stamina(2)
    M.player:add_gold(10)

    if location_screen and location_screen.build_and_publish_screen then
        location_screen.build_and_publish_screen()
    end

    return {
        gold_gained = 10,
        stamina_spent = 2,
        current_gold = M.player.gold,
        current_stamina = M.player.stamina,
    }
end

return M
