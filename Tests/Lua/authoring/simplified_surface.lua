-- SAS-01..05: Simplified Authoring Surface Specification (ADR-0028)
-- Verifies per-module _ENV injection, authoring environment globals,
-- prohibition of global writes (AuthoringGlobalWriteDisallowed),
-- loader-created descriptors, and implicit command return normalization.

local authoring_context = require("core:module.authoring.context")
local mutation_window = require("core:module.runtime.mutation_window")
local handler_registry = require("core:module.runtime.handler_registry")
local screens = require("core:module.presentation.screen_requests")

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

        -- Pre-bound entity authoring prototypes (EAE-05)
        assert(env.Actor == mod.Actor, "env.Actor must map to mod.Actor")
        assert(env.Location == mod.Location, "env.Location must map to mod.Location")
        assert(env.Quest == mod.Quest, "env.Quest must map to mod.Quest")
        assert(env.Item == mod.Item, "env.Item must map to mod.Item")

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
                assert(r4.error.code == "textsystem:error.location.wrong_location", "error code must match")
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
            if game and game.instances and game.instances.clear_for_test then
                game.instances.clear_for_test()
                game.instances.register_kind("item")
            end
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
    end,

    presentation_source_registration_and_validation = function()
        if not game.presentation then return end
        game.presentation.clear_for_test()

        -- 1. Invalid source type (non-function)
        local bad_type_ok, bad_type_err = pcall(function()
            game.presentation.register_source("not_a_function")
        end)
        assert(not bad_type_ok, "registering non-function must fail")
        assert(tostring(bad_type_err):find("InvalidPresentationSource"),
            "error must be InvalidPresentationSource, got: " .. tostring(bad_type_err))

        -- 2. Valid source registration
        local call_count = 0
        local dummy_source = function()
            call_count = call_count + 1
            return {
                screen_id = "rh:screen.location.market",
                fields = {
                    title = { schema_id = "core:schema.ui_field.text.v1", value = { text_id = "rh:text.location.market.title" } },
                },
            }
        end
        game.presentation.register_source(dummy_source)
        assert(game.presentation.has_source() == true, "has_source must be true")

        -- 3. Duplicate registration must fail
        local dup_ok, dup_err = pcall(function()
            game.presentation.register_source(dummy_source)
        end)
        assert(not dup_ok, "duplicate registration must fail")
        assert(tostring(dup_err):find("PresentationSourceDuplicateRegistration"),
            "error must be PresentationSourceDuplicateRegistration, got: " .. tostring(dup_err))

        -- 4. Registration after freeze must fail
        game.presentation.freeze()
        assert(game.presentation.is_frozen() == true, "is_frozen must be true")

        local late_ok, late_err = pcall(function()
            game.presentation.register_source(dummy_source)
        end)
        assert(not late_ok, "late registration after freeze must fail")
        assert(tostring(late_err):find("PresentationSourceRegistryFrozen"),
            "error must be PresentationSourceRegistryFrozen, got: " .. tostring(late_err))

        -- Clean up
        game.presentation.clear_for_test()
    end,

    automatic_invalidation_after_successful_command = function()
        handler_registry.with_isolated_handlers(function()
            game.presentation.clear_for_test()

            local mod, env = authoring_context.create_authoring_environment("rh")

            mutation_window.execute_in_window(function()
                local player = game.instances.actors.player()
                if not player then
                    local hero = game.instances.actors.create("rh:actor.character.hero", {
                        stamina = 50,
                        gold = 50,
                        current_location = "rh:location.city.tavern",
                    })
                    game.state.meta.player_actor_id = hero.instance_id
                else
                    player.stamina = 50
                    player.current_location = "rh:location.city.tavern"
                end
            end)

            -- Register presentation source that produces screen for current location
            local source_resolved_count = 0
            game.presentation.register_source(function()
                source_resolved_count = source_resolved_count + 1
                local cur_loc = env.player.current_location
                local loc_id = type(cur_loc) == "table" and cur_loc.id or cur_loc
                return {
                    screen_id = "rh:screen." .. (loc_id == "rh:location.city.market" and "location.market" or "location.tavern"),
                    fields = {
                        loc = { schema_id = "core:schema.ui_field.text.v1", value = { text_id = "core:text.sample" } },
                    },
                }
            end)

            env.commands.test_success_cmd = function()
                env.player:travel("rh:location.city.market")
            end

            env.commands.test_refusal_cmd = function()
                env.fail("some.refusal_error", {})
            end

            mod.register({})

            -- Clear any pending screen before command
            screens.take_pending()
            source_resolved_count = 0

            -- 1. Successful command must automatically trigger presentation resolution
            game.runtime.dispatch_command({
                command_id = "rh:command.test_success_cmd",
                args = {},
                sequence = 1001,
            })
            local r = game.runtime.last_command_result
            assert(r ~= nil and r.ok == true, "command must succeed")

            assert(source_resolved_count == 1, "presentation source must be resolved once after success")
            local pending = screens.take_pending()
            assert(pending ~= nil, "screen request must be published")
            assert(pending.screen_id == "rh:screen.location.market", "published screen must match new location")

            -- 2. Refused command must NOT trigger presentation resolution
            source_resolved_count = 0
            game.runtime.dispatch_command({
                command_id = "rh:command.test_refusal_cmd",
                args = {},
                sequence = 1002,
            })
            local r_ref = game.runtime.last_command_result
            assert(r_ref ~= nil and r_ref.ok == false, "command must be refused")

            assert(source_resolved_count == 0, "presentation source must NOT be resolved after refusal")
            assert(screens.take_pending() == nil, "no screen request must be pending after refusal")

            -- Clean up
            game.presentation.clear_for_test()
        end)
    end,

    presentation_source_state_mutation_disallowed = function()
        game.presentation.clear_for_test()

        -- Presentation source attempting to mutate state outside mutation window
        game.presentation.register_source(function()
            game.state.meta.player_actor_id = "corrupted_id"
        end)

        local ok, err = pcall(function()
            game.presentation.resolve()
        end)

        assert(not ok, "state mutation in presentation source must fail")
        assert(tostring(err):find("MutationWindowClosed") or tostring(err):find("StateWriteOutsideMutationWindow"),
            "mutation outside window must raise MutationWindowClosed / StateWriteOutsideMutationWindow, got: " .. tostring(err))

        game.presentation.clear_for_test()
    end,

    general_buy_command_supports_arbitrary_item_definitions = function()
        handler_registry.with_isolated_handlers(function()
            if game and game.instances and game.instances.clear_for_test then
                game.instances.clear_for_test()
                game.instances.register_kind("item")
            end
            local mod, env = authoring_context.create_authoring_environment("rh")

            -- Define buy handler as in authoring/gameplay.lua
            env.commands.buy = function(item)
                env.player:require_location("rh:location.city.market")
                env.player:require_gold(item.price, "shop.insufficient_gold")

                env.player:spend_gold(item.price)
                env.player:add_item(item)
            end

            mod.register({})

            local hero = nil
            mutation_window.execute_in_window(function()
                local player = game.instances.actors.player()
                if not player then
                    hero = game.instances.actors.create("rh:actor.character.hero", {
                        stamina = 50,
                        gold = 100,
                        current_location = "rh:location.city.market",
                    })
                    game.state.meta.player_actor_id = hero.instance_id
                else
                    hero = player
                    hero.stamina = 50
                    hero.gold = 100
                    hero.current_location = "rh:location.city.market"
                end
            end)

            -- 1. Buy standard iron sword (price 10)
            game.runtime.dispatch_command({
                command_id = "rh:command.buy",
                args = { item = "rh:item.weapon.iron_sword" },
                sequence = 1101,
            })
            local r1 = game.runtime.last_command_result
            assert(r1 ~= nil and r1.ok == true, "buy sword must succeed")
            assert(hero.gold == 90, "gold must be 100 - 10 = 90")

            -- 2. Buy standard leather armor (price 25)
            game.runtime.dispatch_command({
                command_id = "rh:command.buy",
                args = { item = "rh:item.armor.leather_armor" },
                sequence = 1102,
            })
            local r2 = game.runtime.last_command_result
            assert(r2 ~= nil and r2.ok == true, "buy armor must succeed")
            assert(hero.gold == 65, "gold must be 90 - 25 = 65")

            -- 3. Buy 3rd custom item (requires 0 lines of new Lua)
            -- We verify passing any item definition handle works seamlessly
            local custom_potion_def = {
                id = "rh:item.potion.healing",
                price = 15,
            }
            game.runtime.dispatch_command({
                command_id = "rh:command.buy",
                args = { item = custom_potion_def },
                sequence = 1103,
            })
            local r3 = game.runtime.last_command_result
            assert(r3 ~= nil and r3.ok == true, "buy 3rd item definition must succeed without new Lua")
            assert(hero.gold == 50, "gold must be 65 - 15 = 50")
        end)
    end,

    entity_authoring_prototypes_and_method_registration = function()
        game.entity_extensions.with_isolated_extensions(function()
            local mod, env = authoring_context.create_authoring_environment("rh", "rh:authoring.test_entities")

            -- 1. Declare method on Actor prototype
            function env.Actor:spec_add_points(pts)
                self.points = (self.points or 0) + pts
                return self.points
            end

            -- 2. Declare method on Location prototype
            function env.Location:spec_is_peaceful()
                return (self.danger_level or 0) == 0
            end

            -- 3. Declare method on dynamic PascalCase prototype (Faction)
            function env.Faction:spec_reputation_level()
                return "neutral"
            end

            -- 4. Verify methods registered in entity_extension_registry
            assert(game and game.entity_extensions, "game.entity_extensions must exist")
            local actor_fn = game.entity_extensions.get_method("Actor", "spec_add_points")
            assert(type(actor_fn) == "function", "spec_add_points must be registered on Actor")

            local loc_fn = game.entity_extensions.get_method("Location", "spec_is_peaceful")
            assert(type(loc_fn) == "function", "spec_is_peaceful must be registered on Location")

            local faction_fn = game.entity_extensions.get_method("Faction", "spec_reputation_level")
            assert(type(faction_fn) == "function", "spec_reputation_level must be registered on Faction")

            -- 5. Test invoking methods on mock objects
            local test_actor = { points = 10 }
            local new_pts = actor_fn(test_actor, 25)
            assert(new_pts == 35 and test_actor.points == 35)

            local test_loc = { danger_level = 0 }
            assert(loc_fn(test_loc) == true)
            test_loc.danger_level = 3
            assert(loc_fn(test_loc) == false)
        end)
    end,

    entity_method_fail_package_attribution = function()
        game.entity_extensions.with_isolated_extensions(function()
            handler_registry.with_isolated_handlers(function()
                local mod, env = authoring_context.create_authoring_environment("rh", "rh:authoring.test_fail")

                -- Declare an Actor method in package 'rh' that calls fail()
                function env.Actor:spec_require_mana(required_mana)
                    local current = self.mana or 0
                    if current < required_mana then
                        env.fail("insufficient_mana", {
                            current_mana = current,
                            required_mana = required_mana,
                        })
                    end
                end

                -- Declare a command calling the method
                env.commands.test_cast_spell = function(args)
                    local player_actor = { instance_id = "test@0", mana = 5 }
                    local actor_require_mana = game.entity_extensions.get_method("Actor", "spec_require_mana")
                    actor_require_mana(player_actor, 20)
                end

                mod.register({})

                mutation_window.execute_in_window(function()
                    game.runtime.dispatch_command({
                        command_id = "rh:command.test_cast_spell",
                        args = {},
                        sequence = 1201,
                    })
                    local res = game.runtime.last_command_result
                    assert(res ~= nil and res.ok == false, "command must fail when Actor:spec_require_mana fails")
                    assert(res.error ~= nil, "error object must be present")
                    -- Verify error code is canonicalized with package 'rh'
                    assert(res.error.code == "rh:error.insufficient_mana",
                        "error code must be attributed to package 'rh', got: " .. tostring(res.error.code))
                    assert(res.error.params.required_mana == 20)
                    assert(res.error.params.current_mana == 5)
                end)
            end)
        end)
    end,

    managed_properties_validation_with_entity_extensions = function()
        local actor_reg = require("core:module.runtime.actor_registry")
        local properties_mod = require("core:module.authoring.properties")

        properties_mod.with_isolated_state(function()
            game.entity_extensions.with_isolated_extensions(function()
                local reg = actor_reg.create_registry()

                -- Register schema with a managed field requiring operation 'add_mana'
                properties_mod.register_schema("mage", {
                    fields = {
                        mana = {
                            storage = "state",
                            write_policy = "managed",
                            operations = { "add_mana" },
                            schema = { type = "integer" },
                        },
                    },
                })

                -- 1. Without decorator and without extension method, freeze() fails with MissingDomainOperation
                local ok1, err1 = pcall(function()
                    reg.freeze()
                end)
                assert(not ok1, "freeze must fail when managed operation is missing")
                assert(string.find(tostring(err1), "MissingDomainOperation") ~= nil,
                    "error must be MissingDomainOperation, got: " .. tostring(err1))

                -- 2. Register 'add_mana' via game.entity_extensions
                game.entity_extensions.register("rh:authoring.mage", "rh", "Actor", "add_mana", function(self, amt)
                    self.mana = (self.mana or 0) + amt
                end)

                -- 3. Now freeze() succeeds because operation exists in entity_extensions
                local ok2, err2 = pcall(function()
                    reg.freeze()
                end)
                assert(ok2, "freeze must succeed when managed operation is defined in entity_extensions: " .. tostring(err2))
            end)
        end)
    end,

    full_game_tier_actor_and_location_composition = function()
        local save_mod = require("core:module.runtime.save")
        local state_val = require("core:module.runtime.state_validator")
        local properties = require("core:module.authoring.properties")

        mutation_window.execute_in_window(function()
            local p = game.instances.actors.player()
            if not p then
                local hero = game.instances.actors.create("rh:actor.character.hero", { gold = 50, stamina = 100 })
                game.state.meta.player_actor_id = hero.instance_id
                p = hero
            end

            -- TextSystem domain methods
            assert(p:is_player() == true)
            assert(p:is_npc() == false)
            assert(type(p.require_location) == "function")
            assert(type(p.move_to) == "function")

            -- RH domain methods
            assert(type(p.get_gold) == "function")
            assert(type(p.add_gold) == "function")
            assert(type(p.require_gold) == "function")
            assert(type(p.spend_gold) == "function")
            assert(type(p.get_stamina) == "function")
            assert(type(p.add_item) == "function")

            -- Location definition methods
            local market = properties.wrap_definition("rh:location.city.market")
            assert(market ~= nil)
            assert(type(market.is_connected) == "function")
            assert(type(market.require_connected) == "function")

            -- State purity invariant (INV-001, INV-008): methods are not stored in state
            local canonical_codec = require("core:module.runtime.canonical_codec")
            local serialized = canonical_codec.serialize(game.state)
            assert(type(serialized) == "string" and #serialized > 0)
            assert(string.find(serialized, "function") == nil, "serialized state must not contain function references")
        end)
    end,
}
