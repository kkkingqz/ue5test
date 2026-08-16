-- GEW-14: Location Travel Events Specification
-- Verifies publication of core:event.location.leave and core:event.location.enter facts,
-- strict order (leave before enter), zero facts on refused command, and subscriber reaction.

local event_bus = require("core:module.runtime.event_bus")
local command_dispatcher = require("core:module.runtime.command_dispatcher")

return {
    leave_then_enter_facts_published_on_successful_travel = function()
        event_bus.clear_published_events()
        event_bus.clear_subscribers()
        game.runtime.phase = "idle"

        -- Ensure starting at market
        local world = game.instances.world()
        local dispatcher = command_dispatcher.new()

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

        -- Verify exactly 2 events published in strict order: leave -> enter
        local published = game.events.get_published_events()
        assert(#published == 2, "exactly 2 facts must be published, got " .. tostring(#published))
        assert(published[1].event_id == "core:event.location.leave", "1st event must be leave")
        assert(published[2].event_id == "core:event.location.enter", "2nd event must be enter")

        -- Verify payload contents
        assert(published[1].payload.from_location_id == "rh:location.city.market", "leave from market")
        assert(published[1].payload.to_location_id == "rh:location.city.tavern", "leave to tavern")
        assert(published[2].payload.from_location_id == "rh:location.city.market", "enter from market")
        assert(published[2].payload.to_location_id == "rh:location.city.tavern", "enter to tavern")

        -- Verify subscribers received both events in exact order
        assert(#events_order == 2, "subscribers must have run twice, got " .. tostring(#events_order))
        assert(events_order[1].kind == "leave", "leave subscriber ran first")
        assert(events_order[2].kind == "enter", "enter subscriber ran second")

        -- Cleanup
        event_bus.clear_subscribers()
        event_bus.clear_published_events()
        game.runtime.phase = "idle"
    end,

    refused_travel_publishes_no_facts = function()
        event_bus.clear_published_events()
        event_bus.clear_subscribers()
        game.runtime.phase = "idle"

        local world = game.instances.world()
        local current_loc = world.current_location_id

        local subscriber_invoked = false
        game.events.subscribers.register(
            "core:subscriber.test.guard",
            "core:event.location.leave",
            function(_env)
                subscriber_invoked = true
            end
        )

        local dispatcher = command_dispatcher.new()
        -- Attempt to travel to current location (rejected by validator)
        local ok, _ = pcall(function()
            dispatcher.dispatch({
                command_id = "core:command.location.travel",
                args = { target_location_id = current_loc },
                sequence = 802,
            })
        end)

        assert(ok, "typed refusal must not throw Lua exception")
        assert(subscriber_invoked == false, "subscribers must NOT be invoked on refused travel")
        local published = game.events.get_published_events()
        assert(#published == 0, "0 events must be published on refused travel, got " .. tostring(#published))

        -- Cleanup
        event_bus.clear_subscribers()
        event_bus.clear_published_events()
        game.runtime.phase = "idle"
    end,

    subscriber_reacts_only_to_specific_condition = function()
        event_bus.clear_published_events()
        event_bus.clear_subscribers()
        game.runtime.phase = "idle"

        local tavern_entered = false
        local forest_entered = false

        game.events.subscribers.register(
            "core:subscriber.test.tavern_checker",
            "core:event.location.enter",
            function(env)
                if env.payload.to_location_id == "rh:location.city.tavern" then
                    tavern_entered = true
                elseif env.payload.to_location_id == "rh:location.wilderness.forest" then
                    forest_entered = true
                end
            end
        )

        local dispatcher = command_dispatcher.new()
        -- Move to market first
        dispatcher.dispatch({
            command_id = "core:command.location.travel",
            args = { target_location_id = "rh:location.city.market" },
            sequence = 803,
        })
        event_bus.clear_published_events()

        -- Now travel to tavern
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

        local dispatcher = command_dispatcher.new()

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
