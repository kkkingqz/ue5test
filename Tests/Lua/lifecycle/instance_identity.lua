-- LSM-04: Instance Identity & Allocator Specification (ADR-0024, CanonicalStateAndSave.md)
-- Verifies monotonic instance_id allocation, <kind>@<number> grammar,
-- counter persistence in state.meta, and prototype isolation.

local instance_allocator = require("core:module.runtime.instance_allocator")
local state_validator = require("core:module.runtime.state_validator")

local function make_clean_state()
    return state_validator.create_empty_canonical_state()
end

return {
    instance_allocator_allocates_sequential_ids = function()
        local state = make_clean_state()

        local id1 = instance_allocator.allocate(state, "actor")
        local id2 = instance_allocator.allocate(state, "actor")
        local id3 = instance_allocator.allocate(state, "item")

        assert(id1 == "actor@1", "first actor must be actor@1, got: " .. tostring(id1))
        assert(id2 == "actor@2", "second actor must be actor@2, got: " .. tostring(id2))
        assert(id3 == "item@1", "first item must be item@1, got: " .. tostring(id3))

        assert(state.meta.instance_counters["actor"] == 3, "actor next counter must be 3")
        assert(state.meta.instance_counters["item"] == 2, "item next counter must be 2")
    end,

    instance_allocator_respects_existing_counter = function()
        local state = make_clean_state()
        state.meta.instance_counters["actor"] = 100

        local next_id = instance_allocator.allocate(state, "actor")
        assert(next_id == "actor@100", "must allocate from existing counter, got: " .. tostring(next_id))
        assert(state.meta.instance_counters["actor"] == 101, "counter must be incremented to 101")
    end,

    instance_allocator_rejects_invalid_kind = function()
        local state = make_clean_state()

        local invalid_kinds = { "", "Actor", "123actor", "actor-name", "actor.sub", "very_long_" .. string.rep("x", 60) }
        for _, kind in ipairs(invalid_kinds) do
            local ok, _ = pcall(function()
                instance_allocator.allocate(state, kind)
            end)
            assert(not ok, "allocating invalid kind '" .. kind .. "' must be rejected")
        end
    end,

    instance_allocator_parses_and_formats = function()
        assert(instance_allocator.is_valid("actor@42"), "actor@42 must be valid")
        assert(instance_allocator.is_valid("item@1"), "item@1 must be valid")
        assert(not instance_allocator.is_valid("actor@0"), "actor@0 must be invalid")
        assert(not instance_allocator.is_valid("actor@-5"), "actor@-5 must be invalid")
        assert(not instance_allocator.is_valid("@42"), "@42 must be invalid")
        assert(not instance_allocator.is_valid("actor@"), "actor@ must be invalid")
        assert(not instance_allocator.is_valid("Actor@1"), "Actor@1 must be invalid")

        local kind, counter = instance_allocator.parse("actor@55")
        assert(kind == "actor", "parsed kind must be actor")
        assert(counter == 55, "parsed counter must be 55")

        local formatted = instance_allocator.format("actor", 55)
        assert(formatted == "actor@55", "formatted string must be actor@55")
    end,

    instance_objects_are_isolated = function()
        local state = make_clean_state()
        local id1 = instance_allocator.allocate(state, "actor")
        local id2 = instance_allocator.allocate(state, "actor")

        state.actors[id1] = { instance_id = id1, definition_id = "core:actor.hero", tags = { "player" } }
        state.actors[id2] = { instance_id = id2, definition_id = "core:actor.npc", tags = { "merchant" } }

        assert(state.actors[id1] ~= state.actors[id2], "instances must be distinct tables")
        assert(state.actors[id1].tags ~= state.actors[id2].tags, "nested tables must not be shared between instances")
    end,
}
