-- PEP-03 (ADR-0047): Scripts/boundary/outbound.lua's own light validation and
-- staged/committed/rollback lifecycle for the one-shot presentation effect queue.
-- The host-side DTO parsing, sequence stamping, and typed rejection reasons
-- (ReadPresentationEffect, TakePendingEffects, ResolveEffectTarget) are a C++
-- mechanism instead, covered by GV2RuntimeCore::Testing::RunPresentationEffectConformance
-- against a synthetic stub (ADR-0024: a rule expressed in Lua belongs to a Lua spec,
-- not a C++ conformance fixture, and vice versa). Mirrors save_path.lua's own
-- staged-during-command/committed-on-success/discarded-on-failure test shape for
-- game.bridge.request_save, applied to game.ui.publish_effect instead.

return {
    -- ADR-0047 §1: publish_effect requires a table with a non-empty effect_id string;
    -- deep validation of target/args shape happens host-side when the effect is
    -- actually pulled, not here.
    publish_effect_rejects_missing_effect_id = function()
        local outbound = require("core:module.boundary.outbound")
        local ok, err = outbound.publish_effect({})
        assert(ok == false, "publish_effect must reject a table with no effect_id")
        assert(err == "InvalidPresentationEffect", "rejection must be InvalidPresentationEffect, got: " .. tostring(err))

        local ok2, err2 = outbound.publish_effect("core:effect.test.not_a_table")
        assert(ok2 == false, "publish_effect must reject a non-table argument")
        assert(err2 == "InvalidPresentationEffect", "rejection must be InvalidPresentationEffect, got: " .. tostring(err2))
    end,

    -- ADR-0047 §3: outside any command context, publish_effect enqueues directly --
    -- mirrors outbound.request_save's own idle-direct-enqueue behavior.
    publish_effect_idle_direct_enqueue = function()
        local outbound = require("core:module.boundary.outbound")
        outbound.clear_queue()
        local ok = outbound.publish_effect({ effect_id = "core:effect.test.idle" })
        assert(ok == true, "publish_effect in idle must succeed")
        local pending = outbound.take_pending_effects()
        assert(pending ~= nil and #pending == 1, "must return one pending effect")
        assert(pending[1].effect_id == "core:effect.test.idle", "effect_id must match")
        assert(outbound.take_pending_effects() == nil, "subsequent take must return nil")
    end,

    -- ADR-0041/ADR-0047: an effect published mid-command must not survive that
    -- command's own rollback, same as a save/load request must not -- staged during
    -- dispatch, committed only once the command itself succeeds.
    publish_effect_staged_and_committed_on_command_success = function()
        local outbound = require("core:module.boundary.outbound")
        local dispatcher_mod = require("core:module.runtime.command_dispatcher")
        local handler_reg_mod = require("core:module.runtime.handler_registry")
        outbound.clear_queue()

        local reg = handler_reg_mod.create_registry()
        reg.register("core:command.test.effect_publish_ok", function(_req)
            local ok, err = game.ui.publish_effect({ effect_id = "core:effect.test.command_staged" })
            assert(ok == true, "publish_effect inside command must succeed: " .. tostring(err))
            assert(outbound.get_staged_effect_length() == 1, "must be staged during command dispatch")
            assert(outbound.get_effect_queue_length() == 0, "must not be in committed queue yet")
            return { ok = true }
        end)

        local dispatcher = dispatcher_mod.new(reg)
        dispatcher.dispatch({
            command_id = "core:command.test.effect_publish_ok",
            args = {},
        })

        assert(outbound.get_staged_effect_length() == 0, "staged must be cleared after commit")
        local pending = outbound.take_pending_effects()
        assert(pending ~= nil and #pending == 1, "pending must contain committed effect")
        assert(pending[1].effect_id == "core:effect.test.command_staged", "effect_id must match")
    end,

    -- ADR-0041/ADR-0047: a command that fails must discard whatever it staged,
    -- including any effect -- same guarantee save/load requests already have.
    publish_effect_discarded_on_command_failure = function()
        local outbound = require("core:module.boundary.outbound")
        local dispatcher_mod = require("core:module.runtime.command_dispatcher")
        local handler_reg_mod = require("core:module.runtime.handler_registry")
        outbound.clear_queue()

        local reg = handler_reg_mod.create_registry()
        reg.register("core:command.test.effect_publish_fail", function(_req)
            game.ui.publish_effect({ effect_id = "core:effect.test.discarded" })
            assert(outbound.get_staged_effect_length() == 1, "must be staged")
            return {
                ok = false,
                error = { code = "core:error.command.validation_refused", params = {} },
            }
        end)

        local dispatcher = dispatcher_mod.new(reg)
        dispatcher.dispatch({
            command_id = "core:command.test.effect_publish_fail",
            args = {},
        })

        assert(outbound.get_staged_effect_length() == 0, "staged effect queue must be cleared on failure")
        assert(outbound.take_pending_effects() == nil, "committed effect queue must be empty on command refusal")
    end,

    -- ADR-0047 §3: a save/load control request and a presentation effect staged in
    -- the SAME command must be independent queues -- one committing must not leak
    -- into or clear the other.
    publish_effect_and_request_save_are_independent_queues = function()
        local outbound = require("core:module.boundary.outbound")
        local dispatcher_mod = require("core:module.runtime.command_dispatcher")
        local handler_reg_mod = require("core:module.runtime.handler_registry")
        outbound.clear_queue()

        local reg = handler_reg_mod.create_registry()
        reg.register("core:command.test.effect_and_save", function(_req)
            game.ui.publish_effect({ effect_id = "core:effect.test.alongside_save" })
            game.bridge.request_save("alongside_effect_slot")
            return { ok = true }
        end)

        local dispatcher = dispatcher_mod.new(reg)
        dispatcher.dispatch({
            command_id = "core:command.test.effect_and_save",
            args = {},
        })

        local pending_effects = outbound.take_pending_effects()
        assert(pending_effects ~= nil and #pending_effects == 1, "effect queue must have exactly the published effect")
        local pending_requests = outbound.take_pending_requests()
        assert(pending_requests ~= nil and #pending_requests == 1, "request queue must have exactly the save request")
        assert(pending_requests[1].slot_id == "alongside_effect_slot", "slot_id must match")
    end,
}
