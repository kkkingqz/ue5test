-- GEW-05 / TAS-12: state.world.current_location_id as a Stable ID of kind
-- `location`, resolved against the pinned repository. Migrated from
-- GV2RuntimeCore::Testing::RunWorldCurrentLocationConformance (removed) —
-- same cases, same conditions, same coverage. Runs against the real
-- production repository, so it references real GameData/core definitions
-- (core:location.city.market, core:screen.main) instead of an in-memory
-- fixture repository the way the removed C++ conformance test did.

local function make_tree(current_location_id)
    return {
        meta = { schema_version = 1, save_version = 1, save_id = "", instance_counters = {}, prng = {}, time = {} },
        player = {},
        actors = {},
        item_instances = {},
        world = { current_location_id = current_location_id },
        quests = {},
        mods = {},
    }
end

return {
    valid_location_reference_accepted = function()
        local state_validator = require("core:module.runtime.state_validator")
        local ok = pcall(function()
            state_validator.validate_state_tree(make_tree("core:location.city.market"))
        end)
        assert(ok, "a Stable ID of kind 'location' resolving in the pinned repository must be accepted as world.current_location_id")
    end,

    wrong_kind_reference_rejected = function()
        local state_validator = require("core:module.runtime.state_validator")
        local ok = pcall(function()
            state_validator.validate_state_tree(make_tree("core:screen.main"))
        end)
        assert(not ok, "a Stable ID of kind 'screen' must be rejected as world.current_location_id")
    end,

    dangling_reference_rejected = function()
        local state_validator = require("core:module.runtime.state_validator")
        local ok = pcall(function()
            state_validator.validate_state_tree(make_tree("core:location.city.nonexistent"))
        end)
        assert(not ok, "a well-formed but non-existent location reference must be rejected as dangling")
    end,

    current_location_readable_without_mutation = function()
        local mutation_window = require("core:module.runtime.mutation_window")
        mutation_window.execute_in_window(function()
            game.state.world.current_location_id = "core:location.city.market"
        end)
        local w = game.instances.world()
        assert(w.current_location_id == "core:location.city.market", "current_location_id must be readable through the World wrapper")
    end,
}
