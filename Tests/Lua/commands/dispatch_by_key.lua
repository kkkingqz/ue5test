-- CHR-05 / CHR-07: Dispatch by Key & Unknown Command Refusal Specification
-- Verifies that commands are looked up by command_id from the handler registry,
-- unknown commands return typed refusal { ok = false, error = { code = "core:error.command.unknown", params = { command_id = ... } } },
-- the mutation window does not open, events are not delivered, and command sequence counter increments.

local command_dispatcher = require("core:module.runtime.command_dispatcher")
local handler_registry = require("core:module.runtime.handler_registry")
local event_bus = require("core:module.runtime.event_bus")

return {
    unknown_command_returns_typed_refusal_without_mutation = function()
        event_bus.with_isolated_subscribers(function()
            event_bus.clear_published_events()
            game.runtime.phase = "idle"

            local initial_count = game.runtime.command_count or 0

            local before_hash = ""
            if game.runtime.get_canonical_state_hash then
                before_hash = game.runtime.get_canonical_state_hash()
            end

            local seq = game.runtime.dispatch_command({
                command_id = "core:command.test.definitely_does_not_exist",
                args = {},
                sequence = 801,
            })
            assert(seq == 801, "dispatch must return sequence number")

            local result = game.runtime.last_command_result
            assert(type(result) == "table", "last_command_result must be a table")
            assert(result.ok == false, "unknown command result.ok must be false")
            assert(type(result.error) == "table", "result.error must be a table")
            assert(result.error.code == "core:error.command.unknown",
                "error code must be core:error.command.unknown, got: " .. tostring(result.error.code))
            assert(result.error.params ~= nil, "error params must exist")
            assert(result.error.params.command_id == "core:command.test.definitely_does_not_exist",
                "error params.command_id must match requested command_id")

            if game.runtime.get_canonical_state_hash then
                local after_hash = game.runtime.get_canonical_state_hash()
                assert(after_hash == before_hash, "state must not mutate on unknown command refusal")
            end

            local events = game.events.get_published_events()
            assert(#events == 0, "no events should be published on unknown command")
            assert(game.runtime.command_count == initial_count + 1, "command count must increment")
        end)
    end,

    deferred_unknown_command_returns_typed_refusal = function()
        event_bus.with_isolated_subscribers(function()
            event_bus.clear_published_events()
            game.commands.clear_queue()
            game.runtime.phase = "idle"

            local reg = handler_registry.create_registry()
            reg.register("core:command.test.trigger_deferred_unknown", function(_request)
                game.commands.enqueue({
                    command_id = "core:command.test.deferred_nonexistent",
                    args = {},
                })
                return { ok = true }
            end)

            local dispatcher = command_dispatcher.new(reg)
            dispatcher.dispatch({
                command_id = "core:command.test.trigger_deferred_unknown",
                args = {},
                sequence = 802,
            })

            -- After the outer command completes, the deferred queue is drained.
            -- The deferred unknown command is executed and its result becomes last_command_result.
            local result = game.runtime.last_command_result
            assert(type(result) == "table", "last_command_result must be a table")
            assert(result.ok == false, "deferred unknown command result.ok must be false")
            assert(result.error.code == "core:error.command.unknown",
                "deferred error code must be core:error.command.unknown, got: " .. tostring(result.error.code))
            assert(result.error.params.command_id == "core:command.test.deferred_nonexistent",
                "deferred error params.command_id must match")
        end)
    end,

    registered_command_executes_in_mutation_window = function()
        local reg = handler_registry.create_registry()
        reg.register("core:command.test.normal_action", function(request)
            game.state.test_action_run = true
            return { ok = true, value = { echo = request.args.param } }
        end)

        local dispatcher = command_dispatcher.new(reg)
        local seq = dispatcher.dispatch({
            command_id = "core:command.test.normal_action",
            args = { param = "hello" },
            sequence = 803,
        })
        assert(seq == 803)

        local result = game.runtime.last_command_result
        assert(result.ok == true)
        assert(result.value.echo == "hello")
        assert(game.state.test_action_run == true)
    end,
}
