-- LSM-05, DLA-02, DLA-03: Mutation Window Specification (ADR-0024, ADR-0027, CommandsAndEvents.md, CanonicalStateAndSave.md)
-- Verifies that state mutations outside the active mutation window are rejected,
-- that mutation is permitted within execute_in_window, that nested tables are protected,
-- that write_revision monotonically increments on every state mutation (direct, nested, table.insert/remove, wrapped decorator),
-- and that raw canonical state is strictly isolated (unwrap_state is not exported).

local mutation_window = require("core:module.runtime.mutation_window")
local state_hasher = require("core:module.runtime.state_hasher")
local test_isolation = require("core:module.runtime.test_isolation")

return {
    direct_mutation_outside_window_rejected = function()
        assert(game and game.state, "game.state must exist")

        local ok, err = pcall(function()
            game.state.world.unauthorized_flag = true
        end)
        assert(not ok, "Direct mutation to game.state.world outside mutation window must be rejected")
        assert(string.find(tostring(err), "MutationWindowClosed"), "Error must mention MutationWindowClosed, got: " .. tostring(err))
    end,

    direct_mutation_inside_window_succeeds = function()
        mutation_window.execute_in_window(function()
            game.state.world.authorized_flag = "spec_ok"
        end)
        assert(game.state.world.authorized_flag == "spec_ok", "State write inside window must succeed")

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.world.authorized_flag = nil
        end)
    end,

    nested_table_mutation_outside_window_rejected = function()
        mutation_window.execute_in_window(function()
            game.state.world.nested_test = { val = 1 }
        end)

        local ok, err = pcall(function()
            game.state.world.nested_test.val = 2
        end)
        assert(not ok, "Nested table mutation outside window must be rejected")
        assert(string.find(tostring(err), "MutationWindowClosed"), "Error must mention MutationWindowClosed, got: " .. tostring(err))

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.world.nested_test = nil
        end)
    end,

    nested_table_mutation_inside_window_succeeds = function()
        mutation_window.execute_in_window(function()
            game.state.world.nested_test2 = { counter = 10 }
        end)

        mutation_window.execute_in_window(function()
            game.state.world.nested_test2.counter = 20
        end)
        assert(game.state.world.nested_test2.counter == 20, "Nested write inside window must succeed")

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.world.nested_test2 = nil
        end)
    end,

    write_revision_increments_on_direct_mutation = function()
        local r0 = mutation_window.write_revision()
        assert(type(r0) == "number", "write_revision must return a number")

        mutation_window.execute_in_window(function()
            game.state.world.rev_test_direct = 42
        end)
        local r1 = mutation_window.write_revision()
        assert(r1 > r0, "write_revision must increment on direct field write: " .. r1 .. " > " .. r0)

        -- Multiple mutations in one window produce multiple increments
        mutation_window.execute_in_window(function()
            game.state.world.rev_test_direct = 43
            game.state.world.rev_test_direct = 44
        end)
        local r2 = mutation_window.write_revision()
        assert(r2 >= r1 + 2, "write_revision must increment per mutation: " .. r2 .. " >= " .. (r1 + 2))

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.world.rev_test_direct = nil
        end)
    end,

    write_revision_increments_on_nested_mutation = function()
        mutation_window.execute_in_window(function()
            game.state.world.rev_nested = { sub = { count = 0 } }
        end)
        local r0 = mutation_window.write_revision()

        mutation_window.execute_in_window(function()
            game.state.world.rev_nested.sub.count = 1
        end)
        local r1 = mutation_window.write_revision()
        assert(r1 > r0, "write_revision must increment on deeply nested mutation")

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.world.rev_nested = nil
        end)
    end,

    write_revision_increments_on_table_methods = function()
        mutation_window.execute_in_window(function()
            game.state.world.rev_list = { 10, 20 }
        end)
        local r0 = mutation_window.write_revision()

        -- table.insert on guarded proxy table
        mutation_window.execute_in_window(function()
            table.insert(game.state.world.rev_list, 30)
        end)
        local r1 = mutation_window.write_revision()
        assert(r1 > r0, "write_revision must increment on table.insert")
        assert(#game.state.world.rev_list == 3, "list length must be 3")
        assert(game.state.world.rev_list[3] == 30, "inserted value must match")

        -- table.remove on guarded proxy table
        mutation_window.execute_in_window(function()
            table.remove(game.state.world.rev_list, 1)
        end)
        local r2 = mutation_window.write_revision()
        assert(r2 > r1, "write_revision must increment on table.remove")
        assert(#game.state.world.rev_list == 2, "list length must be 2")
        assert(game.state.world.rev_list[1] == 20, "first element must now be 20")

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.world.rev_list = nil
        end)
    end,

    write_revision_increments_via_wrapped_decorator = function()
        local actor_reg_mod = require("core:module.runtime.actor_registry")
        local reg = actor_reg_mod.create_registry()
        reg.register_type("test_hero", function(base)
            return setmetatable({
                level_up = function(self)
                    self.level = (self.level or 1) + 1
                end,
            }, { __index = base })
        end)

        mutation_window.execute_in_window(function()
            game.state.actors["actor@1:99"] = {
                instance_id = "actor@1:99",
                definition_id = "core:actor.test",
                discriminator = "test_hero",
                level = 1,
            }
        end)

        local r0 = mutation_window.write_revision()
        mutation_window.execute_in_window(function()
            local wrapped = reg.wrap(game.state.actors["actor@1:99"])
            wrapped:level_up()
        end)
        local r1 = mutation_window.write_revision()
        assert(r1 > r0, "write_revision must increment on wrapped decorator mutation: " .. r1 .. " > " .. r0)
        assert(game.state.actors["actor@1:99"].level == 2, "level must be 2")

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.actors["actor@1:99"] = nil
        end)
    end,

    raw_state_isolated_and_unwrap_state_not_exported = function()
        -- DLA-03: unwrap_state is absent from public exports
        assert(mutation_window.unwrap_state == nil, "mutation_window.unwrap_state must NOT be exported")
        assert(type(mutation_window.guard_state) == "function", "guard_state must be exported for host")
        assert(type(mutation_window.write_revision) == "function", "write_revision must be exported")
    end,

    -- ADR-0050: transaction journal and rollback (EGC-03).
    -- The deferred begin_transaction/commit_transaction/rollback_transaction
    -- entry point is what command_dispatcher uses; these specs exercise it
    -- directly, independently of the dispatcher, at the mutation_window level.

    transaction_commit_discards_journal_and_keeps_writes = function()
        mutation_window.begin_transaction()
        game.state.world.txn_commit_test = "kept"
        mutation_window.commit_transaction()
        assert(game.state.world.txn_commit_test == "kept", "Committed write must survive commit")
        assert(not mutation_window.has_mutations("canonical"), "has_mutations must be false once the transaction is resolved")

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.world.txn_commit_test = nil
        end)
    end,

    transaction_rollback_restores_absent_sparse_key_as_absent = function()
        local before = state_hasher.hash_state(game.state)

        mutation_window.begin_transaction()
        assert(not mutation_window.has_mutations("canonical"), "Opening a transaction must not itself count as a mutation")
        game.state.world.txn_sparse_key = { [5] = "gap" }
        assert(mutation_window.has_mutations("canonical"), "has_mutations must be true once a canonical write happened")
        mutation_window.rollback_transaction()

        local after = state_hasher.hash_state(game.state)
        assert(after == before, "Rollback of a key absent before the transaction must restore the hash exactly: " .. before .. " vs " .. after)

        local ok = pcall(function()
            return game.state.world.txn_sparse_key
        end)
        assert(ok, "Reading an absent key must not itself error")
        assert(game.state.world.txn_sparse_key == nil, "Key absent before the transaction must be absent after rollback, not present with nil")
    end,

    transaction_rollback_restores_nested_table_mutation = function()
        mutation_window.execute_in_window(function()
            game.state.world.txn_nested = { sub = { count = 1 } }
        end)
        local before = state_hasher.hash_state(game.state)

        mutation_window.begin_transaction()
        game.state.world.txn_nested.sub.count = 99
        assert(game.state.world.txn_nested.sub.count == 99, "Write must be visible inside the open transaction")
        mutation_window.rollback_transaction()

        local after = state_hasher.hash_state(game.state)
        assert(after == before, "Rollback must restore a deeply nested mutation: " .. before .. " vs " .. after)
        assert(game.state.world.txn_nested.sub.count == 1, "Nested value must be restored to its prior value")

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.world.txn_nested = nil
        end)
    end,

    transaction_rollback_only_records_first_prior_value_on_repeated_write = function()
        mutation_window.execute_in_window(function()
            game.state.world.txn_repeated_key = 1
        end)
        local before = state_hasher.hash_state(game.state)

        mutation_window.begin_transaction()
        game.state.world.txn_repeated_key = 2
        game.state.world.txn_repeated_key = 3
        game.state.world.txn_repeated_key = 4
        mutation_window.rollback_transaction()

        local after = state_hasher.hash_state(game.state)
        assert(after == before, "Rollback after repeated writes to the same key must restore the pre-transaction value, not an intermediate one")
        assert(game.state.world.txn_repeated_key == 1, "Repeated writes to the same key must roll back to the value before the FIRST write")

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.world.txn_repeated_key = nil
        end)
    end,

    transaction_rollback_preserves_boolean_false_old_value = function()
        -- Guards against an and/or-idiom bug that would read a stored `false`
        -- as "absent" and restore it as nil instead of false.
        mutation_window.execute_in_window(function()
            game.state.world.txn_bool_flag = false
        end)
        local before = state_hasher.hash_state(game.state)

        mutation_window.begin_transaction()
        game.state.world.txn_bool_flag = true
        mutation_window.rollback_transaction()

        local after = state_hasher.hash_state(game.state)
        assert(after == before, "Rollback must restore a stored boolean false, not drop it as absent")
        assert(game.state.world.txn_bool_flag == false, "Value must be exactly false, not nil: got " .. tostring(game.state.world.txn_bool_flag))

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.world.txn_bool_flag = nil
        end)
    end,

    transaction_error_mid_write_rolls_back_via_dedicated_entry_point = function()
        mutation_window.execute_in_window(function()
            game.state.world.txn_error_mid = { a = 1, b = 2 }
        end)
        local before = state_hasher.hash_state(game.state)

        mutation_window.begin_transaction()
        local ok, err = pcall(function()
            game.state.world.txn_error_mid.a = 100
            error("SyntheticHandlerFault: simulated failure mid-handler", 0)
        end)
        assert(not ok, "Synthetic fault must propagate as an error")
        mutation_window.rollback_transaction()

        local after = state_hasher.hash_state(game.state)
        assert(after == before, "Rollback after an error thrown mid-transaction must restore the pre-transaction hash: " .. before .. " vs " .. after)
        assert(game.state.world.txn_error_mid.a == 1, "Field mutated before the error must be restored")

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.world.txn_error_mid = nil
        end)
    end,

    unresolved_transaction_before_next_begin_raises_error = function()
        mutation_window.begin_transaction()
        game.state.world.txn_unresolved = "left_open"

        local ok, err = pcall(function()
            mutation_window.begin_transaction()
        end)
        assert(not ok, "Opening a new transaction while the previous one is unresolved must fail, not silently discard it")
        assert(string.find(tostring(err), "MutationWindowTransactionUnresolved"),
            "Error must mention MutationWindowTransactionUnresolved, got: " .. tostring(err))

        -- Resolve the still-open transaction and clean up.
        mutation_window.rollback_transaction()
        assert(game.state.world.txn_unresolved == nil, "Rolling back the left-open transaction must still restore absence")
    end,

    dispatcher_rolls_back_canonical_state_on_table_refusal_after_mutation = function()
        -- ADR-0050 trigger 2: a handler returning `{ ok = false }` as a plain
        -- table (not via fail()'s sentinel) after already mutating state.
        -- Registered directly against game.commands.handlers, bypassing the
        -- authoring wrapper's SAS-04 "implicit success" normalization, which
        -- would otherwise wrap a raw `{ ok = false }` return as a success
        -- value instead of letting it reach command_dispatcher unchanged.
        test_isolation.with_isolated_handlers(function()
            mutation_window.execute_in_window(function()
                game.state.world.txn_refusal_probe = 1
            end)
            local before = state_hasher.hash_state(game.state)

            game.commands.handlers.register("core:command.test.mutation_window_refusal", function()
                game.state.world.txn_refusal_probe = 999
                return { ok = false, error = { code = "core:error.test.refused", params = {} } }
            end)

            local seq = game.runtime.dispatch_command({ command_id = "core:command.test.mutation_window_refusal" })
            local result = game.runtime.last_command_result
            assert(type(result) == "table" and result.ok == false, "Dispatch must report the table refusal unchanged")

            local after = state_hasher.hash_state(game.state)
            assert(after == before, "Table refusal after mutation must roll back canonical state: " .. before .. " vs " .. after)
            assert(game.state.world.txn_refusal_probe == 1, "Field mutated before the refusal must be restored")
        end)

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.world.txn_refusal_probe = nil
        end)
    end,

    dispatcher_commits_before_resolve_so_a_failing_presentation_builder_does_not_roll_back = function()
        -- ADR-0050/0051: the transaction is resolved (here: committed) before
        -- facts are delivered and before game.presentation.resolve() runs, so
        -- an error thrown by the presentation builder must not undo a
        -- canonical write the command already committed.
        if not (game and game.presentation and game.presentation.register_source) then
            return
        end

        -- game.presentation is shared, un-isolated session state (no
        -- facade-slot isolation exists for it, unlike handlers/services/
        -- actors/validators), so the real production source and its frozen
        -- flag are captured and restored exactly, regardless of what other
        -- spec files later assume about it.
        local original_source = game.presentation.get_source()
        local was_frozen = game.presentation.is_frozen()

        local restore_ok, restore_err = pcall(function()
            test_isolation.with_isolated_handlers(function()
                game.presentation.clear_for_test()
                game.presentation.register_source(function()
                    error("SyntheticPresentationBuilderFault: simulated failing presentation source", 0)
                end)

                game.commands.handlers.register("core:command.test.mutation_window_resolve_fault", function()
                    game.state.world.txn_resolve_fault_probe = "committed"
                    return { ok = true }
                end)

                local ok, err = pcall(function()
                    game.runtime.dispatch_command({ command_id = "core:command.test.mutation_window_resolve_fault" })
                end)
                assert(not ok, "A failing presentation builder must propagate its error out of dispatch")
                assert(string.find(tostring(err), "SyntheticPresentationBuilderFault"),
                    "Error must propagate from the presentation builder, got: " .. tostring(err))

                assert(game.state.world.txn_resolve_fault_probe == "committed",
                    "The canonical write already committed before resolve() must survive a presentation-builder error")
            end)
        end)

        -- ADR-0051 explicitly leaves presentation-error handling as today's
        -- behavior ("обработка такой ошибки остаётся сегодняшней"): the
        -- dispatcher's phase cleanup on this path only resets from
        -- "executing_command", not from "pumping_events" left behind by a
        -- completed event pump, so this probe restores it itself rather than
        -- changing dispatcher behavior outside this task's scope.
        if game and game.runtime and game.runtime.phase == "pumping_events" then
            game.runtime.phase = "idle"
        end

        game.presentation.clear_for_test()
        if original_source ~= nil then
            game.presentation.register_source(original_source)
        end
        if was_frozen then
            game.presentation.freeze()
        end

        if not restore_ok then
            error(restore_err, 0)
        end

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.world.txn_resolve_fault_probe = nil
        end)
    end,

    has_mutations_false_without_any_transaction = function()
        -- Between transactions (and under plain execute_in_window, which never
        -- opens one), has_mutations must not spuriously report a leftover
        -- category from a prior, already-resolved transaction.
        mutation_window.begin_transaction()
        game.state.world.txn_leftover_check = 1
        mutation_window.commit_transaction()

        assert(not mutation_window.has_mutations("canonical"), "has_mutations must be false once no transaction is active")

        mutation_window.execute_in_window(function()
            game.state.world.txn_leftover_check = nil
        end)
    end,
}
