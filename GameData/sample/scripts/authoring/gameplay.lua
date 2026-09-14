-- Authoring gameplay script for sample package (TSL-16, CFC-12)
-- Implements sample travel, scout service, and binds semantic actions.

local hub = location("hub")

local function handle_travel(target)
    if type(target) == "table" and target.target ~= nil then
        target = target.target
    end
    player.current_location:require_connected(target)
    player:move_to(target)
end

commands.travel = handle_travel

-- Semantic action binding for textsystem travel
actions["textsystem:action.location.travel"] = "sample:command.travel"

-- CFC-12: Package-owned game start command
local function handle_start_game()
    local hero = instances.create("actor", {
        definition = def.actor("character.hero"),
        current_location = hub,
        gold = 100,
        stamina = 50,
        is_player = true,
        scout_count = 0,
    })
    return {
        player = hero,
    }
end

commands.start_game = handle_start_game
commands["game.start"] = handle_start_game

-- CFC-12: Scout service, command, and action
services.scout = {
    perform_scout = function()
        local roll = game.random.next_u32("sample:random_stream.scout")
        player.gold = (player.gold or 0) + 25
        player.scout_count = (player.scout_count or 0) + 1

        emit("sample:event.scout_completed", {
            scout_count = player.scout_count,
            roll = roll,
            reward = 25,
        })
        return {
            roll = roll,
            scout_count = player.scout_count,
        }
    end,
}

commands.scout = function()
    return services.scout.perform_scout()
end

actions["sample:action.scout"] = "sample:command.scout"
