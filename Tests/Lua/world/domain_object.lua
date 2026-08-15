-- GEW-04 / TAS-12: game.instances.world as a disposable singleton wrapper
-- over state.world, mirroring the Actor model without instance identity.
-- Migrated from GV2RuntimeCore::Testing::RunWorldDomainObjectConformance
-- (removed) — same cases, same conditions, same coverage.

return {
    world_is_callable = function()
        assert(type(game.instances) == "table", "game.instances must exist")
        assert(type(game.instances.world) == "function", "game.instances.world must be callable")
    end,

    repeated_access_returns_distinct_wrappers = function()
        local w1 = game.instances.world()
        local w2 = game.instances.world()
        assert(type(w1) == "table", "world() must return a table wrapper")
        assert(type(w2) == "table", "world() must return a table wrapper")
        assert(w1 ~= w2, "repeated access must not return the same wrapper table")
    end,

    wrapper_writes_delegate_to_state_world = function()
        local mutation_window = require("core:module.runtime.mutation_window")
        mutation_window.execute_in_window(function()
            local w1 = game.instances.world()
            w1.marker = "spec_gew04"
        end)
        assert(game.state.world.marker == "spec_gew04", "wrapper writes must delegate to state.world")
        local w2 = game.instances.world()
        assert(w2.marker == "spec_gew04", "a freshly obtained wrapper must observe the same underlying state.world")
    end,

    wrapper_rejected_when_stored_in_state = function()
        local state_validator = require("core:module.runtime.state_validator")
        local world = game.instances.world()
        local bad_tree = {
            meta = { schema_version = 1, save_version = 1, save_id = "", instance_counters = {}, prng = {}, time = {} },
            player = {},
            actors = {},
            item_instances = {},
            world = { bad_ref = world },
            quests = {},
            mods = {},
        }
        local ok = pcall(function()
            state_validator.validate_state_tree(bad_tree)
        end)
        assert(not ok, "storing a world wrapper (has a metatable) in canonical state must be rejected by the state validator")
    end,
}
