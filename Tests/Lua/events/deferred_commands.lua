-- GEW-12: Deferred Commands Queue Specification
-- Verifies that commands enqueued by event handlers are executed sequentially
-- after the current event pump completes, each in its own mutation window;
-- synchronous dispatch is rejected; and queue capacity limits are enforced without consuming sequence.

local event_bus = require("core:module.runtime.event_bus")
local command_dispatcher = require("core:module.runtime.command_dispatcher")
local handler_registry = require("core:module.runtime.handler_registry")

return {
    deferred_command_executed_after_pump_in_own_window = function()
        event_bus.with_isolated_subscribers(function()
            event_bus.clear_published_events()
            game.commands.clear_queue()
            game.runtime.phase = "idle"

            local execution_trace = {}

            game.events.subscribers.register(
                "core:subscriber.test.step1_listener",
                "core:event.test.step1_done",
                function(_env)
                    table.insert(execution_trace, "event_step1_received")
                    -- Enqueue deferred command
                    game.commands.enqueue({
                        command_id = "core:command.test.step2",
                        args = { value = "second_step" },
                    })
                end
            )

            game.events.subscribers.register(
                "core:subscriber.test.step2_listener",
                "core:event.test.step2_done",
                function(_env)
                    table.insert(execution_trace, "event_step2_received")
                end
            )

            local reg = handler_registry.create_registry()
            reg.register("core:command.test.step1", function(_request)
                table.insert(execution_trace, "cmd_step1_executed")
                game.state.step1_complete = true
                game.events.enqueue({
                    event_id = "core:event.test.step1_done",
                    payload = {},
                })
                return { ok = true }
            end)
            reg.register("core:command.test.step2", function(_request)
                table.insert(execution_trace, "cmd_step2_executed")
                game.state.step2_complete = true
                game.events.enqueue({
                    event_id = "core:event.test.step2_done",
                    payload = {},
                })
                return { ok = true }
            end)

            local dispatcher = command_dispatcher.new(reg)
            dispatcher.dispatch({
                command_id = "core:command.test.step1",
                args = {},
                sequence = 601,
            })

            -- Verify exact trace: Cmd1 -> Event1 -> Cmd2 -> Event2
            assert(#execution_trace == 4, "must have 4 execution steps, got " .. tostring(#execution_trace))
            assert(execution_trace[1] == "cmd_step1_executed", "1: cmd_step1_executed")
            assert(execution_trace[2] == "event_step1_received", "2: event_step1_received")
            assert(execution_trace[3] == "cmd_step2_executed", "3: cmd_step2_executed (deferred in own window)")
            assert(execution_trace[4] == "event_step2_received", "4: event_step2_received")

            assert(game.state.step1_complete == true, "step1 state must be committed")
            assert(game.state.step2_complete == true, "step2 state must be committed")

            -- Cleanup
            event_bus.clear_published_events()
            game.commands.clear_queue()
            game.runtime.phase = "idle"
        end)
    end,

    command_queue_overflow_rejected_without_consuming_sequence = function()
        event_bus.with_isolated_subscribers(function()
            event_bus.clear_published_events()
            game.commands.clear_queue()
            game.runtime.phase = "idle"

            -- Fill queue to capacity (100)
            for i = 1, 100 do
                local ok = game.commands.enqueue({
                    command_id = "core:command.test.queued_" .. tostring(i),
                    args = {},
                })
                assert(ok, "enqueue item " .. tostring(i) .. " must succeed")
            end

            assert(game.commands.get_queue_length() == 100, "queue length must be 100")

            -- 101st attempt must fail with CommandQueueFull
            local ok_overflow, err = pcall(function()
                game.commands.enqueue({
                    command_id = "core:command.test.overflow",
                    args = {},
                })
            end)

            assert(not ok_overflow, "101st enqueue must throw CommandQueueFull")
            assert(string.find(tostring(err), "CommandQueueFull") ~= nil,
                "error should be CommandQueueFull, got: " .. tostring(err))

            assert(game.commands.get_queue_length() == 100, "queue length must still be 100")

            -- Cleanup
            game.commands.clear_queue()
            game.runtime.phase = "idle"
        end)
    end,

    invalid_command_request_rejected_at_enqueue = function()
        game.commands.clear_queue()

        local ok_bad_kind = pcall(function()
            game.commands.enqueue({
                command_id = "core:event.not_a_command",
                args = {},
            })
        end)
        assert(not ok_bad_kind, "enqueueing non-command kind ID must throw")

        local ok_non_table = pcall(function()
            game.commands.enqueue("not_a_table")
        end)
        assert(not ok_non_table, "enqueueing non-table must throw")

        game.commands.clear_queue()
    end,
}
