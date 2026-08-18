-- Work Command Handlers for rh package (DLA-21)
-- Handles working in the tavern (+10 gold, -2 stamina), requiring stamina > 5.

local authoring = require("core:module.authoring.context")
local location_screen = require("rh:module.presentation.location_screen")

local M = authoring.gameplay("rh")
M.id = "rh:module.gameplay.work"

M.commands["work.do_work"] = function()
    if M.player.current_location_id ~= "rh:location.city.tavern" then
        return M.fail("location.wrong_location", {
            required_location_id = "rh:location.city.tavern",
            current_location_id = M.player.current_location_id,
        })
    end

    if M.player.stamina <= 5 then
        return M.fail("work.insufficient_stamina", {
            current_stamina = M.player.stamina,
            required_stamina = 6,
        })
    end

    M.player:spend_stamina(2)
    M.player:add_gold(10)

    if location_screen and location_screen.build_and_publish_screen then
        location_screen.build_and_publish_screen()
    end

    return {
        ok = true,
        value = {
            gold_gained = 10,
            stamina_spent = 2,
            current_gold = M.player.gold,
            current_stamina = M.player.stamina,
        },
    }
end

return M
