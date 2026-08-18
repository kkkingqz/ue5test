-- Travel Command Handler for rh package (SAS-10..13)
-- Overrides core:command.location.travel to deduct stamina on travel.

local authoring = require("core:module.authoring.context")

local M = authoring.gameplay("rh")
M.id = "rh:module.gameplay.travel"

local TRAVEL_STAMINA_COST = 5

M.commands["core:command.location.travel"] = function(target)
    M.player:require_stamina(TRAVEL_STAMINA_COST, "travel.insufficient_stamina")
    M.player.current_location:require_connected(target)

    M.player:spend_stamina(TRAVEL_STAMINA_COST)
    M.player:travel(target)
end

return M
