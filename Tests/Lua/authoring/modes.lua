-- EGC-05/EGC-06: Mode Store and `modes` Authoring Surface Specification (ADR-0050, ADR-0051)
-- Verifies:
--   1. modes.<key> = { start, view } declaration; missing view, duplicate and
--      late declaration are rejected the same way services are.
--   2. modes.<key>.<field> = field.*(...) declares run content the same way
--      Actor.<field> = field.*(...) declares an entity field.
--   3. start/require_active/state/finish/cancel/active lifecycle, including
--      finish()+start() of a different mode within one command.
--   4. mode.not_active / mode.already_active typed refusals.
--   5. Writing an undeclared run field, and writing a managed (random_stream)
--      field directly, are rejected the same way an entity field is.
--   6. field.random_stream() seeding on start() and shape validation before
--      a draw.
--   7. view() can only be called outside command scope, and cannot mutate
--      run even when called correctly (mutation window is closed outside a
--      command, independent of this guard).
--   8. core:command.session.save refuses with core:error.mode.active while a
--      mode is active.
--   9. EGC-06: run writes journal through the same transaction as canonical
--      writes -- fail() after a run-only write is a normal refusal (not
--      AuthoringFailAfterMutation) and rolls run back; a mixed canonical+run
--      error rolls back both in the one journal; a run-only command does not
--      change the canonical state hash.
--  10. EGC-06: cancel() without a declared random_stream leaves the hash
--      exactly unchanged; cancel() with one leaves exactly one divergence
--      (core:random_stream.mode_seed), proven by neutralizing that one word
--      and re-hashing rather than comparing one full hash.
--  11. EGC-06: a failed draw rolls back the run-local stream word-for-word;
--      replaying from the same stream state gives the same draw.
--  12. EGC-06: start()/finish()/cancel() publish core:event.mode.started/
--      finished post-commit (outcome "finished"/"cancelled"); a rolled-back
--      start() publishes nothing.
--  13. EGC-07: the Tests/Fixtures/SyntheticMechanicalFixture/synthetic
--      package is the first consumer of the mode mechanism outside
--      Tests/Lua itself (plan rule 2) -- full lifecycle through its real,
--      bootstrap-registered commands (synthetic:command.*), not modes.*
--      called directly from the spec. Covers both halves of EGC-06's
--      cancel() hash criterion (with/without a declared stream), a failed
--      draw's rollback/replay, core:command.session.save refusing while
--      active and succeeding after finish(), core:command.session.load not
--      refusing while active, a saved envelope carrying no run data, event
--      consumption by the fixture's own subscriber, and no leak of the
--      active mode between spec cases.

local authoring_context = require("core:module.authoring.context")
local test_isolation = require("core:module.runtime.test_isolation")
local mode_store = require("core:module.runtime.mode_store")
local field = require("core:module.authoring.field")
local state_hasher = require("core:module.runtime.state_hasher")
local mutation_window = require("core:module.runtime.mutation_window")
local save_module = require("core:module.runtime.save")
local load_module = require("core:module.runtime.load")
local outbound = require("core:module.boundary.outbound")

-- EGC-07: synthetic:command.* are real, bootstrap-registered commands (not
-- isolated per case like the EGC-05/06 fixtures above, since isolating the
-- handler registry would hide them). Nothing resets mode_store between
-- these cases automatically, so each one defensively clears any active mode
-- before and after its body -- a failure partway through must not leak a
-- stuck mode into whatever spec case (isolated or not) runs next.
local function egc07_ensure_mode_cleared()
    if mode_store.active() ~= nil then
        game.runtime.dispatch_command({ command_id = "synthetic:command.cancel_duel" })
    end
end

local function egc07_run_case(body)
    egc07_ensure_mode_cleared()
    local ok, err = pcall(body)
    egc07_ensure_mode_cleared()
    if not ok then
        error(err, 0)
    end
end

return {
    mode_declaration_requires_view = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            local ok, err = pcall(function()
                M.modes.no_view = { start = function() end }
            end)
            assert(not ok, "declaring a mode without view must fail")
            assert(string.find(tostring(err), "ModeMissingView"), "Error must mention ModeMissingView, got: " .. tostring(err))
        end)
    end,

    mode_declaration_requires_start = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            local ok, err = pcall(function()
                M.modes.no_start = { view = function() end }
            end)
            assert(not ok, "declaring a mode without start must fail")
            assert(string.find(tostring(err), "InvalidModeDeclaration"), "Error must mention InvalidModeDeclaration, got: " .. tostring(err))
        end)
    end,

    mode_duplicate_declaration_rejected = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() end }

            local ok, err = pcall(function()
                M.modes.duel = { start = function() end, view = function() end }
            end)
            assert(not ok, "declaring the same mode key twice must fail")
            assert(string.find(tostring(err), "ModeDuplicateDeclaration"), "Error must mention ModeDuplicateDeclaration, got: " .. tostring(err))
        end)
    end,

    mode_declaration_after_freeze_rejected = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() end }
            M.register({})

            local ok, err = pcall(function()
                M.modes.late = { start = function() end, view = function() end }
            end)
            assert(not ok, "declaring a mode after freeze must fail")
            assert(string.find(tostring(err), "ModeDeclarationAfterFreeze"), "Error must mention ModeDeclarationAfterFreeze, got: " .. tostring(err))
        end)
    end,

    mode_reserved_verb_name_cannot_be_declared = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            local ok, err = pcall(function()
                M.modes.start = { start = function() end, view = function() end }
            end)
            assert(not ok, "declaring a mode named 'start' must fail")
            assert(string.find(tostring(err), "ModeReservedKeyName"), "Error must mention ModeReservedKeyName, got: " .. tostring(err))
        end)
    end,

    unknown_mode_key_raises_not_returns_nil = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            local ok, err = pcall(function()
                local _ = M.modes.nonexistent_typo
            end)
            assert(not ok, "reading an unknown modes key must raise, not return nil")
            assert(string.find(tostring(err), "UnknownModeKey"), "Error must mention UnknownModeKey, got: " .. tostring(err))
        end)
    end,

    mode_field_declared_via_handle_like_entity_prototype = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() end }
            M.modes.duel.rounds_won = field.integer({ default = 0 })

            local ok, err = pcall(function()
                M.modes.duel.rounds_won = field.integer()
            end)
            assert(not ok, "declaring the same run field twice must fail")
            assert(string.find(tostring(err), "ModeFieldDuplicateDeclaration"), "Error must mention ModeFieldDuplicateDeclaration, got: " .. tostring(err))
        end)
    end,

    mode_field_descriptor_type_enforced = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() end }

            local ok, err = pcall(function()
                M.modes.duel.rounds_won = 5
            end)
            assert(not ok, "a non field.* value must be rejected")
            assert(string.find(tostring(err), "InvalidModeFieldDescriptor"), "Error must mention InvalidModeFieldDescriptor, got: " .. tostring(err))
        end)
    end,

    mode_multiple_random_streams_rejected = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() end }
            M.modes.duel.dice = field.random_stream()

            local ok, err = pcall(function()
                M.modes.duel.luck = field.random_stream()
            end)
            assert(not ok, "a second random_stream field on the same mode must be rejected")
            assert(string.find(tostring(err), "ModeMultipleRandomStreams"), "Error must mention ModeMultipleRandomStreams, got: " .. tostring(err))
        end)
    end,

    require_active_without_mode_gives_typed_refusal = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.commands.probe = function()
                M.modes.require_active()
            end
            M.register({})

            local res = M.commands.probe:run()
            assert(res.ok == false, "require_active() without an active mode must refuse")
            assert(res.error.code == "core:error.mode.not_active", "Error code must be core:error.mode.not_active, got: " .. tostring(res.error.code))
        end)
    end,

    start_while_active_gives_typed_refusal = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() end }
            M.modes.trade = { start = function() end, view = function() end }

            M.commands.start_duel = function() M.modes.start("duel") end
            M.commands.start_trade_while_active = function() M.modes.start("trade") end
            M.register({})

            local res1 = M.commands.start_duel:run()
            assert(res1.ok == true, "starting the first mode must succeed")
            assert(M.modes.active() == "rh:mode.duel", "duel must be the active mode")

            local res2 = M.commands.start_trade_while_active:run()
            assert(res2.ok == false, "starting a mode while one is active must refuse")
            assert(res2.error.code == "core:error.mode.already_active", "Error code must be core:error.mode.already_active, got: " .. tostring(res2.error.code))
            assert(M.modes.active() == "rh:mode.duel", "the original mode must still be active after the refused start")
        end)
    end,

    finish_then_start_other_mode_in_one_command_succeeds = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() end }
            M.modes.trade = { start = function() end, view = function() end }

            M.commands.start_duel = function() M.modes.start("duel") end
            M.commands.switch_to_trade = function()
                M.modes.finish()
                M.modes.start("trade")
            end
            M.register({})

            assert(M.commands.start_duel:run().ok == true, "starting duel must succeed")
            assert(M.modes.active() == "rh:mode.duel")

            local res = M.commands.switch_to_trade:run()
            assert(res.ok == true, "finish() followed by start() of another mode in one command must succeed")
            assert(M.modes.active() == "rh:mode.trade", "trade must be the active mode after the switch")
        end)
    end,

    finish_without_active_mode_gives_typed_refusal = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.commands.finish_nothing = function() M.modes.finish() end
            M.register({})

            local res = M.commands.finish_nothing:run()
            assert(res.ok == false, "finish() without an active mode must refuse")
            assert(res.error.code == "core:error.mode.not_active", "Error code must be core:error.mode.not_active, got: " .. tostring(res.error.code))
        end)
    end,

    cancel_ends_active_mode = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() end }
            M.commands.start_duel = function() M.modes.start("duel") end
            M.commands.cancel_duel = function() M.modes.cancel() end
            M.register({})

            assert(M.commands.start_duel:run().ok == true)
            assert(M.modes.active() == "rh:mode.duel")

            local res = M.commands.cancel_duel:run()
            assert(res.ok == true, "cancel() of an active mode must succeed")
            assert(M.modes.active() == nil, "no mode must be active after cancel()")
        end)
    end,

    undeclared_run_field_write_rejected_like_entity_field = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() end }
            M.modes.duel.rounds_won = field.integer({ default = 0 })

            M.commands.start_duel = function() M.modes.start("duel") end
            M.commands.write_unknown = function()
                M.modes.state().not_declared = 1
            end
            M.register({})

            assert(M.commands.start_duel:run().ok == true)

            local ok, err = pcall(function()
                M.commands.write_unknown:run()
            end)
            assert(not ok, "writing an undeclared run field must raise")
            assert(string.find(tostring(err), "ModeRunFieldUnknown"), "Error must mention ModeRunFieldUnknown, got: " .. tostring(err))
        end)
    end,

    declared_run_field_round_trips_through_state = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() end }
            M.modes.duel.rounds_won = field.integer({ default = 0 })

            M.commands.start_duel = function() M.modes.start("duel") end
            M.commands.win_round = function()
                M.modes.state().rounds_won = M.modes.state().rounds_won + 1
            end
            M.register({})

            assert(M.commands.start_duel:run().ok == true)
            assert(M.modes.state().rounds_won == 0, "unwritten run field must read its default")

            assert(M.commands.win_round:run().ok == true)
            assert(M.modes.state().rounds_won == 1, "written run field must round-trip")
        end)
    end,

    managed_random_stream_field_cannot_be_written_directly = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() end }
            M.modes.duel.dice = field.random_stream()

            M.commands.start_duel = function() M.modes.start("duel") end
            M.commands.poke_dice = function()
                M.modes.state().dice = { algorithm = "xoshiro128ss-v1", s0 = "00000000", s1 = "00000000", s2 = "00000000", s3 = "00000000" }
            end
            M.register({})

            assert(M.commands.start_duel:run().ok == true)

            local ok, err = pcall(function()
                M.commands.poke_dice:run()
            end)
            assert(not ok, "writing a managed random_stream field directly must raise")
            assert(string.find(tostring(err), "ModeRunFieldManaged"), "Error must mention ModeRunFieldManaged, got: " .. tostring(err))
        end)
    end,

    random_stream_field_seeded_on_start = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() end }
            M.modes.duel.dice = field.random_stream()

            M.commands.start_duel = function() M.modes.start("duel") end
            M.register({})

            assert(M.commands.start_duel:run().ok == true)
            local dice = M.modes.state().dice
            assert(type(dice) == "table", "random_stream field must be seeded on start()")
            assert(dice.algorithm == "xoshiro128ss-v1", "seeded stream must carry the algorithm tag")
            for _, word in ipairs({ "s0", "s1", "s2", "s3" }) do
                assert(type(dice[word]) == "string" and dice[word]:match("^[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]$"),
                    "word '" .. word .. "' must be 8 lowercase hex characters, got: " .. tostring(dice[word]))
            end
        end)
    end,

    corrupted_random_stream_word_rejected_by_descriptor_before_draw = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() end }
            M.modes.duel.dice = field.random_stream()

            M.commands.start_duel = function() M.modes.start("duel") end
            M.commands.corrupt_dice = function()
                -- Reaches into the raw run proxy directly (not modes.state()):
                -- the field wrapper's write guard would itself reject this
                -- write as a managed field, which is a different fixture.
                -- This simulates state that reached run some other way (e.g.
                -- a future load path) with an already-corrupted word.
                mode_store.run_proxy().dice.s0 = "not-hex!"
            end
            M.register({})

            assert(M.commands.start_duel:run().ok == true)
            assert(M.commands.corrupt_dice:run().ok == true)

            local decl = mode_store.get_declaration("rh:mode.duel")
            assert(decl ~= nil, "mode declaration must be retrievable from mode_store")

            local draw_ok, draw_err = pcall(function()
                mode_store.draw_u32("dice", decl.fields.dice)
            end)
            assert(not draw_ok, "a corrupted word must be rejected before the draw's bit arithmetic runs")
            assert(string.find(tostring(draw_err), "FieldValidationConstraintFailed"), "Error must mention FieldValidationConstraintFailed, got: " .. tostring(draw_err))
        end)
    end,

    view_cannot_be_called_from_a_command = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() return { screen_id = "rh:screen.duel" } end }
            M.commands.start_duel = function() M.modes.start("duel") end
            M.commands.call_view_from_command = function()
                authoring_context.call_active_mode_view()
            end
            M.register({})

            assert(M.commands.start_duel:run().ok == true)

            local ok, err = pcall(function()
                M.commands.call_view_from_command:run()
            end)
            assert(not ok, "calling view() from within a command must raise")
            assert(string.find(tostring(err), "ModeViewCalledFromCommand"), "Error must mention ModeViewCalledFromCommand, got: " .. tostring(err))
        end)
    end,

    view_is_a_pure_projection_mutation_attempt_refused = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = {
                start = function() end,
                view = function(_canonical_state, run)
                    run.attempted_mutation = true
                    return { screen_id = "rh:screen.duel" }
                end,
            }
            M.modes.duel.attempted_mutation = field.boolean({ default = false })
            M.commands.start_duel = function() M.modes.start("duel") end
            M.register({})

            assert(M.commands.start_duel:run().ok == true)

            -- Called from outside any command: the guard passes, but the
            -- mutation window is closed here regardless (ADR-0051 §D3: view
            -- is a pure projection), so the attempted write still fails.
            local ok, err = pcall(function()
                authoring_context.call_active_mode_view()
            end)
            assert(not ok, "a view() that attempts to mutate run must fail")
            assert(string.find(tostring(err), "MutationWindowClosed"), "Error must mention MutationWindowClosed, got: " .. tostring(err))
        end)
    end,

    view_returns_route_when_no_mutation_attempted = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = {
                start = function() end,
                view = function(_canonical_state, _run)
                    return { screen_id = "rh:screen.duel" }
                end,
            }
            M.commands.start_duel = function() M.modes.start("duel") end
            M.register({})

            assert(M.commands.start_duel:run().ok == true)

            local route = authoring_context.call_active_mode_view()
            assert(type(route) == "table" and route.screen_id == "rh:screen.duel", "view() must return the route when it does not mutate")
        end)
    end,

    view_returns_nil_without_active_mode = function()
        test_isolation.with_isolated_context(function()
            local route = authoring_context.call_active_mode_view()
            assert(route == nil, "call_active_mode_view() without an active mode must return nil")
        end)
    end,

    session_save_refused_while_mode_active = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() end }
            M.commands.start_duel = function() M.modes.start("duel") end
            M.register({})

            assert(M.commands.start_duel:run().ok == true)

            -- Calls session_controls' handler directly: with_isolated_context
            -- swaps game.commands.handlers for a fresh registry, so the real
            -- core:command.session.save handler registered at bootstrap is
            -- not reachable through game.runtime.dispatch_command here.
            local session_controls = require("core:module.runtime.session_controls")
            local res = session_controls.handle_save({ args = { slot_id = "slot_1" } })
            assert(res ~= nil and res.ok == false, "save must be refused while a mode is active")
            assert(res.error.code == "core:error.mode.active", "Error code must be core:error.mode.active, got: " .. tostring(res.error.code))
        end)
    end,

    unresolved_mode_run_leak_fixture_isolated_between_cases_case_a_starts_mode = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() end }
            M.commands.start_duel = function() M.modes.start("duel") end
            M.register({})
            assert(M.commands.start_duel:run().ok == true)
            assert(M.modes.active() == "rh:mode.duel")
        end)
    end,

    unresolved_mode_run_leak_fixture_isolated_between_cases_case_b_sees_no_active_mode = function()
        assert(mode_store.active() == nil, "mode_store must not leak an active mode across spec cases")
    end,

    -- EGC-06: run writes share the canonical journal (ADR-0050 single-journal rule).

    egc06_fail_after_run_only_write_is_normal_refusal_and_rolls_back_run = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() end }
            M.modes.duel.counter = field.integer({ default = 0 })
            M.commands.start_duel = function() M.modes.start("duel") end
            M.commands.bump_then_fail = function()
                M.modes.state().counter = M.modes.state().counter + 1
                M.fail("too_far")
            end
            M.register({})

            assert(M.commands.start_duel:run().ok == true)
            assert(M.modes.state().counter == 0)

            local res = M.commands.bump_then_fail:run()
            assert(res.ok == false, "fail() after a run-only write must be a normal refusal")
            assert(res.error.code == "rh:error.too_far",
                "refusal must be the package's own fail() code, not AuthoringFailAfterMutation, got: " .. tostring(res.error.code))
            assert(M.modes.state().counter == 0, "the run write before fail() must roll back")
        end)
    end,

    egc06_mixed_canonical_and_run_error_rolls_back_both_in_one_journal = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() end }
            M.modes.duel.counter = field.integer({ default = 0 })
            M.commands.start_duel = function() M.modes.start("duel") end
            M.commands.mixed_fault = function()
                game.state.world.egc06_mixed_probe = "mutated"
                M.modes.state().counter = 7
                error("SyntheticMixedFault: simulated failure after canonical and run writes", 0)
            end
            M.register({})

            assert(M.commands.start_duel:run().ok == true)
            local hash_before = state_hasher.hash_state(game.state)
            local counter_before = M.modes.state().counter

            local ok, err = pcall(function() M.commands.mixed_fault:run() end)
            assert(not ok, "a thrown error must propagate out of :run()")
            assert(string.find(tostring(err), "SyntheticMixedFault"), "error must propagate, got: " .. tostring(err))

            assert(state_hasher.hash_state(game.state) == hash_before, "canonical state must roll back")
            assert(M.modes.state().counter == counter_before, "run state must roll back in the same journal")
            assert(game.state.world.egc06_mixed_probe == nil, "canonical probe field must not persist")
        end)
    end,

    egc06_run_only_command_does_not_change_state_hash = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() end }
            M.modes.duel.counter = field.integer({ default = 0 })
            M.commands.start_duel = function() M.modes.start("duel") end
            M.commands.bump_counter = function()
                M.modes.state().counter = M.modes.state().counter + 1
            end
            M.register({})

            assert(M.commands.start_duel:run().ok == true)
            local hash_before = state_hasher.hash_state(game.state)

            assert(M.commands.bump_counter:run().ok == true)

            assert(state_hasher.hash_state(game.state) == hash_before,
                "a command that only writes run must not change the canonical state hash")
            assert(M.modes.state().counter == 1)
        end)
    end,

    -- EGC-06: cancel() hash-equality criterion is two-part (ADR-0051 §D5).

    egc06_cancel_without_random_stream_leaves_hash_exactly_unchanged = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() end }
            M.commands.start_duel = function() M.modes.start("duel") end
            M.commands.cancel_duel = function() M.modes.cancel() end
            M.register({})

            local hash_before = state_hasher.hash_state(game.state)
            assert(M.commands.start_duel:run().ok == true)
            assert(M.commands.cancel_duel:run().ok == true)
            local hash_after = state_hasher.hash_state(game.state)

            assert(hash_after == hash_before, "cancel() of a stream-less mode must leave the hash exactly unchanged")
        end)
    end,

    egc06_cancel_with_random_stream_leaves_exactly_one_divergence = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() end }
            M.modes.duel.dice = field.random_stream()
            M.commands.start_duel = function() M.modes.start("duel") end
            M.commands.cancel_duel = function() M.modes.cancel() end
            M.register({})

            local hash_before = state_hasher.hash_state(game.state)
            -- A nil meta.prng and an empty meta.prng serialize differently, so
            -- the neutralization below must restore which of the two this was,
            -- not just the one key inside it.
            local prng_was_absent = (game.state.meta.prng == nil)
            -- Deep-copy, not alias: game.state.meta.prng[id] is itself a live
            -- guarded-proxy view of the same raw table next_u32 advances in
            -- place, so holding the reference would "restore" to whatever it
            -- currently is, not to what it was at this moment.
            local seed_before = nil
            if not prng_was_absent then
                local live = game.state.meta.prng["core:random_stream.mode_seed"]
                if live ~= nil then
                    seed_before = { algorithm = live.algorithm, s0 = live.s0, s1 = live.s1, s2 = live.s2, s3 = live.s3 }
                end
            end

            assert(M.commands.start_duel:run().ok == true)
            assert(M.commands.cancel_duel:run().ok == true)

            local seed_after = game.state.meta.prng["core:random_stream.mode_seed"]
            assert(seed_after ~= nil, "starting a mode with a declared random_stream field must draw core:random_stream.mode_seed")

            local hash_after = state_hasher.hash_state(game.state)
            assert(hash_after ~= hash_before, "drawing the mode_seed stream must change the state hash")

            -- Prove the mode_seed advance is the ONLY divergence: neutralize it
            -- (compared by slice, not by one opaque full-state hash) and re-hash.
            local function neutralize()
                mutation_window.execute_in_window(function()
                    if prng_was_absent then
                        game.state.meta.prng = nil
                    else
                        game.state.meta.prng["core:random_stream.mode_seed"] = seed_before
                    end
                end)
            end
            neutralize()
            local neutralized_hash = state_hasher.hash_state(game.state)
            assert(neutralized_hash == hash_before,
                "neutralizing core:random_stream.mode_seed must exactly restore the pre-start hash")

            -- Cleanup: leave canonical state as this case found it (neutralize()
            -- already did; nothing further to undo since cancel() already
            -- cleared run and the only canonical write was the seed draw).
        end)
    end,

    -- EGC-06: draws are journaled through the run-local stream (ADR-0051 §D5).

    egc06_failed_draw_rolls_back_stream_and_replay_gives_same_sequence = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() end }
            M.modes.duel.dice = field.random_stream()
            M.commands.start_duel = function() M.modes.start("duel") end
            M.register({})

            game.commands.handlers.register("core:command.test.egc06_draw_then_fail", function()
                local decl = mode_store.get_declaration(mode_store.active())
                mode_store.draw_u32("dice", decl.fields.dice)
                error("SyntheticDrawFault: simulated failure after a draw", 0)
            end)
            game.commands.handlers.register("core:command.test.egc06_draw_succeeds", function()
                local decl = mode_store.get_declaration(mode_store.active())
                return { ok = true, rolled = mode_store.draw_u32("dice", decl.fields.dice) }
            end)

            assert(M.commands.start_duel:run().ok == true)

            local function copy_dice()
                local d = M.modes.state().dice
                return { algorithm = d.algorithm, s0 = d.s0, s1 = d.s1, s2 = d.s2, s3 = d.s3 }
            end

            local stream_before = copy_dice()

            local dispatch_ok = pcall(function()
                game.runtime.dispatch_command({ command_id = "core:command.test.egc06_draw_then_fail" })
            end)
            assert(not dispatch_ok, "the draw-then-fail command must propagate its error")

            local stream_after_rollback = copy_dice()
            assert(stream_after_rollback.s0 == stream_before.s0 and stream_after_rollback.s1 == stream_before.s1
                and stream_after_rollback.s2 == stream_before.s2 and stream_after_rollback.s3 == stream_before.s3,
                "a failed draw must roll back the run-local stream state word-for-word")

            game.runtime.dispatch_command({ command_id = "core:command.test.egc06_draw_succeeds" })
            local res1 = game.runtime.last_command_result
            assert(res1 ~= nil and res1.ok == true, "the draw-succeeds command must report ok == true")
            -- Raw handlers registered via game.commands.handlers.register are
            -- not SAS-04-normalized (that wrapping only applies to authoring
            -- commands), so the returned table is last_command_result itself.
            local first_roll = res1.rolled

            -- Rewind to the same starting point and replay: same input, same draw.
            mutation_window.execute_in_window(function()
                local dice = mode_store.run_proxy().dice
                dice.s0, dice.s1, dice.s2, dice.s3 = stream_before.s0, stream_before.s1, stream_before.s2, stream_before.s3
            end)

            game.runtime.dispatch_command({ command_id = "core:command.test.egc06_draw_succeeds" })
            local res2 = game.runtime.last_command_result
            assert(res2 ~= nil and res2.ok == true, "the replayed draw-succeeds command must report ok == true")
            assert(res2.rolled == first_roll, "replaying from the same stream state must give the same draw")
        end)
    end,

    -- EGC-06: post-commit mode facts (ADR-0051 §D8).

    egc06_start_and_finish_publish_mode_events_post_commit = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() end }
            M.commands.start_duel = function() M.modes.start("duel") end
            M.commands.finish_duel = function() M.modes.finish() end
            M.register({})

            local received = {}
            game.events.subscribers.register("core:subscriber.test.egc06_started_probe", "core:event.mode.started", function(env)
                table.insert(received, { event_id = "core:event.mode.started", payload = env.payload })
            end)
            game.events.subscribers.register("core:subscriber.test.egc06_finished_probe", "core:event.mode.finished", function(env)
                table.insert(received, { event_id = "core:event.mode.finished", payload = env.payload })
            end)

            assert(M.commands.start_duel:run().ok == true)
            assert(#received == 1 and received[1].event_id == "core:event.mode.started")
            assert(received[1].payload.mode_id == "rh:mode.duel")

            assert(M.commands.finish_duel:run().ok == true)
            assert(#received == 2 and received[2].event_id == "core:event.mode.finished")
            assert(received[2].payload.mode_id == "rh:mode.duel")
            assert(received[2].payload.outcome == "finished", "finish() must publish outcome 'finished', got: " .. tostring(received[2].payload.outcome))
        end)
    end,

    egc06_cancel_publishes_finished_event_with_outcome_cancelled = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() end }
            M.commands.start_duel = function() M.modes.start("duel") end
            M.commands.cancel_duel = function() M.modes.cancel() end
            M.register({})

            local received = {}
            game.events.subscribers.register("core:subscriber.test.egc06_cancel_probe", "core:event.mode.finished", function(env)
                table.insert(received, env.payload)
            end)

            assert(M.commands.start_duel:run().ok == true)
            assert(M.commands.cancel_duel:run().ok == true)
            assert(#received == 1 and received[1].outcome == "cancelled",
                "cancel() must publish outcome 'cancelled', got: " .. tostring(received[1] and received[1].outcome))
        end)
    end,

    egc06_rolled_back_start_publishes_no_mode_event = function()
        test_isolation.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")
            M.modes.duel = { start = function() end, view = function() end }
            M.commands.start_then_fail = function()
                M.modes.start("duel")
                error("SyntheticRollbackFault: simulated failure after start()", 0)
            end
            M.register({})

            local received = {}
            game.events.subscribers.register("core:subscriber.test.egc06_rollback_probe", "core:event.mode.started", function(env)
                table.insert(received, env)
            end)

            local ok = pcall(function() M.commands.start_then_fail:run() end)
            assert(not ok, "the command must propagate its error")
            assert(#received == 0, "a rolled-back start() must not publish core:event.mode.started")
            assert(mode_store.active() == nil, "the mode must not be active after rollback")
        end)
    end,

    egc07_fixture_lifecycle_through_commands_no_stream = function()
        egc07_run_case(function()
            assert(mode_store.active() == nil)

            game.runtime.dispatch_command({ command_id = "synthetic:command.start_duel" })
            assert(game.runtime.last_command_result.ok == true, "start_duel must succeed")
            assert(mode_store.active() == "synthetic:mode.duel")

            game.runtime.dispatch_command({ command_id = "synthetic:command.bump_rounds" })
            assert(game.runtime.last_command_result.ok == true, "bump_rounds must succeed")

            game.runtime.dispatch_command({ command_id = "synthetic:command.finish_duel" })
            assert(game.runtime.last_command_result.ok == true, "finish_duel must succeed")
            assert(mode_store.active() == nil)
        end)
    end,

    egc07_fixture_bump_then_fail_is_normal_refusal_and_rolls_back = function()
        egc07_run_case(function()
            game.runtime.dispatch_command({ command_id = "synthetic:command.start_duel" })
            assert(game.runtime.last_command_result.ok == true)

            game.runtime.dispatch_command({ command_id = "synthetic:command.bump_then_fail" })
            local res = game.runtime.last_command_result
            assert(res.ok == false, "bump_then_fail must refuse, not raise AuthoringFailAfterMutation")
            assert(res.error.code == "synthetic:error.too_far", "refusal must be the package's own fail() code, got: " .. tostring(res.error.code))

            game.runtime.dispatch_command({ command_id = "synthetic:command.finish_duel" })
            assert(game.runtime.last_command_result.ok == true)
        end)
    end,

    egc07_fixture_command_after_finish_refused_mode_not_active = function()
        egc07_run_case(function()
            game.runtime.dispatch_command({ command_id = "synthetic:command.start_duel" })
            assert(game.runtime.last_command_result.ok == true)
            game.runtime.dispatch_command({ command_id = "synthetic:command.finish_duel" })
            assert(game.runtime.last_command_result.ok == true)

            game.runtime.dispatch_command({ command_id = "synthetic:command.bump_rounds" })
            local res = game.runtime.last_command_result
            assert(res.ok == false, "a mode command after finish() must refuse")
            assert(res.error.code == "core:error.mode.not_active", "Error code must be core:error.mode.not_active, got: " .. tostring(res.error.code))
        end)
    end,

    egc07_fixture_cancel_without_stream_leaves_hash_exactly_unchanged = function()
        egc07_run_case(function()
            local hash_before = state_hasher.hash_state(game.state)
            game.runtime.dispatch_command({ command_id = "synthetic:command.start_duel" })
            assert(game.runtime.last_command_result.ok == true)
            game.runtime.dispatch_command({ command_id = "synthetic:command.cancel_duel" })
            assert(game.runtime.last_command_result.ok == true)
            local hash_after = state_hasher.hash_state(game.state)
            assert(hash_after == hash_before, "cancel() of the no-stream fixture mode must leave the hash exactly unchanged")
        end)
    end,

    egc07_fixture_cancel_with_stream_leaves_exactly_one_divergence = function()
        egc07_run_case(function()
            local hash_before = state_hasher.hash_state(game.state)
            local prng_was_absent = (game.state.meta.prng == nil)
            local seed_before = nil
            if not prng_was_absent then
                local live = game.state.meta.prng["core:random_stream.mode_seed"]
                if live ~= nil then
                    seed_before = { algorithm = live.algorithm, s0 = live.s0, s1 = live.s1, s2 = live.s2, s3 = live.s3 }
                end
            end

            game.runtime.dispatch_command({ command_id = "synthetic:command.start_dice_duel" })
            assert(game.runtime.last_command_result.ok == true)
            game.runtime.dispatch_command({ command_id = "synthetic:command.cancel_duel" })
            assert(game.runtime.last_command_result.ok == true)

            local seed_after = game.state.meta.prng["core:random_stream.mode_seed"]
            assert(seed_after ~= nil, "starting the dice_duel fixture mode must draw core:random_stream.mode_seed")

            local hash_after = state_hasher.hash_state(game.state)
            assert(hash_after ~= hash_before, "drawing the mode_seed stream must change the state hash")

            mutation_window.execute_in_window(function()
                if prng_was_absent then
                    game.state.meta.prng = nil
                else
                    game.state.meta.prng["core:random_stream.mode_seed"] = seed_before
                end
            end)
            local neutralized_hash = state_hasher.hash_state(game.state)
            assert(neutralized_hash == hash_before,
                "neutralizing core:random_stream.mode_seed must exactly restore the pre-start hash")
        end)
    end,

    egc07_fixture_roll_then_fail_rolls_back_and_replay_gives_same_sequence = function()
        egc07_run_case(function()
            game.runtime.dispatch_command({ command_id = "synthetic:command.start_dice_duel" })
            assert(game.runtime.last_command_result.ok == true)

            local function copy_dice()
                local d = mode_store.run_proxy().dice
                return { s0 = d.s0, s1 = d.s1, s2 = d.s2, s3 = d.s3 }
            end

            local stream_before = copy_dice()

            game.runtime.dispatch_command({ command_id = "synthetic:command.roll_dice_then_fail" })
            local fail_res = game.runtime.last_command_result
            assert(fail_res.ok == false, "roll_dice_then_fail must refuse")
            assert(fail_res.error.code == "synthetic:error.bad_luck")

            local stream_after_rollback = copy_dice()
            assert(stream_after_rollback.s0 == stream_before.s0 and stream_after_rollback.s1 == stream_before.s1
                and stream_after_rollback.s2 == stream_before.s2 and stream_after_rollback.s3 == stream_before.s3,
                "a refused roll must roll back the run-local stream word-for-word")

            -- roll_dice is an authoring command: SAS-04 wraps its plain
            -- table return as {ok=true, value={rolled=...}}.
            game.runtime.dispatch_command({ command_id = "synthetic:command.roll_dice" })
            local res1 = game.runtime.last_command_result
            assert(res1.ok == true)
            local first_roll = res1.value.rolled

            mutation_window.execute_in_window(function()
                local dice = mode_store.run_proxy().dice
                dice.s0, dice.s1, dice.s2, dice.s3 = stream_before.s0, stream_before.s1, stream_before.s2, stream_before.s3
            end)

            game.runtime.dispatch_command({ command_id = "synthetic:command.roll_dice" })
            local res2 = game.runtime.last_command_result
            assert(res2.ok == true)
            assert(res2.value.rolled == first_roll, "replaying from the same stream state must give the same draw")
        end)
    end,

    egc07_fixture_save_refused_during_active_mode_and_succeeds_after_finish = function()
        egc07_run_case(function()
            outbound.clear_queue()

            game.runtime.dispatch_command({ command_id = "synthetic:command.start_duel" })
            assert(game.runtime.last_command_result.ok == true)

            game.runtime.dispatch_command({
                command_id = "core:command.session.save",
                args = { slot_id = "egc07_fixture_slot" },
            })
            local refuse_res = game.runtime.last_command_result
            assert(refuse_res ~= nil and refuse_res.ok == false, "save must be refused while the fixture mode is active")
            assert(refuse_res.error.code == "core:error.mode.active", "Error code must be core:error.mode.active, got: " .. tostring(refuse_res.error.code))

            game.runtime.dispatch_command({ command_id = "synthetic:command.finish_duel" })
            assert(game.runtime.last_command_result.ok == true)

            game.runtime.dispatch_command({
                command_id = "core:command.session.save",
                args = { slot_id = "egc07_fixture_slot" },
            })
            local save_res = game.runtime.last_command_result
            assert(save_res ~= nil and save_res.ok == true,
                "save after finish() must succeed, got: " .. tostring(save_res and save_res.error and save_res.error.code))

            local pending = outbound.take_pending_requests()
            assert(pending ~= nil and #pending == 1 and pending[1].slot_id == "egc07_fixture_slot",
                "save after finish() must enqueue exactly one committed request")
        end)
    end,

    egc07_fixture_load_does_not_refuse_during_active_mode = function()
        egc07_run_case(function()
            outbound.clear_queue()

            game.runtime.dispatch_command({ command_id = "synthetic:command.start_duel" })
            assert(game.runtime.last_command_result.ok == true)

            game.runtime.dispatch_command({
                command_id = "core:command.session.load",
                args = { slot_id = "egc07_fixture_slot", revision = "current" },
            })
            local res = game.runtime.last_command_result
            assert(res ~= nil and res.ok == true,
                "core:command.session.load must not refuse while a mode is active, got: " .. tostring(res and res.error and res.error.code))

            -- ADR-0051 §D6: what actually discards the active mode on load is
            -- the host-side VM replacement this request triggers (outside
            -- this Lua VM entirely) -- already covered by
            -- GV2ColdStartLoadConformance's C++ tests, not re-provable from
            -- inside the surviving VM here.
            local pending = outbound.take_pending_requests()
            assert(pending ~= nil and #pending == 1 and pending[1].kind == "load")
        end)
    end,

    egc07_fixture_save_envelope_after_finish_contains_no_run_data = function()
        egc07_run_case(function()
            game.runtime.dispatch_command({ command_id = "synthetic:command.start_dice_duel" })
            assert(game.runtime.last_command_result.ok == true)
            game.runtime.dispatch_command({ command_id = "synthetic:command.bump_rounds" })
            assert(game.runtime.last_command_result.ok == true)
            game.runtime.dispatch_command({ command_id = "synthetic:command.finish_duel" })
            assert(game.runtime.last_command_result.ok == true)
            assert(mode_store.active() == nil)

            local envelope = save_module.build_envelope(game.state, "egc07_save_id", "repo_hash", "script_hash", { "core" })
            local container = save_module.serialize_envelope(envelope)
            assert(not string.find(container, "rounds_won", 1, true), "serialized save must not contain run field names")
            assert(not string.find(container, "dice_duel", 1, true), "serialized save must not contain the mode id")

            local loaded_state, err = load_module.decode_and_prepare(container)
            assert(loaded_state ~= nil, "decode_and_prepare must succeed: " .. tostring(err))
        end)
    end,

    egc07_fixture_events_consumed_by_subscriber_with_outcome = function()
        egc07_run_case(function()
            game.runtime.dispatch_command({ command_id = "synthetic:command.start_duel" })
            assert(game.runtime.last_command_result.ok == true)
            game.runtime.dispatch_command({ command_id = "synthetic:command.cancel_duel" })
            assert(game.runtime.last_command_result.ok == true)

            game.runtime.dispatch_command({ command_id = "synthetic:command.query_last_mode_events" })
            local res = game.runtime.last_command_result
            assert(res.ok == true)
            -- query_last_mode_events is an authoring command: SAS-04 wraps a
            -- plain table return as {ok=true, value=res}, unlike the raw
            -- (unwrapped) synthetic:command.roll_dice above.
            local value = res.value
            assert(value.started_mode_id == "synthetic:mode.duel", "fixture subscriber must have consumed core:event.mode.started")
            assert(value.finished_mode_id == "synthetic:mode.duel", "fixture subscriber must have consumed core:event.mode.finished")
            assert(value.finished_outcome == "cancelled", "finished outcome must be 'cancelled', got: " .. tostring(value.finished_outcome))
        end)
    end,

    egc07_fixture_mode_not_leaked_between_cases_case_a_starts_mode = function()
        egc07_run_case(function()
            game.runtime.dispatch_command({ command_id = "synthetic:command.start_duel" })
            assert(game.runtime.last_command_result.ok == true)
            assert(mode_store.active() == "synthetic:mode.duel")
        end)
    end,

    egc07_fixture_mode_not_leaked_between_cases_case_b_sees_no_active_mode = function()
        assert(mode_store.active() == nil, "the fixture package's mode must not leak across spec cases")
    end,
}
