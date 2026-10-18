-- DLA-05, DLA-06, DLA-07, DLA-08, DLA-09: Designer Commands Specification (ADR-0027, Commands.md)
-- Verifies:
--   1. Commands proxy declaration, duplicate rejection, and unknown key error (not nil)
--   2. Deferred registration and freeze enforcement
--   3. Argument canonicalization into tagged references and rehydration to fresh wrappers
--   4. fail() before mutation returns typed refusal; fail() after mutation raises AuthoringFailAfterMutation
--   5. :run() synchronous execution, rejection on nested call, and :later() deferred execution

local authoring_context = require("core:module.authoring.context")
local authoring_commands = require("core:module.authoring.commands")
local tagged_ref = require("core:module.authoring.tagged_ref")
local mutation_window = require("core:module.runtime.mutation_window")
local test_isolation = require("core:module.runtime.test_isolation")
local command_dispatcher = require("core:module.runtime.command_dispatcher")
local state_hasher = require("core:module.runtime.state_hasher")

local function ensure_player(initial_stamina, initial_gold)
    local player = game.instances.actors.player()
    if not player then
        local hero = game.instances.actors.create("rh:actor.character.hero", {
            stamina = initial_stamina or 20,
            gold = initial_gold or 50,
        })
        game.state.meta.player_actor_id = hero.instance_id
        return hero
    end
    if initial_stamina ~= nil then
        player.stamina = initial_stamina
    end
    if initial_gold ~= nil then
        player.gold = initial_gold
    end
    return player
end

return {
    commands_proxy_declaration_and_descriptor_stability = function()
        test_isolation.with_isolated_handlers(function()
            local M = authoring_context.gameplay("rh")
            assert(M.commands ~= nil, "M.commands proxy must exist")

            -- Access descriptor before assignment
            local pre_desc = M.commands["shop.test_item"]
            assert(pre_desc ~= nil, "Descriptor must be accessible before assignment")
            assert(pre_desc.command_id == "rh:command.shop.test_item", "Canonical ID must match")

            -- Assign handler
            local handler_called = false
            M.commands["shop.test_item"] = function(_arg)
                handler_called = true
            end

            -- Access descriptor after assignment -> must be the same descriptor
            local post_desc = M.commands["shop.test_item"]
            assert(post_desc == pre_desc, "Descriptor must be stable across assignment")

            -- Duplicate assignment must be rejected
            local dup_ok, dup_err = pcall(function()
                M.commands["shop.test_item"] = function() end
            end)
            assert(not dup_ok, "Duplicate command assignment must be rejected")
            assert(string.find(tostring(dup_err), "CommandAlreadyDefined"), "Error must mention CommandAlreadyDefined")

            -- Non-function assignment must be rejected
            local non_fn_ok, non_fn_err = pcall(function()
                M.commands["shop.invalid_type"] = 12345
            end)
            assert(not non_fn_ok, "Non-function command assignment must be rejected")
            assert(string.find(tostring(non_fn_err), "InvalidCommandHandler"), "Error must mention InvalidCommandHandler")
        end)
    end,

    commands_proxy_errors_on_unknown_key_after_freeze = function()
        test_isolation.with_isolated_handlers(function()
            local M = authoring_context.gameplay("rh")
            M.commands.valid_cmd = function() end

            -- Register / freeze module
            M.register()

            -- Declaring after freeze must fail
            local late_ok, late_err = pcall(function()
                M.commands.late_cmd = function() end
            end)
            assert(not late_ok, "Command declaration after freeze must fail")
            assert(string.find(tostring(late_err), "CommandDeclarationAfterFreeze"), "Error must mention CommandDeclarationAfterFreeze")

            -- Accessing unknown key after freeze must throw error, NOT return nil!
            local get_ok, get_err = pcall(function()
                local _ = M.commands.nonexistent_typo_command
            end)
            assert(not get_ok, "Accessing unknown command after freeze must throw error")
            assert(string.find(tostring(get_err), "UnknownCommandKey"), "Error must mention UnknownCommandKey, got: " .. tostring(get_err))
        end)
    end,

    argument_canonicalization_and_rehydration = function()
        test_isolation.with_isolated_handlers(function()
            local player = nil
            mutation_window.execute_in_window(function()
                player = ensure_player(20, 50)
            end)

            local M = authoring_context.gameplay("rh")

            local received_actor = nil
            local received_string = nil
            local received_number = nil
            local received_table = nil

            M.commands.check_args = function(actor_arg, str_arg, num_arg, tbl_arg)
                received_actor = actor_arg
                received_string = str_arg
                received_number = num_arg
                received_table = tbl_arg
                return { ok = true }
            end

            M.register()

            -- Call via :run() passing wrapper, string, number, and table
            local mock_def = { definition_id = "rh:location.city.tavern", __is_definition_handle = true }
            local str_id = "custom_message_tag"
            local res = M.commands.check_args:run(player, str_id, 42, { target = mock_def, count = 3 })

            assert(res ~= nil and res.ok == true, "Command run must succeed")
            assert(received_actor ~= nil, "Actor wrapper must be received")
            assert(received_actor.instance_id == player.instance_id, "Rehydrated actor instance_id must match")
            assert(received_string == str_id, "String must remain a string: got " .. tostring(received_string))
            assert(received_number == 42, "Number must remain 42")
            assert(type(received_table) == "table", "Table must be received")
            assert(received_table.count == 3, "Table count must be 3")
        end)
    end,

    action_produces_semantic_action_dto = function()
        local M = authoring_context.gameplay("rh")
        M.commands.buy = function() end

        local mock_def = { definition_id = "rh:location.city.tavern", __is_definition_handle = true }
        local act = M.action(M.commands.buy, mock_def, 5)

        assert(type(act) == "table", "action() must return a table")
        assert(act.command_id == "rh:command.buy", "command_id must match: " .. tostring(act.command_id))
        assert(type(act.args) == "table", "args must be a table")
        assert(act.args[1].__gv2_ref == "definition", "First arg must be canonicalized tagged definition ref")
        assert(act.args[1].id == "rh:location.city.tavern", "Tagged ref id must match")
        assert(act.args[2] == 5, "Second arg must be 5")
    end,

    fail_before_mutation_returns_typed_refusal = function()
        test_isolation.with_isolated_handlers(function()
            local player = nil
            local initial_gold = 0
            mutation_window.execute_in_window(function()
                player = ensure_player(20, 50)
                initial_gold = player.gold
            end)

            local M = authoring_context.gameplay("rh")
            M.commands.expensive_buy = function(_amount)
                -- Precondition check before state mutation
                if player.gold < 1000 then
                    return M.fail("shop.insufficient_gold", {
                        current = player.gold,
                        required = 1000,
                    })
                end
                player.gold = player.gold - 1000
                return { ok = true }
            end

            M.register()

            local res = M.commands.expensive_buy:run(1)
            assert(type(res) == "table", "Result must be a table")
            assert(res.ok == false, "Command result ok must be false on fail()")
            assert(type(res.error) == "table", "res.error must be a table")
            assert(res.error.code == "rh:error.shop.insufficient_gold", "Error code must match canonical Stable ID: " .. tostring(res.error.code))
            assert(res.error.params.current == initial_gold, "Error params current must match")
            assert(res.error.params.required == 1000, "Error params required must match")
            assert(player.gold == initial_gold, "State must not mutate on refusal")
        end)
    end,

    fail_after_mutation_throws_authoring_fail_after_mutation = function()
        test_isolation.with_isolated_handlers(function()
            mutation_window.execute_in_window(function()
                ensure_player(20, 50)
            end)

            local M = authoring_context.gameplay("rh")
            M.commands.bad_command = function()
                -- State is mutated first!
                local player = game.instances.actors.player()
                player.gold = player.gold - 10

                -- Then fail() is called -> must be caught as programmer error!
                return M.fail("some_error", { details = "bad" })
            end

            M.register()

            local ok, err = pcall(function()
                M.commands.bad_command:run()
            end)

            assert(not ok, "fail() after mutation must raise Lua error / fault")
            assert(string.find(tostring(err), "AuthoringFailAfterMutation"),
                "Error must mention AuthoringFailAfterMutation, got: " .. tostring(err))
        end)
    end,

    -- ADR-0050 (EGC-03): command_dispatcher resolves the mutation-window
    -- transaction itself once the outcome is known, so a thrown Lua error
    -- after mutation must leave canonical state exactly as it was before the
    -- command, proven by hash rather than by reading individual fields. The
    -- sibling trigger (a handler returning `{ ok = false }` as a plain table,
    -- without fail()) is exercised at the programmer-API level in
    -- Tests/Lua/lifecycle/mutation_window.lua: the authoring wrapper's SAS-04
    -- "implicit success" normalization only recognizes fail()'s sentinel, so
    -- a designer handler can reach this trigger only through fail().

    fail_before_mutation_leaves_canonical_state_hash_unchanged = function()
        test_isolation.with_isolated_handlers(function()
            mutation_window.execute_in_window(function()
                ensure_player(20, 50)
            end)
            local before = state_hasher.hash_state(game.state)

            local M = authoring_context.gameplay("rh")
            M.commands.refuse_before_mutation = function()
                return M.fail("shop.insufficient_gold", { current = 50, required = 1000 })
            end
            M.register()

            local res = M.commands.refuse_before_mutation:run()
            assert(type(res) == "table" and res.ok == false, "fail() before mutation must return a table refusal")

            local after = state_hasher.hash_state(game.state)
            assert(after == before, "fail() before mutation must leave canonical state hash unchanged: " .. before .. " vs " .. after)
        end)
    end,

    runtime_error_mid_handler_rolls_back_canonical_state = function()
        test_isolation.with_isolated_handlers(function()
            local player = nil
            mutation_window.execute_in_window(function()
                player = ensure_player(20, 50)
            end)
            local before = state_hasher.hash_state(game.state)

            local M = authoring_context.gameplay("rh")
            M.commands.crash_after_mutation = function()
                player.gold = player.gold - 10
                error("SyntheticHandlerFault: simulated programmer error after mutation", 0)
            end
            M.register()

            local ok, err = pcall(function()
                M.commands.crash_after_mutation:run()
            end)
            assert(not ok, "A thrown Lua error must propagate out of :run()")
            assert(string.find(tostring(err), "SyntheticHandlerFault"), "Error must propagate unchanged, got: " .. tostring(err))

            local after = state_hasher.hash_state(game.state)
            assert(after == before, "A thrown error mid-handler must roll back canonical state: " .. before .. " vs " .. after)
            assert(player.gold == 50, "Gold must be restored to its pre-command value")
        end)
    end,

    successful_command_leaves_single_journal_entry_per_key_and_commits = function()
        test_isolation.with_isolated_handlers(function()
            local player = nil
            mutation_window.execute_in_window(function()
                player = ensure_player(20, 50)
            end)

            local M = authoring_context.gameplay("rh")
            M.commands.pay_twice = function()
                player.gold = player.gold - 5
                player.gold = player.gold - 5
                return { ok = true }
            end
            M.register()

            local res = M.commands.pay_twice:run()
            assert(res.ok == true, "Command must succeed")
            assert(player.gold == 40, "Both writes to the same key must apply: 50 - 5 - 5 = 40")
            assert(not mutation_window.has_mutations("canonical"), "has_mutations must be false once the command's transaction has committed")
        end)
    end,

    nested_run_rejected_from_inside_command_handler = function()
        test_isolation.with_isolated_handlers(function()
            mutation_window.execute_in_window(function()
                ensure_player(20, 50)
            end)

            local M = authoring_context.gameplay("rh")
            M.commands.cmd_inner = function()
                return { ok = true }
            end

            M.commands.cmd_outer = function()
                -- Calling :run() synchronously from inside active handler
                return M.commands.cmd_inner:run()
            end

            M.register()

            local ok, err = pcall(function()
                M.commands.cmd_outer:run()
            end)

            assert(not ok, "Nested :run() from inside command handler must be rejected")
            assert(string.find(tostring(err), "AuthoringNestedRunDisallowed"),
                "Error must mention AuthoringNestedRunDisallowed, got: " .. tostring(err))
        end)
    end,

    later_enqueues_and_executes_in_deferred_queue = function()
        test_isolation.with_isolated_handlers(function()
            local player = nil
            local initial_gold = 0
            mutation_window.execute_in_window(function()
                player = ensure_player(20, 50)
                initial_gold = player.gold
            end)

            local M = authoring_context.gameplay("rh")
            M.commands.deferred_reward = function(amount)
                player.gold = player.gold + amount
                return { ok = true }
            end

            M.register()

            -- Enqueue via :later()
            game.commands.clear_queue()
            local enqueued = M.commands.deferred_reward:later(25)
            assert(enqueued == true, ":later() must return true")

            -- State not changed yet
            assert(player.gold == initial_gold, "Gold must not change before queue pump")

            -- Pump queue
            local count = command_dispatcher.drain_queue()
            assert(count == 1, "One command must be drained from queue")
            assert(player.gold == initial_gold + 25, "Gold must increase after queue execution")
        end)
    end,
}
