-- SAS-01..05: Simplified Authoring Surface Specification (ADR-0028)
-- Verifies per-module _ENV injection, authoring environment globals,
-- prohibition of global writes (AuthoringGlobalWriteDisallowed),
-- loader-created descriptors, and implicit command return normalization.

local authoring_context = require("core:module.authoring.context")
local mutation_window = require("core:module.runtime.mutation_window")
local handler_registry = require("core:module.runtime.handler_registry")

return {
    environment_symbols_and_globals_isolation = function()
        local mod, env = authoring_context.create_authoring_environment("rh")
        assert(type(mod) == "table", "mod descriptor must be a table")
        assert(type(env) == "table", "env must be a table")

        -- Pre-bound authoring symbols
        assert(env.commands == mod.commands, "env.commands must map to mod.commands")
        assert(env.player == mod.player, "env.player must map to mod.player")
        assert(env.world == mod.world, "env.world must map to mod.world")
        assert(env.def == mod.def, "env.def must map to mod.def")
        assert(type(env.location) == "function", "env.location must be a function")
        assert(type(env.actor) == "function", "env.actor must be a function")
        assert(type(env.actors) == "function", "env.actors must be a function")
        assert(type(env.fail) == "function", "env.fail must be a function")
        assert(type(env.emit) == "function", "env.emit must be a function")
        assert(type(env.on) == "function", "env.on must be a function")
        assert(type(env.text) == "function", "env.text must be a function")
        assert(type(env.button) == "function", "env.button must be a function")
        assert(type(env.action) == "function", "env.action must be a function")
        assert(type(env.show_screen) == "function", "env.show_screen must be a function")

        -- Standard globals accessible via __index
        assert(env.type == type, "standard globals must be accessible via _ENV")
        assert(env.assert == assert, "standard globals must be accessible via _ENV")
        assert(env.tostring == tostring, "standard globals must be accessible via _ENV")
        assert(env.string == string, "standard globals must be accessible via _ENV")
        assert(env.table == table, "standard globals must be accessible via _ENV")
        assert(env.math == math, "standard globals must be accessible via _ENV")

        -- Attempting to write undeclared global variables must fail with AuthoringGlobalWriteDisallowed
        local ok, err = pcall(function()
            env.my_undeclared_global = 123
        end)
        assert(not ok, "writing global in authoring environment must throw")
        assert(string.find(tostring(err), "AuthoringGlobalWriteDisallowed"),
            "error must be AuthoringGlobalWriteDisallowed, got: " .. tostring(err))
    end,

    implicit_command_returns_and_fail_normalization = function()
        handler_registry.with_isolated_handlers(function()
            local mod, env = authoring_context.create_authoring_environment("rh")

            local local_multiplier = 2

            -- 1. Implicit nil success
            env.commands.test_implicit_nil = function()
                -- returns nil
            end

            -- 2. Implicit table value success
            env.commands.test_implicit_value = function(args)
                return {
                    doubled = args.val * local_multiplier,
                }
            end

            -- 3. Implicit primitive value success
            env.commands.test_implicit_primitive = function()
                return 42
            end

            -- 4. fail() typed refusal
            env.commands.test_fail_refusal = function()
                return env.fail("test.forbidden", { reason = "test_condition" })
            end

            -- 5. Fake fail table returned without calling fail()
            env.commands.test_fake_fail_table = function()
                return { ok = false, error = "manual_fake_error" }
            end

            -- Register declared commands
            mod.register({})

            mutation_window.execute_in_window(function()
                -- 1. Implicit nil success
                game.runtime.dispatch_command({
                    command_id = "rh:command.test_implicit_nil",
                    args = {},
                    sequence = 501,
                })
                local r1 = game.runtime.last_command_result
                assert(r1 ~= nil and r1.ok == true, "implicit nil must succeed")

                -- 2. Implicit table value success
                game.runtime.dispatch_command({
                    command_id = "rh:command.test_implicit_value",
                    args = { val = 21 },
                    sequence = 502,
                })
                local r2 = game.runtime.last_command_result
                assert(r2 ~= nil and r2.ok == true, "implicit table value must succeed")
                assert(r2.value ~= nil and r2.value.doubled == 42, "value.doubled must be 42")

                -- 3. Implicit primitive value success
                game.runtime.dispatch_command({
                    command_id = "rh:command.test_implicit_primitive",
                    args = {},
                    sequence = 503,
                })
                local r3 = game.runtime.last_command_result
                assert(r3 ~= nil and r3.ok == true, "implicit primitive value must succeed")
                assert(r3.value == 42, "value must be 42")

                -- 4. fail() typed refusal
                game.runtime.dispatch_command({
                    command_id = "rh:command.test_fail_refusal",
                    args = {},
                    sequence = 504,
                })
                local r4 = game.runtime.last_command_result
                assert(r4 ~= nil and r4.ok == false, "fail() must produce typed refusal")
                assert(r4.error.code == "rh:error.test.forbidden")
                assert(r4.error.params.reason == "test_condition")

                -- 5. SAS-04 negative case: table with ok = false returned without fail() is treated as value
                game.runtime.dispatch_command({
                    command_id = "rh:command.test_fake_fail_table",
                    args = {},
                    sequence = 505,
                })
                local r5 = game.runtime.last_command_result
                assert(r5 ~= nil and r5.ok == true, "table returned without fail() must be treated as success value")
                assert(type(r5.value) == "table" and r5.value.ok == false,
                    "value must contain the raw returned table with ok = false")
            end)
        end)
    end,
}
