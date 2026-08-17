-- GEW-09: Runtime Phases and Pump Limit Specification
-- Verifies actual phase transitions (idle -> executing_command -> pumping_events -> idle),
-- rejection of synchronous command dispatch during ExecutingCommand and PumpingEvents,
-- and session transition to 'failed' upon pump limit breach or event handler fault.

local event_bus = require("core:module.runtime.event_bus")
local command_dispatcher = require("core:module.runtime.command_dispatcher")
local handler_registry = require("core:module.runtime.handler_registry")

return {
    runtime_phase_transitions_during_command_lifecycle = function()
        event_bus.with_isolated_subscribers(function()
            event_bus.clear_published_events()
            game.runtime.phase = "idle"

            local observed_phases = {}

            event_bus.subscribe("core:event.test.phase_check", function(_env)
                table.insert(observed_phases, { context = "event_subscriber", phase = game.runtime.phase })
            end)

            local reg = handler_registry.create_registry()
            reg.register("core:command.test.phase_lifecycle", function(_request)
                table.insert(observed_phases, { context = "command_handler", phase = game.runtime.phase })
                game.events.enqueue({
                    event_id = "core:event.test.phase_check",
                    payload = {},
                })
                return { ok = true }
            end)

            local dispatcher = command_dispatcher.new(reg)

            assert(game.runtime.phase == "idle", "phase must start at idle")
            dispatcher.dispatch({
                command_id = "core:command.test.phase_lifecycle",
                args = {},
                sequence = 301,
            })
            assert(game.runtime.phase == "idle", "phase must return to idle after dispatch")

            assert(#observed_phases == 2, "must have recorded handler and subscriber phases")
            assert(observed_phases[1].context == "command_handler" and observed_phases[1].phase == "executing_command",
                "handler must see phase 'executing_command'")
            assert(observed_phases[2].context == "event_subscriber" and observed_phases[2].phase == "pumping_events",
                "subscriber must see phase 'pumping_events'")
        end)
    end,

    synchronous_command_during_event_pump_rejected = function()
        event_bus.with_isolated_subscribers(function()
            event_bus.clear_published_events()
            game.runtime.phase = "idle"

            local nested_dispatch_threw = false
            local nested_error_message = ""
            local test_dispatcher = nil

            event_bus.subscribe("core:event.test.nested_cmd", function(_env)
                local ok, err = pcall(function()
                    test_dispatcher.dispatch({
                        command_id = "core:command.test.inner",
                        args = {},
                        sequence = 302,
                    })
                end)
                if not ok then
                    nested_dispatch_threw = true
                    nested_error_message = tostring(err)
                end
            end)

            local reg = handler_registry.create_registry()
            reg.register("core:command.test.outer", function(_request)
                game.events.enqueue({
                    event_id = "core:event.test.nested_cmd",
                    payload = {},
                })
                return { ok = true }
            end)
            reg.register("core:command.test.inner", function(_request)
                return { ok = true }
            end)

            test_dispatcher = command_dispatcher.new(reg)
            test_dispatcher.dispatch({
                command_id = "core:command.test.outer",
                args = {},
                sequence = 303,
            })

            assert(nested_dispatch_threw, "synchronous command dispatch during event pump must throw")
            assert(string.find(nested_error_message, "CommandDispatchDuringEventPump") ~= nil,
                "error should be CommandDispatchDuringEventPump, got: " .. nested_error_message)
        end)
    end,

    reentrant_command_during_command_execution_rejected = function()
        event_bus.with_isolated_subscribers(function()
            event_bus.clear_published_events()
            game.runtime.phase = "idle"

            local inner_dispatch_threw = false
            local inner_error_message = ""
            local test_dispatcher = nil

            local reg = handler_registry.create_registry()
            reg.register("core:command.test.reentrant_outer", function(_request)
                local ok, err = pcall(function()
                    test_dispatcher.dispatch({
                        command_id = "core:command.test.reentrant_inner",
                        args = {},
                        sequence = 304,
                    })
                end)
                if not ok then
                    inner_dispatch_threw = true
                    inner_error_message = tostring(err)
                end
                return { ok = true }
            end)
            reg.register("core:command.test.reentrant_inner", function(_request)
                return { ok = true }
            end)

            test_dispatcher = command_dispatcher.new(reg)
            test_dispatcher.dispatch({
                command_id = "core:command.test.reentrant_outer",
                args = {},
                sequence = 305,
            })

            assert(inner_dispatch_threw, "reentrant command dispatch during handler execution must throw")
            assert(string.find(inner_error_message, "CommandDispatchReentrant") ~= nil,
                "error should be CommandDispatchReentrant, got: " .. inner_error_message)
        end)
    end,

    pump_limit_breach_transitions_session_to_failed = function()
        event_bus.with_isolated_subscribers(function()
            event_bus.clear_published_events()
            game.runtime.phase = "idle"
            event_bus.set_pump_limit(5)

            -- Self-sustaining cyclic event loop
            event_bus.subscribe("core:event.test.infinite_loop", function(_env)
                game.events.enqueue({
                    event_id = "core:event.test.infinite_loop",
                    payload = {},
                })
            end)

            local reg = handler_registry.create_registry()
            reg.register("core:command.test.trigger_loop", function(_request)
                game.events.enqueue({
                    event_id = "core:event.test.infinite_loop",
                    payload = {},
                })
                return { ok = true }
            end)

            local dispatcher = command_dispatcher.new(reg)
            local ok, err = pcall(function()
                dispatcher.dispatch({
                    command_id = "core:command.test.trigger_loop",
                    args = {},
                    sequence = 306,
                })
            end)

            assert(not ok, "pump limit breach must throw an error")
            assert(string.find(tostring(err), "EventPumpLimitExceeded") ~= nil,
                "error should be EventPumpLimitExceeded, got: " .. tostring(err))
            assert(game.runtime.phase == "failed", "session phase must be transitioned to 'failed'")

            -- Attempting any command in failed phase must be rejected
            local ok_after_fail, err_after_fail = pcall(function()
                dispatcher.dispatch({
                    command_id = "core:command.test.trigger_loop",
                    args = {},
                    sequence = 307,
                })
            end)
            assert(not ok_after_fail, "dispatching in failed phase must throw")
            assert(string.find(tostring(err_after_fail), "SessionStateFailed") ~= nil,
                "error should be SessionStateFailed, got: " .. tostring(err_after_fail))

            -- Cleanup for next tests
            event_bus.reset_pump_limit()
            event_bus.clear_published_events()
            game.runtime.phase = "idle"
        end)
    end,

    event_handler_fault_transitions_session_to_failed = function()
        event_bus.with_isolated_subscribers(function()
            event_bus.clear_published_events()
            game.runtime.phase = "idle"

            event_bus.subscribe("core:event.test.fault_event", function(_env)
                error("DeliberateEventHandlerFault: subscriber threw exception", 0)
            end)

            local reg = handler_registry.create_registry()
            reg.register("core:command.test.trigger_fault", function(_request)
                game.events.enqueue({
                    event_id = "core:event.test.fault_event",
                    payload = {},
                })
                return { ok = true }
            end)

            local dispatcher = command_dispatcher.new(reg)
            local ok, _ = pcall(function()
                dispatcher.dispatch({
                    command_id = "core:command.test.trigger_fault",
                    args = {},
                    sequence = 308,
                })
            end)

            assert(not ok, "event handler fault must fail dispatch")
            assert(game.runtime.phase == "failed", "session phase must be transitioned to 'failed'")

            -- Cleanup for next tests
            event_bus.clear_published_events()
            game.runtime.phase = "idle"
        end)
    end,
}
