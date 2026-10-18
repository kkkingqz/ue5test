-- EGC-07 (plan EngineGapClosure): demonstration mode fixture (ADR-0051) --
-- the first real consumer of the mode mechanism outside Tests/Lua itself
-- (plan rule 2: "every mechanism needs a fixture consumer"). Declares two
-- modes so both halves of EGC-06's cancel() hash-equality criterion have a
-- real fixture: `duel` has no random_stream field (unconditional half --
-- cancel() must leave the hash exactly unchanged), `dice_duel` has exactly
-- one (the one-divergence half, plus draw rollback/replay). Lifecycle goes
-- through these commands only; nothing here calls mode_store directly.

modes.duel = {
    start = function() end,
    view = function(_canonical_state, _run)
        return { screen_id = nil }
    end,
}
modes.duel.rounds_won = field.integer({ default = 0 })

modes.dice_duel = {
    start = function() end,
    view = function(_canonical_state, _run)
        return { screen_id = nil }
    end,
}
modes.dice_duel.rounds_won = field.integer({ default = 0 })
modes.dice_duel.dice = field.random_stream()

commands.start_duel = function()
    modes.start("duel")
end

commands.start_dice_duel = function()
    modes.start("dice_duel")
end

actions["synthetic:action.start_duel"] = "synthetic:command.start_duel"

commands.bump_rounds = function()
    modes.state().rounds_won = modes.state().rounds_won + 1
end

commands.bump_then_fail = function()
    modes.state().rounds_won = modes.state().rounds_won + 1
    fail("too_far")
end

commands.finish_duel = function()
    modes.finish()
end

commands.cancel_duel = function()
    modes.cancel()
end

commands.roll_dice = function()
    return { rolled = modes.draw_u32("dice") }
end

commands.roll_dice_then_fail = function()
    modes.draw_u32("dice")
    fail("bad_luck")
end

-- Query command: read-only observation of what the subscribers below
-- captured. Event scope cannot write canonical state (mutation window is
-- closed there), so the facts are held in these module-level upvalues
-- instead and surfaced back out through an ordinary command read.
local last_started_payload = nil
local last_finished_payload = nil

commands.query_last_mode_events = function()
    return {
        started_mode_id = last_started_payload and last_started_payload.mode_id,
        finished_mode_id = last_finished_payload and last_finished_payload.mode_id,
        finished_outcome = last_finished_payload and last_finished_payload.outcome,
    }
end

-- Without this subscriber, core:event.mode.started/finished (published by
-- EGC-06) would be a mechanism with no consumer at all -- rule 2 of the
-- plan treats that as not closed, not merely untested.
on("core:event.mode.started", function(payload, _env)
    last_started_payload = payload
end)

on("core:event.mode.finished", function(payload, _env)
    last_finished_payload = payload
end)
