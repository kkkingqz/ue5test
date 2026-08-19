-- Authoring gameplay script for sample package (TSL-16)
-- Implements sample travel without stamina requirements and binds semantic action.

local function handle_travel(target)
    player.current_location:require_connected(target)
    player:move_to(target)
end

commands.travel = handle_travel

-- Semantic action binding for textsystem travel
actions["textsystem:action.location.travel"] = "sample:command.travel"
