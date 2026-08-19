-- TSL-09 / TSL-17: Location Travel Command Specification (TextSystem tier)
-- Verifies the travel command pipeline using sample package entities:
-- connectivity check -> player:move_to -> World.

local command_dispatcher = require("core:module.runtime.command_dispatcher")
local mutation_window = require("core:module.runtime.mutation_window")

local function ensure_player()
    mutation_window.execute_in_window(function()
        local player = game.instances.actors.player()
        if not player then
            local hero = game.instances.actors.create("sample:actor.character.hero", {})
            game.state.meta.player_actor_id = hero.instance_id
        end
    end)
end

return {
    successful_travel_updates_current_location = function()
        game.runtime.phase = "idle"
        ensure_player()

        local world = game.instances.world()
        assert(world ~= nil, "world instance must be available")

        local dispatcher = command_dispatcher.new()

        -- Start at hub
        mutation_window.execute_in_window(function()
            game.instances.actors.player().current_location = "sample:location.hub"
        end)

        local seq = dispatcher.dispatch({
            command_id = "sample:command.travel",
            args = {
                target_location_id = "sample:location.east",
            },
            sequence = 701,
        })
        assert(seq == 701, "sequence must match")

        local after_world = game.instances.world()
        assert(after_world.current_location_id == "sample:location.east",
            "world.current_location_id must be updated to target location")
        assert(game.instances.actors.player().current_location == "sample:location.east" or game.instances.actors.player().current_location.id == "sample:location.east",
            "player.current_location must be updated to target location")

        -- Restore location to hub
        dispatcher.dispatch({
            command_id = "sample:command.travel",
            args = {
                target_location_id = "sample:location.hub",
            },
            sequence = 702,
        })
        assert(game.instances.world().current_location_id == "sample:location.hub",
            "location must be back at hub")
    end,

    travel_to_unconnected_location_rejected = function()
        game.runtime.phase = "idle"
        ensure_player()

        -- East is connected only to hub, not to west
        mutation_window.execute_in_window(function()
            game.instances.actors.player().current_location = "sample:location.east"
        end)

        local dispatcher = command_dispatcher.new()
        local ok, _err = pcall(function()
            dispatcher.dispatch({
                command_id = "sample:command.travel",
                args = {
                    target_location_id = "sample:location.west",
                },
                sequence = 703,
            })
        end)

        assert(ok, "typed refusal must not throw Lua exception")
        assert(game.instances.world().current_location_id == "sample:location.east", "location must not change on refusal")

        local last_res = game.runtime.last_command_result
        assert(last_res ~= nil and last_res.ok == false, "command result must be ok = false")
        assert(last_res.error.code == "textsystem:error.travel.not_connected",
            "error code must be textsystem:error.travel.not_connected, got: " .. tostring(last_res.error and last_res.error.code))
        assert(last_res.error.params.from_location == "sample:location.east")
        assert(last_res.error.params.to_location == "sample:location.west")
    end,
}
