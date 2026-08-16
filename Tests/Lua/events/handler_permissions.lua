-- GEW-11: Event Handler Permissions Specification
-- Verifies that event handlers run while mutation window is closed (direct state mutation rejected),
-- handlers can read state and repository, and an event handler fault leaves originating command committed.

local event_bus = require("core:module.runtime.event_bus")
local command_dispatcher = require("core:module.runtime.command_dispatcher")
local handler_registry = require("core:module.runtime.handler_registry")

return {
    event_handler_cannot_mutate_state_directly = function()
        event_bus.clear_published_events()
        event_bus.clear_subscribers()
        game.runtime.phase = "idle"

        game.events.subscribers.register(
            "core:subscriber.test.illegal_mutator",
            "core:event.test.illegal_mutation",
            function(_env)
                -- Attempt to mutate state during event pump
                game.state.illegal_field = "illegal_value"
            end
        )

        local reg = handler_registry.create_registry()
        reg.register("core:command.test.trigger_illegal_mutation", function(_request)
            game.events.enqueue({
                event_id = "core:event.test.illegal_mutation",
                payload = {},
            })
            return { ok = true }
        end)

        local dispatcher = command_dispatcher.new(reg)
        local ok, err = pcall(function()
            dispatcher.dispatch({
                command_id = "core:command.test.trigger_illegal_mutation",
                args = {},
                sequence = 501,
            })
        end)

        assert(not ok, "dispatch must fail when subscriber attempts direct state mutation")
        assert(tostring(err):find("MutationWindowClosed") ~= nil or tostring(err):find("state") ~= nil,
            "error should mention MutationWindowClosed, got: " .. tostring(err))
        assert(game.runtime.phase == "failed", "session phase must be transitioned to 'failed'")

        -- Cleanup
        event_bus.clear_subscribers()
        event_bus.clear_published_events()
        game.runtime.phase = "idle"
    end,

    event_handler_fault_leaves_originating_command_committed = function()
        event_bus.clear_published_events()
        event_bus.clear_subscribers()
        game.runtime.phase = "idle"

        game.events.subscribers.register(
            "core:subscriber.test.faulty_subscriber",
            "core:event.test.fault_target",
            function(_env)
                error("DeliberateSubscriberFault: simulated failure in event handler", 0)
            end
        )

        local reg = handler_registry.create_registry()
        reg.register("core:command.test.commit_then_fault", function(_request)
            -- Legitimate mutation
            game.state.counter = 200
            game.events.enqueue({
                event_id = "core:event.test.fault_target",
                payload = {},
            })
            return { ok = true, value = { counter = 200 } }
        end)

        local dispatcher = command_dispatcher.new(reg)
        local ok, _ = pcall(function()
            dispatcher.dispatch({
                command_id = "core:command.test.commit_then_fault",
                args = {},
                sequence = 502,
            })
        end)

        assert(not ok, "dispatch must fail when subscriber throws")
        assert(game.state.counter == 200, "originating command state changes must remain committed (counter == 200)")
        assert(game.runtime.phase == "failed", "session phase must be transitioned to 'failed'")

        -- Cleanup
        event_bus.clear_subscribers()
        event_bus.clear_published_events()
        game.runtime.phase = "idle"
    end,

    event_handler_can_read_state_and_repository = function()
        event_bus.clear_published_events()
        event_bus.clear_subscribers()
        game.runtime.phase = "idle"

        local read_success = false

        game.events.subscribers.register(
            "core:subscriber.test.reader",
            "core:event.test.read_check",
            function(_env)
                assert(type(game.state) == "table", "state must be readable")
                assert(game.repository ~= nil, "repository must be readable")
                read_success = true
            end
        )

        local reg = handler_registry.create_registry()
        reg.register("core:command.test.read_check", function(_request)
            game.events.enqueue({
                event_id = "core:event.test.read_check",
                payload = {},
            })
            return { ok = true }
        end)

        local dispatcher = command_dispatcher.new(reg)
        dispatcher.dispatch({
            command_id = "core:command.test.read_check",
            args = {},
            sequence = 503,
        })

        assert(read_success, "subscriber must have successfully read state and repository")

        -- Cleanup
        event_bus.clear_subscribers()
        event_bus.clear_published_events()
        game.runtime.phase = "idle"
    end,
}
