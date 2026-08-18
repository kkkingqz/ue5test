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

    preconditions_and_non_local_exit = function()
        handler_registry.with_isolated_handlers(function()
            local mod, env = authoring_context.create_authoring_environment("rh")

            local hero = nil
            mutation_window.execute_in_window(function()
                local player = game.instances.actors.player()
                if not player then
                    hero = game.instances.actors.create("rh:actor.character.hero", {
                        stamina = 10,
                        gold = 20,
                        current_location = "rh:location.city.market",
                    })
                    game.state.meta.player_actor_id = hero.instance_id
                else
                    hero = player
                    hero.stamina = 10
                    hero.gold = 20
                    hero.current_location = "rh:location.city.market"
                end
            end)

            -- Command using preconditions without explicit return on require_*
            env.commands.test_preconditions = function()
                local market = env.location("rh:location.city.market")
                env.player:require_location(market)
                env.player:require_gold(15)
                env.player:require_stamina(5)

                -- If all pass, deduct and succeed
                env.player:spend_gold(15)
                env.player:spend_stamina(5)
            end

            -- Command failing gold precondition
            env.commands.test_gold_fail = function()
                env.player:require_gold(100) -- Player has 20 -> fails
                -- This line must never be reached!
                env.player.gold = 9999
            end

            -- Command failing stamina precondition
            env.commands.test_stamina_fail = function()
                env.player:require_stamina(50) -- Player has 10 -> fails
                env.player.stamina = 9999
            end

            -- Command failing location precondition
            env.commands.test_location_fail = function()
                local tavern = env.location("rh:location.city.tavern")
                env.player:require_location(tavern) -- Player is at market -> fails
            end

            mod.register({})

            mutation_window.execute_in_window(function()
                -- 1. Successful preconditions
                game.runtime.dispatch_command({
                    command_id = "rh:command.test_preconditions",
                    args = {},
                    sequence = 601,
                })
                local r1 = game.runtime.last_command_result
                assert(r1 ~= nil and r1.ok == true, "command with met preconditions must succeed")
                assert(hero.gold == 5, "gold must be 5")
                assert(hero.stamina == 5, "stamina must be 5")

                -- 2. Gold precondition failure
                game.runtime.dispatch_command({
                    command_id = "rh:command.test_gold_fail",
                    args = {},
                    sequence = 602,
                })
                local r2 = game.runtime.last_command_result
                assert(r2 ~= nil and r2.ok == false, "unmet gold precondition must fail")
                assert(r2.error.code == "rh:error.economy.insufficient_gold", "error code must match")
                assert(r2.error.params.required_gold == 100, "required_gold must be 100")
                assert(hero.gold == 5, "state must not change on precondition failure")

                -- 3. Stamina precondition failure
                game.runtime.dispatch_command({
                    command_id = "rh:command.test_stamina_fail",
                    args = {},
                    sequence = 603,
                })
                local r3 = game.runtime.last_command_result
                assert(r3 ~= nil and r3.ok == false, "unmet stamina precondition must fail")
                assert(r3.error.code == "rh:error.economy.insufficient_stamina", "error code must match")
                assert(hero.stamina == 5, "state must not change on precondition failure")

                -- 4. Location precondition failure
                game.runtime.dispatch_command({
                    command_id = "rh:command.test_location_fail",
                    args = {},
                    sequence = 604,
                })
                local r4 = game.runtime.last_command_result
                assert(r4 ~= nil and r4.ok == false, "unmet location precondition must fail")
                assert(r4.error.code == "rh:error.location.wrong_location", "error code must match")
            end)
        end)
    end,

    spend_operations_and_precondition_not_checked_fault = function()
        handler_registry.with_isolated_handlers(function()
            local mod, env = authoring_context.create_authoring_environment("rh")

            mutation_window.execute_in_window(function()
                local player = game.instances.actors.player()
                if not player then
                    local hero = game.instances.actors.create("rh:actor.character.hero", {
                        stamina = 5,
                        gold = 5,
                    })
                    game.state.meta.player_actor_id = hero.instance_id
                else
                    player.stamina = 5
                    player.gold = 5
                end
            end)

            -- Command that forgets require_gold before spending
            env.commands.test_missed_require_gold = function()
                env.player:spend_gold(50) -- Fails because player only has 5 gold!
            end

            -- Command that forgets require_stamina before spending
            env.commands.test_missed_require_stamina = function()
                env.player:spend_stamina(50)
            end

            mod.register({})

            mutation_window.execute_in_window(function()
                -- 1. spend_gold without require_gold -> PreconditionNotChecked fault
                local ok1, err1 = pcall(function()
                    game.runtime.dispatch_command({
                        command_id = "rh:command.test_missed_require_gold",
                        args = {},
                        sequence = 701,
                    })
                end)
                assert(not ok1, "spend_gold without sufficient gold must throw fault")
                assert(string.find(tostring(err1), "PreconditionNotChecked"),
                    "Error must indicate PreconditionNotChecked, got: " .. tostring(err1))

                -- 2. spend_stamina without require_stamina -> PreconditionNotChecked fault
                local ok2, err2 = pcall(function()
                    game.runtime.dispatch_command({
                        command_id = "rh:command.test_missed_require_stamina",
                        args = {},
                        sequence = 702,
                    })
                end)
                assert(not ok2, "spend_stamina without sufficient stamina must throw fault")
                assert(string.find(tostring(err2), "PreconditionNotChecked"),
                    "Error must indicate PreconditionNotChecked, got: " .. tostring(err2))
            end)
        end)
    end,

    command_handlers_receive_definition_handles = function()
        handler_registry.with_isolated_handlers(function()
            local mod, env = authoring_context.create_authoring_environment("rh")

            mutation_window.execute_in_window(function()
                local player = game.instances.actors.player()
                if not player then
                    local hero = game.instances.actors.create("rh:actor.character.hero", {
                        stamina = 20,
                        gold = 50,
                        current_location = "rh:location.city.tavern",
                    })
                    game.state.meta.player_actor_id = hero.instance_id
                else
                    player.current_location = "rh:location.city.tavern"
                end
            end)

            local received_target = nil
            env.commands.test_travel_handle = function(target)
                received_target = target
                -- Direct call on definition handle:
                env.player.current_location:require_connected(target)
                env.player:travel(target)
            end

            mod.register({})

            mutation_window.execute_in_window(function()
                -- Dispatch with named arg { target_location_id = "rh:location.city.market" }
                game.runtime.dispatch_command({
                    command_id = "rh:command.test_travel_handle",
                    args = { target_location_id = "rh:location.city.market" },
                    sequence = 801,
                })
                local r = game.runtime.last_command_result
                assert(r ~= nil and r.ok == true, "travel command must succeed")
                assert(received_target ~= nil, "target handle must be received")
                assert(type(received_target) == "table", "target handle must be a table")
                assert(received_target.id == "rh:location.city.market", "target id must match")
                assert(type(received_target.is_connected) == "function", "target must have is_connected method")

                -- Check player.current_location is a DefinitionHandle
                local cur_loc = env.player.current_location
                assert(type(cur_loc) == "table", "player.current_location must be a handle")
                assert(cur_loc.id == "rh:location.city.market", "player.current_location id must match")
            end)
        end)
    end,

    unified_actor_domain_api = function()
        handler_registry.with_isolated_handlers(function()
            local mod, env = authoring_context.create_authoring_environment("rh")

            local hero = nil
            mutation_window.execute_in_window(function()
                local player = game.instances.actors.player()
                if not player then
                    hero = game.instances.actors.create("rh:actor.character.hero", {
                        stamina = 10,
                        gold = 10,
                        current_location = "rh:location.city.tavern",
                    })
                    game.state.meta.player_actor_id = hero.instance_id
                else
                    hero = player
                    hero.stamina = 10
                    hero.gold = 10
                    hero.current_location = "rh:location.city.tavern"
                end
            end)

            env.commands.test_actor_api = function()
                -- 1. Gold operations
                env.player:add_gold(30)
                env.player:require_gold(40)
                env.player:spend_gold(15)

                -- 2. Stamina operations
                env.player:add_stamina(10)
                env.player:require_stamina(20)
                env.player:spend_stamina(8)

                -- 3. Location operations
                local market = env.location("rh:location.city.market")
                env.player.current_location:require_connected(market)
                env.player:travel(market)
                env.player:require_location(market)

                -- 4. Item operations
                local sword_def = env.def.item("weapon.iron_sword")
                local item_id = env.player:add_item(sword_def)
                assert(item_id ~= nil and type(item_id) == "string", "item_id must be allocated")
            end

            mod.register({})

            mutation_window.execute_in_window(function()
                game.runtime.dispatch_command({
                    command_id = "rh:command.test_actor_api",
                    args = {},
                    sequence = 901,
                })
                local r = game.runtime.last_command_result
                assert(r ~= nil and r.ok == true, "actor api test must succeed")
                assert(hero.gold == 25, "gold must be 10 + 30 - 15 = 25")
                assert(hero.stamina == 12, "stamina must be 10 + 10 - 8 = 12")
                assert(hero.current_location == "rh:location.city.market" or env.player.current_location.id == "rh:location.city.market",
                    "player location must be market")
            end)
        end)
    end,
}
