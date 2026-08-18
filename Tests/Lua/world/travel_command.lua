-- GEW-13: Location Travel Command Specification
-- Verifies the full command pipeline for core:command.location.travel:
-- validator -> handler -> Gameplay Service -> World.

local command_dispatcher = require("core:module.runtime.command_dispatcher")
local mutation_window = require("core:module.runtime.mutation_window")

local function ensure_player()
    mutation_window.execute_in_window(function()
        local player = game.instances.actors.player()
        if not player then
            local hero = game.instances.actors.create("rh:actor.character.hero", {
                stamina = 50,
                gold = 50,
            })
            game.state.meta.player_actor_id = hero.instance_id
        else
            player.stamina = 50
        end
    end)
end

return {
    successful_travel_updates_current_location = function()
        game.runtime.phase = "idle"
        ensure_player()

        -- Ensure world is at initial market location
        local world = game.instances.world()
        assert(world ~= nil, "world instance must be available")

        local dispatcher = command_dispatcher.new()

        -- First move to market to be sure
        local handler_direct = game.services.get("core:service.location")
        assert(handler_direct ~= nil, "location service must be registered")

        -- Execute travel to tavern via full dispatcher path
        -- (Set world location to market first inside a dispatcher call or service)
        local initial_loc = world.current_location_id

        local target_loc = (initial_loc == "rh:location.city.market")
            and "rh:location.city.tavern"
            or "rh:location.city.market"

        local seq = dispatcher.dispatch({
            command_id = "core:command.location.travel",
            args = {
                target_location_id = target_loc,
            },
            sequence = 701,
        })
        assert(seq == 701, "sequence must match")

        local after_world = game.instances.world()
        assert(after_world.current_location_id == target_loc,
            "world.current_location_id must be updated to target location: " .. target_loc)
        assert(game.instances.actors.player().current_location_id == target_loc,
            "player.current_location_id must be updated to target location")
        assert(game.state.world.current_location_id == nil,
            "state.world.current_location_id must not exist in state.world")

        -- Restore location to market
        dispatcher.dispatch({
            command_id = "core:command.location.travel",
            args = {
                target_location_id = "rh:location.city.market",
            },
            sequence = 702,
        })
        assert(game.instances.world().current_location_id == "rh:location.city.market",
            "location must be back at market")
    end,

    travel_to_same_location_rejected_by_validator = function()
        game.runtime.phase = "idle"
        ensure_player()

        local current_loc = game.instances.world().current_location_id

        local dispatcher = command_dispatcher.new()
        local ok, _err = pcall(function()
            dispatcher.dispatch({
                command_id = "core:command.location.travel",
                args = {
                    target_location_id = current_loc,
                },
                sequence = 703,
            })
        end)

        assert(ok, "typed refusal must not throw Lua exception")
        assert(game.instances.world().current_location_id == current_loc, "location must not change on refusal")

        local last_res = game.runtime.last_command_result
        assert(last_res ~= nil and last_res.ok == false, "command result must be ok = false")
        assert(last_res.error.code == "core:error.location.already_at_target",
            "error code must be core:error.location.already_at_target, got: " .. tostring(last_res.error.code))
    end,

    travel_to_unknown_location_rejected_by_validator = function()
        game.runtime.phase = "idle"
        ensure_player()

        local current_loc = game.instances.world().current_location_id
        local dispatcher = command_dispatcher.new()

        local ok, _err = pcall(function()
            dispatcher.dispatch({
                command_id = "core:command.location.travel",
                args = {
                    target_location_id = "rh:location.city.nonexistent",
                },
                sequence = 704,
            })
        end)

        assert(ok, "typed refusal must not throw Lua exception")
        assert(game.instances.world().current_location_id == current_loc, "location must not change on refusal")

        local last_res = game.runtime.last_command_result
        assert(last_res ~= nil and last_res.ok == false, "command result must be ok = false")
        assert(last_res.error.code == "core:error.location.not_found",
            "error code must be core:error.location.not_found, got: " .. tostring(last_res.error.code))
    end,

    travel_with_invalid_id_format_rejected_by_validator = function()
        game.runtime.phase = "idle"
        ensure_player()

        local current_loc = game.instances.world().current_location_id
        local dispatcher = command_dispatcher.new()

        local ok, _err = pcall(function()
            dispatcher.dispatch({
                command_id = "core:command.location.travel",
                args = {
                    target_location_id = "rh:item.weapon.iron_sword",
                },
                sequence = 705,
            })
        end)

        assert(ok, "typed refusal must not throw Lua exception")
        assert(game.instances.world().current_location_id == current_loc, "location must not change on refusal")

        local last_res = game.runtime.last_command_result
        assert(last_res ~= nil and last_res.ok == false, "command result must be ok = false")
        assert(last_res.error.code == "core:error.location.invalid_target",
            "error code must be core:error.location.invalid_target, got: " .. tostring(last_res.error.code))
    end,
}
