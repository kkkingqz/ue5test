-- EGC-05: Mode Store and `modes` Authoring Surface Specification (ADR-0051)
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

local authoring_context = require("core:module.authoring.context")
local test_isolation = require("core:module.runtime.test_isolation")
local mode_store = require("core:module.runtime.mode_store")
local field = require("core:module.authoring.field")

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
}
