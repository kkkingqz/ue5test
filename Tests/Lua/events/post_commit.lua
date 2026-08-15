-- GEW-07: Post-Commit Event Delivery Specification
-- Verifies that facts are queued during command execution and delivered only
-- after successful command commit; refused or failing commands deliver 0 events;
-- and enqueueing events outside active command execution is rejected.

local event_bus = require("core:module.runtime.event_bus")
local command_dispatcher = require("core:module.runtime.command_dispatcher")

return {
    events_published_after_successful_command_commit = function()
        event_bus.clear_published_events()
        event_bus.clear_subscribers()
        game.runtime.phase = "idle"

        local test_handler = {
            handle_command = function(request)
                if request.command_id == "core:command.test.gew07_success" then
                    game.events.enqueue({
                        event_id = "core:event.location.leave",
                        payload = {
                            from_location_id = "core:location.city.market",
                        },
                    })
                    return { ok = true, value = { travelled = true } }
                end
                return nil
            end,
        }

        local dispatcher = command_dispatcher.new({ test_handler })
        local seq = dispatcher.dispatch({
            command_id = "core:command.test.gew07_success",
            args = {},
            sequence = 101,
            correlation_id = "corr_gew07_success",
        })
        assert(seq == 101, "dispatch sequence must match")

        local events = game.events.get_published_events()
        assert(#events == 1, "exactly 1 event must be published after successful commit, got " .. tostring(#events))
        assert(events[1].event_id == "core:event.location.leave", "event_id must match")
        assert(events[1].schema_version == 1, "schema_version must be 1")
        assert(events[1].correlation_id == "corr_gew07_success", "correlation_id must be inherited from command context")
        assert(events[1].causation_id == "core:command.test.gew07_success", "causation_id must match command_id")
        assert(events[1].source.kind == "command", "source.kind must be command")
        assert(events[1].source.command_id == "core:command.test.gew07_success", "source.command_id must match")
        assert(events[1].payload.from_location_id == "core:location.city.market", "payload content must match")
    end,

    refused_command_delivers_no_events = function()
        event_bus.clear_published_events()
        event_bus.clear_subscribers()
        game.runtime.phase = "idle"

        local test_handler = {
            handle_command = function(request)
                if request.command_id == "core:command.test.gew07_refusal" then
                    game.events.enqueue({
                        event_id = "core:event.location.leave",
                        payload = { from = "market" },
                    })
                    return {
                        ok = false,
                        error = {
                            code = "core:error.location.locked",
                            params = {},
                        },
                    }
                end
                return nil
            end,
        }

        local dispatcher = command_dispatcher.new({ test_handler })
        dispatcher.dispatch({
            command_id = "core:command.test.gew07_refusal",
            args = {},
            sequence = 102,
        })

        local events = game.events.get_published_events()
        assert(#events == 0, "refused command must not deliver any events, got " .. tostring(#events))
    end,

    event_queued_before_mutation_discarded_on_handler_refusal = function()
        event_bus.clear_published_events()
        event_bus.clear_subscribers()
        game.runtime.phase = "idle"

        local test_handler = {
            handle_command = function(request)
                if request.command_id == "core:command.test.gew07_early_event_refusal" then
                    -- Enqueue event before performing checks
                    game.events.enqueue({
                        event_id = "core:event.item.add",
                        payload = { item_id = "core:item.apple" },
                    })
                    -- Later refusal
                    return {
                        ok = false,
                        error = {
                            code = "core:error.item.inventory_full",
                            params = {},
                        },
                    }
                end
                return nil
            end,
        }

        local dispatcher = command_dispatcher.new({ test_handler })
        dispatcher.dispatch({
            command_id = "core:command.test.gew07_early_event_refusal",
            args = {},
            sequence = 103,
        })

        local events = game.events.get_published_events()
        assert(#events == 0, "events enqueued before refusal must be discarded")
    end,

    exception_during_handler_discards_pending_events = function()
        event_bus.clear_published_events()
        event_bus.clear_subscribers()
        game.runtime.phase = "idle"

        local test_handler = {
            handle_command = function(request)
                if request.command_id == "core:command.test.gew07_fault" then
                    game.events.enqueue({
                        event_id = "core:event.location.enter",
                        payload = {},
                    })
                    error("SimulatedHandlerFault: unexpected failure", 0)
                end
                return nil
            end,
        }

        local dispatcher = command_dispatcher.new({ test_handler })
        local ok, err = pcall(function()
            dispatcher.dispatch({
                command_id = "core:command.test.gew07_fault",
                args = {},
                sequence = 104,
            })
        end)
        assert(not ok, "dispatch must fail on handler exception")

        local events = game.events.get_published_events()
        assert(#events == 0, "faulted command must not publish any events")

        -- Cleanup
        game.runtime.phase = "idle"
    end,

    cannot_enqueue_events_outside_command_context = function()
        game.runtime.phase = "idle"
        -- Ensure no active command context
        assert(not event_bus.has_active_context(), "there should be no active command context")

        local ok, err = pcall(function()
            game.events.enqueue({
                event_id = "core:event.location.enter",
                payload = {},
            })
        end)
        assert(not ok, "enqueueing events outside active command context must throw an error")
        assert(string.find(tostring(err), "EventEnqueueOutsideCommandContext") ~= nil,
            "error should be EventEnqueueOutsideCommandContext, got: " .. tostring(err))
    end,

    validator_cannot_enqueue_events = function()
        game.runtime.phase = "idle"
        -- Validators run before begin_command_context, so event_bus has no active context
        local validator_attempted_emit = false
        local validator_emit_threw = false

        local test_validator = {
            validate = function(_ctx)
                validator_attempted_emit = true
                local ok, _ = pcall(function()
                    game.events.enqueue({
                        event_id = "core:event.test",
                        payload = {},
                    })
                end)
                if not ok then
                    validator_emit_threw = true
                end
                return true
            end,
        }

        local test_handler = {
            handle_command = function(_request)
                return { ok = true }
            end,
        }

        local dispatcher = command_dispatcher.new({ test_handler })
        -- Direct simulation: call validator
        test_validator.validate({})
        assert(validator_attempted_emit, "validator must have run")
        assert(validator_emit_threw, "validator attempting to enqueue events must be rejected")
    end,
}
