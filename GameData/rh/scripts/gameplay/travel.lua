-- Travel Command Handler for rh package (DLA-21)
-- Overrides core:command.location.travel to deduct stamina on travel.
-- Enforces stamina and map connectivity preconditions via fail() before state mutation.

local authoring = require("core:module.authoring.context")

local M = authoring.gameplay("rh")
M.id = "rh:module.gameplay.travel"

local TRAVEL_STAMINA_COST = 5

M.commands["core:command.location.travel"] = function(args)
    local target_location_id = (type(args) == "table") and (args.target_location_id or args.location_id) or args
    local current_loc = M.world.current_location
    if type(current_loc) == "string" then
        current_loc = M.location(current_loc)
    end

    if M.player.stamina < TRAVEL_STAMINA_COST then
        return M.fail("travel.insufficient_stamina", {
            required_stamina = TRAVEL_STAMINA_COST,
            current_stamina = M.player.stamina,
        })
    end

    if current_loc and not current_loc:is_connected(target_location_id) then
        return M.fail("travel.not_connected", {
            from_location_id = current_loc.id,
            to_location_id = target_location_id,
        })
    end

    M.player:spend_stamina(TRAVEL_STAMINA_COST)
    return M.player:travel(target_location_id)
end

return M
