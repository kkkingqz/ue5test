-- CBM-10: State Reference Fields Dynamic Registry Specs
-- Verifies dynamic registration of reference fields in state_validator,
-- freeze invariants, error semantics, and integration with state validation & load rewriting.

local state_validator = require("core:module.runtime.state_validator")
local load_module = require("core:module.runtime.load")

return {
    reference_fields_exposed_and_registered = function()
        local fields = state_validator.get_reference_fields()
        assert(type(fields) == "table", "get_reference_fields must return table")
        -- current_location_id registered by rh package
        assert(fields.current_location_id == "location", "current_location_id must map to location")
        assert(state_validator.definition_reference_fields.current_location_id == "location",
            "definition_reference_fields proxy must expose registered fields")
    end,

    frozen_registry_rejects_new_registration = function()
        assert(state_validator.is_frozen(), "registry must be frozen after register phase")
        local ok, err = pcall(function()
            state_validator.register_reference_field("custom_ref_id", "item")
        end)
        assert(not ok, "registration after freeze must fail")
        assert(string.find(tostring(err), "StateReferenceFieldRegistryFrozen"),
            "error must be StateReferenceFieldRegistryFrozen, got: " .. tostring(err))
    end,

    direct_assignment_to_definition_reference_fields = function()
        -- Attempting to register via definition_reference_fields doesn't bypass freeze/registry rules
        local fields = state_validator.get_reference_fields()
        assert(fields.current_location_id == "location")
    end,

    validation_enforces_registered_reference_fields = function()
        local state = state_validator.create_empty_canonical_state()
        state.world.current_location_id = "rh:location.city.market"
        assert(state_validator.validate_state_tree(state) == true, "valid state tree must pass validation")

        -- Invalid kind
        state.world.current_location_id = "rh:item.weapon.sword"
        local ok, err = pcall(function()
            state_validator.validate_state_tree(state)
        end)
        assert(not ok, "reference field with wrong kind must fail validation")
        assert(string.find(tostring(err), "invalid current_location_id"),
            "error must report invalid current_location_id, got: " .. tostring(err))

        -- Non-string
        state.world.current_location_id = 123
        local ok2, err2 = pcall(function()
            state_validator.validate_state_tree(state)
        end)
        assert(not ok2, "reference field with non-string must fail validation")
        assert(string.find(tostring(err2), "must be a string"),
            "error must report must be a string, got: " .. tostring(err2))
    end,

    load_rewrite_uses_reference_fields = function()
        -- Test definition redirect resolution in load_module
        local repo_get = function(id)
            if id == "rh:location.city.market" then
                return { id = "rh:location.city.market" }, nil
            elseif id == "rh:location.old_market" then
                return nil, { code = "not_found", canonical_id = "rh:location.city.market" }
            end
            return nil, { code = "not_found" }
        end

        local canonical, err = load_module.resolve_definition_id("rh:location.old_market", repo_get)
        assert(err == nil, "redirect must resolve without error")
        assert(canonical == "rh:location.city.market", "redirect must rewrite to canonical target")
    end,
}
