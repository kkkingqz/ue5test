-- TGS-06 / TGS-07: Travel Stamina Cost & Map Connectivity Specification
-- Verifies that travel consumes 5 stamina in a single mutation window,
-- and that insufficient stamina and non-connected destinations are rejected read-only.

local mutation_window = require("core:module.runtime.mutation_window")
local event_bus = require("core:module.runtime.event_bus")

local function ensure_player(initial_stamina, initial_gold)
    local player = game.instances.actors.player()
    if not player then
        local hero = game.instances.actors.create("rh:actor.character.hero", {
            stamina = initial_stamina or 20,
            gold = initial_gold or 50,
        })
        game.state.meta.player_actor_id = hero.instance_id
        return hero
    end
    if initial_stamina ~= nil then
        player.stamina = initial_stamina
    end
    if initial_gold ~= nil then
        player.gold = initial_gold
    end
    return player
end

return {
    travel_consumes_stamina_and_updates_location = function()
        event_bus.with_isolated_subscribers(function()
            event_bus.clear_published_events()

            mutation_window.execute_in_window(function()
                local player = ensure_player(20, 50)
                local world = game.instances.world()
                player.current_location_id = "rh:location.city.market"

                local received_events = {}
                game.events.subscribers.register(
                    "rh:subscriber.test_travel_tracker",
                    "core:event.location.enter",
                    function(env)
                        table.insert(received_events, env.payload)
                    end
                )

                local seq = game.runtime.dispatch_command({
                    command_id = "core:command.location.travel",
                    args = { target_location_id = "rh:location.city.tavern" },
                    sequence = 801,
                })
                assert(seq == 801, "dispatch sequence must match")

                local result = game.runtime.last_command_result
                assert(result ~= nil and result.ok == true, "travel command must succeed")
                assert(player.stamina == 15, "stamina must decrease by 5 to 15, got: " .. tostring(player.stamina))
                assert(world.current_location_id == "rh:location.city.tavern", "current location must be updated")

                -- Verify event published and delivered post-commit
                assert(#received_events == 1, "must receive 1 enter event")
                assert(received_events[1].from_location_id == "rh:location.city.market")
                assert(received_events[1].to_location_id == "rh:location.city.tavern")

                event_bus.clear_published_events()
            end)
        end)
    end,

    travel_insufficient_stamina_refused_by_validator = function()
        event_bus.with_isolated_subscribers(function()
            event_bus.clear_published_events()

            mutation_window.execute_in_window(function()
                local player = ensure_player(4, 50)
                local world = game.instances.world()
                player.current_location_id = "rh:location.city.market"

                local seq = game.runtime.dispatch_command({
                    command_id = "core:command.location.travel",
                    args = { target_location_id = "rh:location.city.tavern" },
                    sequence = 802,
                })
                assert(seq == 802)

                local result = game.runtime.last_command_result
                assert(result ~= nil and result.ok == false, "travel must be refused by validator")
                assert(result.error.code == "rh:error.travel.insufficient_stamina",
                    "error code must be insufficient_stamina, got: " .. tostring(result.error.code))
                assert(result.error.params.current_stamina == 4)
                assert(result.error.params.required_stamina == 5)

                -- State must remain unchanged
                assert(player.stamina == 4, "stamina must remain 4")
                assert(world.current_location_id == "rh:location.city.market", "location must not change")
                assert(#game.events.get_published_events() == 0, "0 events published on refusal")

                event_bus.clear_published_events()
            end)
        end)
    end,

    travel_not_connected_refused_by_validator = function()
        event_bus.with_isolated_subscribers(function()
            event_bus.clear_published_events()

            mutation_window.execute_in_window(function()
                local player = ensure_player(20, 50)
                local world = game.instances.world()
                player.current_location_id = "rh:location.city.market"

                -- market is connected only to tavern, not directly to gate
                local seq = game.runtime.dispatch_command({
                    command_id = "core:command.location.travel",
                    args = { target_location_id = "rh:location.city.gate" },
                    sequence = 803,
                })
                assert(seq == 803)

                local result = game.runtime.last_command_result
                assert(result ~= nil and result.ok == false, "travel to non-connected location must be refused")
                assert(result.error.code == "rh:error.travel.not_connected",
                    "error code must be not_connected, got: " .. tostring(result.error.code))
                assert(result.error.params.from_location_id == "rh:location.city.market")
                assert(result.error.params.to_location_id == "rh:location.city.gate")

                -- State must remain unchanged
                assert(player.stamina == 20, "stamina must remain 20")
                assert(world.current_location_id == "rh:location.city.market", "location must not change")
                assert(#game.events.get_published_events() == 0, "0 events published on refusal")

                event_bus.clear_published_events()
            end)
        end)
    end,

    travel_already_at_target_refused_by_core_validator = function()
        event_bus.with_isolated_subscribers(function()
            event_bus.clear_published_events()

            mutation_window.execute_in_window(function()
                local player = ensure_player(20, 50)
                local world = game.instances.world()
                player.current_location_id = "rh:location.city.market"

                local seq = game.runtime.dispatch_command({
                    command_id = "core:command.location.travel",
                    args = { target_location_id = "rh:location.city.market" },
                    sequence = 804,
                })
                assert(seq == 804)

                local result = game.runtime.last_command_result
                assert(result ~= nil and result.ok == false)
                assert(result.error.code == "core:error.location.already_at_target")
                assert(player.stamina == 20, "stamina must remain 20")
                assert(world.current_location_id == "rh:location.city.market")

                event_bus.clear_published_events()
            end)
        end)
    end,
}
