-- TSL-07 / TSL-17: Location Travel Events Specification (TextSystem tier)
-- Verifies publication of textsystem:event.location.leave and textsystem:event.location.enter facts,
-- strict order (leave before enter), zero facts on refused command, and subscriber reaction in sample.

local event_bus = require("core:module.runtime.event_bus")
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
    leave_then_enter_facts_published_on_successful_travel = function()
        event_bus.with_isolated_subscribers(function()
            event_bus.clear_published_events()
            game.runtime.phase = "idle"
            ensure_player()

            -- Ensure starting at hub
            local world = game.instances.world()
            local dispatcher = command_dispatcher.new()

            -- Move to hub first if not there
            if world.current_location_id ~= "sample:location.hub" then
                dispatcher.dispatch({
                    command_id = "sample:command.travel",
                    args = { target_location_id = "sample:location.hub" },
                    sequence = 800,
                })
            end

            event_bus.clear_published_events()

            local events_order = {}
            game.events.subscribers.register(
                "core:subscriber.test.leave_tracker",
                "textsystem:event.location.leave",
                function(env)
                    table.insert(events_order, {
                        kind = "leave",
                        from = env.payload.from_location,
                        to = env.payload.to_location,
                    })
                end
            )

            game.events.subscribers.register(
                "core:subscriber.test.enter_tracker",
                "textsystem:event.location.enter",
                function(env)
                    table.insert(events_order, {
                        kind = "enter",
                        from = env.payload.from_location,
                        to = env.payload.to_location,
                    })
                end
            )

            dispatcher.dispatch({
                command_id = "sample:command.travel",
                args = { target_location_id = "sample:location.east" },
                sequence = 801,
            })

            -- Verify exactly 2 events published in strict order: leave -> enter
            local published = game.events.get_published_events()
            assert(#published == 2, "exactly 2 facts must be published, got " .. tostring(#published))
            assert(published[1].event_id == "textsystem:event.location.leave", "1st event must be leave")
            assert(published[2].event_id == "textsystem:event.location.enter", "2nd event must be enter")

            -- Verify payload contents
            assert(published[1].payload.from_location == "sample:location.hub", "leave from hub")
            assert(published[1].payload.to_location == "sample:location.east", "leave to east")
            assert(published[2].payload.from_location == "sample:location.hub", "enter from hub")
            assert(published[2].payload.to_location == "sample:location.east", "enter to east")

            -- Verify subscriber delivery order
            assert(#events_order == 2, "both subscribers must be invoked")
            assert(events_order[1].kind == "leave", "leave subscriber first")
            assert(events_order[2].kind == "enter", "enter subscriber second")

            -- Cleanup
            event_bus.clear_published_events()
            game.runtime.phase = "idle"
        end)
    end,

    no_facts_published_on_refused_travel_command = function()
        event_bus.with_isolated_subscribers(function()
            event_bus.clear_published_events()
            game.runtime.phase = "idle"
            ensure_player()

            mutation_window.execute_in_window(function()
                game.instances.actors.player().current_location = "sample:location.east"
            end)

            event_bus.clear_published_events()

            local subscriber_invoked = false
            game.events.subscribers.register(
                "core:subscriber.test.fail_tracker",
                "textsystem:event.location.leave",
                function(_env)
                    subscriber_invoked = true
                end
            )

            local dispatcher = command_dispatcher.new()
            -- Dispatch travel to non-connected west from east -> validator will refuse
            dispatcher.dispatch({
                command_id = "sample:command.travel",
                args = { target_location_id = "sample:location.west" },
                sequence = 802,
            })

            local published = game.events.get_published_events()
            assert(#published == 0, "no facts must be published when command is refused, got " .. tostring(#published))
            assert(subscriber_invoked == false, "subscriber must NOT be invoked on refused command")

            -- Cleanup
            event_bus.clear_published_events()
            game.runtime.phase = "idle"
        end)
    end,

    subscriber_reacts_only_to_specific_condition = function()
        event_bus.with_isolated_subscribers(function()
            event_bus.clear_published_events()
            game.runtime.phase = "idle"
            ensure_player()

            local east_entered = false
            local west_entered = false

            game.events.subscribers.register(
                "core:subscriber.test.east_checker",
                "textsystem:event.location.enter",
                function(env)
                    if env.payload.to_location == "sample:location.east" then
                        east_entered = true
                    elseif env.payload.to_location == "sample:location.west" then
                        west_entered = true
                    end
                end
            )

            local dispatcher = command_dispatcher.new()
            -- Move to hub first
            dispatcher.dispatch({
                command_id = "sample:command.travel",
                args = { target_location_id = "sample:location.hub" },
                sequence = 803,
            })
            event_bus.clear_published_events()

            -- Now travel to east
            dispatcher.dispatch({
                command_id = "sample:command.travel",
                args = { target_location_id = "sample:location.east" },
                sequence = 804,
            })

            assert(east_entered == true, "east subscriber must have reacted to east enter")
            assert(west_entered == false, "west subscriber must NOT have reacted")

            -- Cleanup
            event_bus.clear_published_events()
            game.runtime.phase = "idle"
        end)
    end,

    subscriber_reaction_enqueues_deferred_command = function()
        event_bus.with_isolated_subscribers(function()
            event_bus.clear_published_events()
            game.commands.clear_queue()
            game.runtime.phase = "idle"
            ensure_player()

            local dispatcher = command_dispatcher.new()

            -- Move to hub first if not there
            if game.instances.world().current_location_id ~= "sample:location.hub" then
                dispatcher.dispatch({
                    command_id = "sample:command.travel",
                    args = { target_location_id = "sample:location.hub" },
                    sequence = 810,
                })
            end
            event_bus.clear_published_events()

            local trace = {}

            game.events.subscribers.register(
                "core:subscriber.test.hub_roundtrip",
                "textsystem:event.location.enter",
                function(env)
                    table.insert(trace, "entered:" .. env.payload.to_location)
                    if env.payload.to_location == "sample:location.east" then
                        -- Enqueue deferred return travel back to hub
                        game.commands.enqueue({
                            command_id = "sample:command.travel",
                            args = { target_location_id = "sample:location.hub" },
                        })
                    end
                end
            )

            dispatcher.dispatch({
                command_id = "sample:command.travel",
                args = { target_location_id = "sample:location.east" },
                sequence = 811,
            })

            -- Verify trace: entered east -> deferred command executed -> entered hub
            assert(#trace == 2, "must have 2 enter events, got " .. tostring(#trace))
            assert(trace[1] == "entered:sample:location.east", "first enter east")
            assert(trace[2] == "entered:sample:location.hub", "second enter hub via deferred command")

            assert(game.instances.world().current_location_id == "sample:location.hub",
                "final location must be hub")

            -- Cleanup
            event_bus.clear_published_events()
            game.commands.clear_queue()
            game.runtime.phase = "idle"
        end)
    end,
}
