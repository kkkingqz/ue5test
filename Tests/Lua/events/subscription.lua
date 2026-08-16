local subscriber_registry = require("core:module.runtime.subscriber_registry")
local event_bus = require("core:module.runtime.event_bus")
local command_dispatcher = require("core:module.runtime.command_dispatcher")
local handler_registry = require("core:module.runtime.handler_registry")

return {
    subscribing_to_event_id_succeeds_and_runs_handler = function()
        event_bus.clear_published_events()
        event_bus.clear_subscribers()
        game.runtime.phase = "idle"

        local received_event = nil

        game.events.subscribers.register(
            "core:subscriber.test.location_tracker",
            "core:event.location.enter",
            function(env)
                -- Imperative condition check on payload fields
                if env.payload.to_location_id == "core:location.city.tavern" then
                    received_event = env
                end
            end
        )

        local reg = handler_registry.create_registry()
        reg.register("core:command.test.gew10_travel", function(_request)
            game.events.enqueue({
                event_id = "core:event.location.enter",
                payload = {
                    from_location_id = "core:location.city.market",
                    to_location_id = "core:location.city.tavern",
                },
            })
            return { ok = true }
        end)

        local dispatcher = command_dispatcher.new(reg)
        dispatcher.dispatch({
            command_id = "core:command.test.gew10_travel",
            args = {},
            sequence = 401,
        })

        assert(received_event ~= nil, "subscriber must have received and matched event")
        assert(received_event.event_id == "core:event.location.enter", "event_id must match")
        assert(received_event.payload.to_location_id == "core:location.city.tavern", "payload field must match")
    end,

    subscribing_to_invalid_event_id_rejected = function()
        local fresh = subscriber_registry.create_registry()

        local ok_bad_kind = pcall(function()
            fresh.register(
                "core:subscriber.test.bad_event",
                "core:command.location.travel",
                function() end
            )
        end)
        assert(not ok_bad_kind, "registering with command ID instead of event ID must fail")

        local ok_malformed = pcall(function()
            fresh.register(
                "core:subscriber.test.bad_event",
                "not_a_valid_id",
                function() end
            )
        end)
        assert(not ok_malformed, "registering with malformed event ID must fail")
    end,

    subscribing_with_invalid_subscriber_id_rejected = function()
        local fresh = subscriber_registry.create_registry()

        local ok_bad_kind = pcall(function()
            fresh.register(
                "core:event.location.enter",
                "core:event.location.enter",
                function() end
            )
        end)
        assert(not ok_bad_kind, "subscriber ID must have kind 'subscriber'")
    end,

    duplicate_subscriber_id_rejected = function()
        local fresh = subscriber_registry.create_registry()

        fresh.register(
            "core:subscriber.test.duplicate",
            "core:event.location.enter",
            function() end
        )

        local ok_dup = pcall(function()
            fresh.register(
                "core:subscriber.test.duplicate",
                "core:event.location.enter",
                function() end
            )
        end)
        assert(not ok_dup, "registering duplicate subscriber ID must throw an error")
    end,

    registration_after_freeze_rejected = function()
        local fresh = subscriber_registry.create_registry()
        fresh.freeze()
        assert(fresh.is_frozen(), "registry must report frozen")

        local ok_late = pcall(function()
            fresh.register(
                "core:subscriber.test.late",
                "core:event.location.enter",
                function() end
            )
        end)
        assert(not ok_late, "registration after freeze must be rejected")
    end,

    direct_assignment_to_subscribers_table_rejected = function()
        local fresh = subscriber_registry.create_registry()

        local ok_assign = pcall(function()
            fresh["custom_field"] = 123
        end)
        assert(not ok_assign, "direct table assignment on subscriber registry must throw")
    end,
}
