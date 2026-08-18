-- TSL-09: Location Travel Command Specification
-- Verifies the full command pipeline for rh:command.travel:
-- connectivity check -> stamina check -> spend stamina -> player:move_to -> World.

local command_dispatcher = require("core:module.runtime.command_dispatcher")
local mutation_window = require("core:module.runtime.mutation_window")

local function ensure_player(stamina)
    mutation_window.execute_in_window(function()
        local player = game.instances.actors.player()
        if not player then
            local hero = game.instances.actors.create("rh:actor.character.hero", {
                stamina = stamina or 50,
                gold = 50,
            })
            game.state.meta.player_actor_id = hero.instance_id
        else
            player.stamina = stamina or 50
        end
    end)
end

return {
    successful_travel_updates_current_location = function()
        game.runtime.phase = "idle"
        ensure_player(50)

        local world = game.instances.world()
        assert(world ~= nil, "world instance must be available")

        local dispatcher = command_dispatcher.new()

        -- Start at market
        mutation_window.execute_in_window(function()
            game.instances.actors.player().current_location = "rh:location.city.market"
        end)

        local seq = dispatcher.dispatch({
            command_id = "rh:command.travel",
            args = {
                target_location_id = "rh:location.city.tavern",
            },
            sequence = 701,
        })
        assert(seq == 701, "sequence must match")

        local after_world = game.instances.world()
        assert(after_world.current_location_id == "rh:location.city.tavern",
            "world.current_location_id must be updated to target location")
        assert(game.instances.actors.player().current_location == "rh:location.city.tavern" or game.instances.actors.player().current_location.id == "rh:location.city.tavern",
            "player.current_location must be updated to target location")

        -- Restore location to market
        dispatcher.dispatch({
            command_id = "rh:command.travel",
            args = {
                target_location_id = "rh:location.city.market",
            },
            sequence = 702,
        })
        assert(game.instances.world().current_location_id == "rh:location.city.market",
            "location must be back at market")
    end,

    travel_to_unconnected_location_rejected_by_validator = function()
        game.runtime.phase = "idle"
        ensure_player(50)

        mutation_window.execute_in_window(function()
            game.instances.actors.player().current_location = "rh:location.city.market"
        end)

        local dispatcher = command_dispatcher.new()
        local ok, _err = pcall(function()
            dispatcher.dispatch({
                command_id = "rh:command.travel",
                args = {
                    target_location_id = "rh:location.city.gate",
                },
                sequence = 703,
            })
        end)

        assert(ok, "typed refusal must not throw Lua exception")
        assert(game.instances.world().current_location_id == "rh:location.city.market", "location must not change on refusal")

        local last_res = game.runtime.last_command_result
        assert(last_res ~= nil and last_res.ok == false, "command result must be ok = false")
        assert(last_res.error.code == "rh:error.travel.not_connected",
            "error code must be rh:error.travel.not_connected, got: " .. tostring(last_res.error.code))
        assert(last_res.error.params.from_location == "rh:location.city.market")
        assert(last_res.error.params.to_location == "rh:location.city.gate")
    end,

    travel_insufficient_stamina_rejected_by_validator = function()
        game.runtime.phase = "idle"
        ensure_player(2)

        mutation_window.execute_in_window(function()
            game.instances.actors.player().current_location = "rh:location.city.market"
        end)

        local dispatcher = command_dispatcher.new()
        local ok, _err = pcall(function()
            dispatcher.dispatch({
                command_id = "rh:command.travel",
                args = {
                    target_location_id = "rh:location.city.tavern",
                },
                sequence = 704,
            })
        end)

        assert(ok, "typed refusal must not throw Lua exception")
        assert(game.instances.world().current_location_id == "rh:location.city.market", "location must not change on refusal")

        local last_res = game.runtime.last_command_result
        assert(last_res ~= nil and last_res.ok == false, "command result must be ok = false")
        assert(last_res.error.code == "rh:error.travel.insufficient_stamina",
            "error code must be rh:error.travel.insufficient_stamina, got: " .. tostring(last_res.error.code))
    end,
}
