-- CHR-08 / CHR-09: Package Commands, Override Semantics and Full Lifecycle Specification
-- Verifies that a package/mod can register its own command handlers under its own namespace,
-- along with validators, state mutations, and event publications without touching core files or C++.
-- Also verifies duplicate registration rejection, explicit override replacement, and override-missing errors.

local command_dispatcher = require("core:module.runtime.command_dispatcher")
local handler_registry = require("core:module.runtime.handler_registry")
local validator_registry = require("core:module.runtime.validator_registry")
local event_bus = require("core:module.runtime.event_bus")

return {
    package_command_full_lifecycle_execution = function()
        event_bus.with_isolated_subscribers(function()
            event_bus.clear_published_events()
            game.runtime.phase = "idle"

            local trace = {}

            -- 1. Mod package validator
            local old_validators = game.commands.validators
            local val_reg = validator_registry.create_registry()
            val_reg.register(
                "test_mod:validator.craft_check",
                {
                    validate = function(ctx)
                        if not ctx.payload or not ctx.payload.item_name then
                            return false, {
                                code = "test_mod:error.craft.missing_item_name",
                                params = {},
                            }
                        end
                        table.insert(trace, "validator_passed")
                        return true
                    end,
                }
            )
            game.commands.validators = val_reg

            -- 2. Mod package subscriber
            game.events.subscribers.register(
                "test_mod:subscriber.craft_listener",
                "test_mod:event.item_crafted",
                function(env)
                    table.insert(trace, "subscriber_received:" .. tostring(env.payload.item_name))
                end
            )

            -- 3. Mod package handler
            local reg = handler_registry.create_registry()
            reg.register("test_mod:command.craft_item", function(request)
                table.insert(trace, "handler_executed")
                game.state.test_crafted_item = request.args.item_name
                game.events.enqueue({
                    event_id = "test_mod:event.item_crafted",
                    payload = {
                        item_name = request.args.item_name,
                    },
                })
                return {
                    ok = true,
                    value = { crafted = request.args.item_name },
                }
            end)

            local dispatcher = command_dispatcher.new(reg)
            local seq = dispatcher.dispatch({
                command_id = "test_mod:command.craft_item",
                args = { item_name = "test_mod:item.potion.healing" },
                sequence = 901,
            })

            -- Restore validators
            game.commands.validators = old_validators

            assert(seq == 901, "dispatch sequence must match")

            -- Verify result
            local result = game.runtime.last_command_result
            assert(result ~= nil and result.ok == true, "command must succeed")
            assert(result.value.crafted == "test_mod:item.potion.healing")
            assert(game.state.test_crafted_item == "test_mod:item.potion.healing", "state mutation must be committed")

            -- Verify lifecycle trace: validator -> handler -> subscriber
            assert(#trace == 3, "must have 3 execution steps, got " .. tostring(#trace))
            assert(trace[1] == "validator_passed", "1: validator passed")
            assert(trace[2] == "handler_executed", "2: handler executed")
            assert(trace[3] == "subscriber_received:test_mod:item.potion.healing", "3: subscriber received event")

            -- Cleanup
            event_bus.clear_published_events()
            game.runtime.phase = "idle"
        end)
    end,

    package_command_validator_refusal_aborts_mutation_and_events = function()
        event_bus.with_isolated_subscribers(function()
            event_bus.clear_published_events()
            game.runtime.phase = "idle"

            local old_validators = game.commands.validators
            local val_reg = validator_registry.create_registry()
            val_reg.register(
                "test_mod:validator.craft_check",
                {
                    validate = function(ctx)
                        if not ctx.payload or not ctx.payload.item_name then
                            return false, {
                                code = "test_mod:error.craft.missing_item_name",
                                params = {},
                            }
                        end
                        return true
                    end,
                }
            )
            game.commands.validators = val_reg

            local subscriber_invoked = false
            game.events.subscribers.register(
                "test_mod:subscriber.guard",
                "test_mod:event.item_crafted",
                function(_env)
                    subscriber_invoked = true
                end
            )

            local reg = handler_registry.create_registry()
            reg.register("test_mod:command.craft_item", function(request)
                game.state.should_not_mutate = true
                game.events.enqueue({
                    event_id = "test_mod:event.item_crafted",
                    payload = { item_name = request.args.item_name },
                })
                return { ok = true }
            end)

            local dispatcher = command_dispatcher.new(reg)
            -- Missing item_name in args -> validator refuses
            local seq = dispatcher.dispatch({
                command_id = "test_mod:command.craft_item",
                args = {},
                sequence = 902,
            })

            -- Restore validators
            game.commands.validators = old_validators

            assert(seq == 902)

            local result = game.runtime.last_command_result
            assert(result ~= nil and result.ok == false, "command must be refused by validator")
            assert(result.error.code == "test_mod:error.craft.missing_item_name",
                "error code must match validator refusal, got: " .. tostring(result.error.code))
            assert(game.state.should_not_mutate == nil, "state must not be mutated on validator refusal")
            assert(subscriber_invoked == false, "subscriber must not be called on refusal")
            assert(#game.events.get_published_events() == 0, "0 events published on refusal")

            -- Cleanup
            event_bus.clear_published_events()
            game.runtime.phase = "idle"
        end)
    end,

    duplicate_registration_without_override_rejected = function()
        local registry = handler_registry.create_registry()

        registry.register("test_mod:command.unique_action", function()
            return { ok = true, value = "first" }
        end)

        local ok, err = pcall(function()
            registry.register("test_mod:command.unique_action", function()
                return { ok = true, value = "second" }
            end)
        end)

        assert(not ok, "duplicate registration without override must fail")
        assert(tostring(err):find("CommandHandlerDuplicateRegistration") ~= nil,
            "must throw CommandHandlerDuplicateRegistration, got: " .. tostring(err))
    end,

    explicit_override_replaces_existing_handler = function()
        local registry = handler_registry.create_registry()

        registry.register("test_mod:command.overridable_action", function()
            return { ok = true, value = "original" }
        end)

        -- Override with explicit option
        local replaced = registry.register(
            "test_mod:command.overridable_action",
            function()
                return { ok = true, value = "overridden" }
            end,
            { override = true }
        )
        assert(type(replaced) == "function", "override registration must return the handler")

        local dispatcher = command_dispatcher.new(registry)
        dispatcher.dispatch({
            command_id = "test_mod:command.overridable_action",
            args = {},
            sequence = 903,
        })

        local result = game.runtime.last_command_result
        assert(result.ok == true)
        assert(result.value == "overridden", "dispatched command must invoke the overridden handler")
    end,

    override_on_unregistered_command_rejected = function()
        local registry = handler_registry.create_registry()

        local ok, err = pcall(function()
            registry.register(
                "test_mod:command.nonexistent_override",
                function() return { ok = true } end,
                { override = true }
            )
        end)

        assert(not ok, "override on unregistered command must fail")
        assert(tostring(err):find("CommandHandlerOverrideMissing") ~= nil,
            "must throw CommandHandlerOverrideMissing, got: " .. tostring(err))
    end,
}
