-- GEW-14: Location Travel Events Specification
-- Verifies publication of core:event.location.leave and core:event.location.enter facts,
-- strict order (leave before enter), zero facts on refused command, and subscriber reaction.

local event_bus = require("core:module.runtime.event_bus")
local command_dispatcher = require("core:module.runtime.command_dispatcher")
local gameplay_root = require("core:module.gameplay.root")

return {
    leave_then_enter_facts_published_on_successful_travel = function()
        event_bus.clear_published_events()
        event_bus.clear_subscribers()
        game.runtime.phase = "idle"

        -- Ensure starting at market
        local world = game.instances.world()
        local dispatcher = command_dispatcher.new({ gameplay_root })

        -- Move to market first if not there
        if world.current_location_id ~= "rh:location.city.market" then
            dispatcher.dispatch({
                command_id = "core:command.location.travel",
                args = { target_location_id = "rh:location.city.market" },
                sequence = 800,
            })
        end

        event_bus.clear_published_events()

        local events_order = {}
        game.events.subscribers.register(
            "core:subscriber.test.leave_tracker",
            "core:event.location.leave",
            function(env)
                table.insert(events_order, {
                    kind = "leave",
                    from = env.payload.from_location_id,
                    to = env.payload.to_location_id,
                })
            end
        )

        game.events.subscribers.register(
            "core:subscriber.test.enter_tracker",
            "core:event.location.enter",
            function(env)
                table.insert(events_order, {
                    kind = "enter",
                    from = env.payload.from_location_id,
                    to = env.payload.to_location_id,
                })
            end
        )

        dispatcher.dispatch({
            command_id = "core:command.location.travel",
            args = { target_location_id = "rh:location.city.tavern" },
            sequence = 801,
        })

        -- Verify exactly 2 events in strict order: leave before enter
        assert(#events_order == 2, "must have received exactly 2 events, got " .. tostring(#events_order))
        assert(events_order[1].kind == "leave", "first event must be location.leave")
        assert(events_order[1].from == "rh:location.city.market", "leave from must be market")
        assert(events_order[1].to == "rh:location.city.tavern", "leave to must be tavern")

        assert(events_order[2].kind == "enter", "second event must be location.enter")
        assert(events_order[2].from == "rh:location.city.market", "enter from must be market")
        assert(events_order[2].to == "rh:location.city.tavern", "enter to must be tavern")

        -- Cleanup
        event_bus.clear_subscribers()
        event_bus.clear_published_events()
        game.runtime.phase = "idle"
    end,

    refused_travel_publishes_no_facts = function()
        event_bus.clear_published_events()
        event_bus.clear_subscribers()
        game.runtime.phase = "idle"

        local received_count = 0
        game.events.subscribers.register(
            "core:subscriber.test.all_listener",
            "core:event.location.leave",
            function(_env)
                received_count = received_count + 1
            end
        )
        game.events.subscribers.register(
            "core:subscriber.test.all_enter_listener",
            "core:event.location.enter",
            function(_env)
                received_count = received_count + 1
            end
        )

        local current_loc = game.instances.world().current_location_id
        local dispatcher = command_dispatcher.new({ gameplay_root })

        -- Attempt invalid travel: travel to current location
        dispatcher.dispatch({
            command_id = "core:command.location.travel",
            args = { target_location_id = current_loc },
            sequence = 802,
        })

        assert(received_count == 0, "no events should be delivered when validator refuses command")
        local published = game.events.get_published_events()
        assert(#published == 0, "published events buffer must be empty on refusal")

        -- Cleanup
        event_bus.clear_subscribers()
        event_bus.clear_published_events()
        game.runtime.phase = "idle"
    end,

    subscriber_reacts_only_to_specific_condition = function()
        event_bus.clear_published_events()
        event_bus.clear_subscribers()
        game.runtime.phase = "idle"

        -- Reset to market
        local dispatcher = command_dispatcher.new({ gameplay_root })
        if game.instances.world().current_location_id ~= "rh:location.city.market" then
            dispatcher.dispatch({
                command_id = "core:command.location.travel",
                args = { target_location_id = "rh:location.city.market" },
                sequence = 803,
            })
        end
        event_bus.clear_published_events()

        local tavern_entered = false
        local forest_entered = false

        -- Subscriber 1: checks tavern
        game.events.subscribers.register(
            "core:subscriber.test.tavern_checker",
            "core:event.location.enter",
            function(env)
                if env.payload.to_location_id == "rh:location.city.tavern" then
                    tavern_entered = true
                end
            end
        )

        -- Subscriber 2: checks forest
        game.events.subscribers.register(
            "core:subscriber.test.forest_checker",
            "core:event.location.enter",
            function(env)
                if env.payload.to_location_id == "rh:location.wild.forest" then
                    forest_entered = true
                end
            end
        )

        dispatcher.dispatch({
            command_id = "core:command.location.travel",
            args = { target_location_id = "rh:location.city.tavern" },
            sequence = 804,
        })

        assert(tavern_entered == true, "tavern subscriber must have reacted to tavern enter")
        assert(forest_entered == false, "forest subscriber must NOT have reacted")

        -- Cleanup
        event_bus.clear_subscribers()
        event_bus.clear_published_events()
        game.runtime.phase = "idle"
    end,

    subscriber_reaction_enqueues_deferred_command = function()
        event_bus.clear_published_events()
        event_bus.clear_subscribers()
        game.commands.clear_queue()
        game.runtime.phase = "idle"

        local dispatcher = command_dispatcher.new({ gameplay_root })

        -- Move to market first if not there
        if game.instances.world().current_location_id ~= "rh:location.city.market" then
            dispatcher.dispatch({
                command_id = "core:command.location.travel",
                args = { target_location_id = "rh:location.city.market" },
                sequence = 810,
            })
        end
        event_bus.clear_published_events()

        local trace = {}

        game.events.subscribers.register(
            "core:subscriber.test.tavern_roundtrip",
            "core:event.location.enter",
            function(env)
                table.insert(trace, "entered:" .. env.payload.to_location_id)
                if env.payload.to_location_id == "rh:location.city.tavern" then
                    -- Enqueue deferred return travel back to market
                    game.commands.enqueue({
                        command_id = "core:command.location.travel",
                        args = { target_location_id = "rh:location.city.market" },
                    })
                end
            end
        )

        dispatcher.dispatch({
            command_id = "core:command.location.travel",
            args = { target_location_id = "rh:location.city.tavern" },
            sequence = 811,
        })

        -- Verify trace: entered tavern -> deferred command executed -> entered market
        assert(#trace == 2, "must have 2 enter events, got " .. tostring(#trace))
        assert(trace[1] == "entered:rh:location.city.tavern", "first enter tavern")
        assert(trace[2] == "entered:rh:location.city.market", "second enter market via deferred command")

        assert(game.instances.world().current_location_id == "rh:location.city.market",
            "final location must be market")

        -- Cleanup
        event_bus.clear_subscribers()
        event_bus.clear_published_events()
        game.commands.clear_queue()
        game.runtime.phase = "idle"
    end,
}
