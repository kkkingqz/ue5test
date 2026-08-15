-- LSM-03: State Sections & Validation Specification (ADR-0024, CanonicalStateAndSave.md)
-- Verifies the canonical structure of game.state, mandatory top-level sections,
-- prototype isolation, and strict validation of allowed scalar/table types.

local state_validator = require("core:module.runtime.state_validator")

local function make_clean_state()
    return state_validator.create_empty_canonical_state()
end

return {
    canonical_state_structure_has_required_sections = function()
        local state = make_clean_state()
        local sections = state_validator.get_canonical_sections()

        assert(type(sections) == "table", "get_canonical_sections must return a table")
        assert(#sections >= 6, "must have at least 6 canonical sections")

        for _, sec in ipairs(sections) do
            assert(state_validator.is_canonical_section(sec), "section '" .. sec .. "' must be recognized as canonical")
            assert(type(state[sec]) == "table", "state[" .. sec .. "] must be a table")
        end

        assert(type(state.meta.schema_version) == "number", "state.meta.schema_version must be a number")
        assert(type(state.meta.instance_counters) == "table", "state.meta.instance_counters must be a table")
    end,

    state_validator_allows_valid_state = function()
        local state = make_clean_state()
        local ok, err = pcall(function()
            state_validator.validate_state_tree(state)
        end)
        assert(ok, "clean canonical state must pass validation, got: " .. tostring(err))
    end,

    state_validator_rejects_unknown_root_section = function()
        local state = make_clean_state()
        state.illegal_custom_section = { data = 123 }
        local ok, err = pcall(function()
            state_validator.validate_state_tree(state)
        end)
        assert(not ok, "unknown top-level section must be rejected")
        assert(string.find(tostring(err), "invalid top%-level section") or string.find(tostring(err), "illegal_custom_section"),
            "Error must mention invalid top-level section, got: " .. tostring(err))
    end,

    state_validator_rejects_function_value = function()
        local state = make_clean_state()
        state.world.callback = function() end
        local ok, _ = pcall(function()
            state_validator.validate_state_tree(state)
        end)
        assert(not ok, "function value inside state tree must be rejected")
    end,

    state_validator_rejects_table_with_metatable = function()
        local state = make_clean_state()
        local obj = setmetatable({}, { __index = {} })
        state.world.custom_obj = obj
        local ok, _ = pcall(function()
            state_validator.validate_state_tree(state)
        end)
        assert(not ok, "table with metatable inside state tree must be rejected")
    end,

    state_validator_rejects_nan_and_infinity = function()
        local state_nan = make_clean_state()
        state_nan.world.val = 0/0
        local ok_nan, _ = pcall(function()
            state_validator.validate_state_tree(state_nan)
        end)
        assert(not ok_nan, "NaN inside state tree must be rejected")

        local state_inf = make_clean_state()
        state_inf.world.val = math.huge
        local ok_inf, _ = pcall(function()
            state_validator.validate_state_tree(state_inf)
        end)
        assert(not ok_inf, "Infinity inside state tree must be rejected")
    end,

    state_validator_rejects_cyclic_reference = function()
        local state = make_clean_state()
        local t1 = {}
        local t2 = { parent = t1 }
        t1.child = t2
        state.world.cycle = t1
        local ok, _ = pcall(function()
            state_validator.validate_state_tree(state)
        end)
        assert(not ok, "cyclic table reference inside state tree must be rejected")
    end,

    state_validator_rejects_definition_table_embedding = function()
        local state = make_clean_state()
        -- Attempting to store raw definition table instead of definition_id
        state.world.embedded_item = {
            id = "core:item.sword",
            schema_version = 1,
            data = {},
        }
        local ok, _ = pcall(function()
            state_validator.validate_state_tree(state)
        end)
        assert(not ok, "embedding raw definition tables in state tree must be rejected")
    end,
}
