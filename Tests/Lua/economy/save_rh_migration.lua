-- RH-12 / TSL-17: RH Save Migration Specification (FullGame tier)
-- Verifies save decode_and_prepare with live rh definitions and rejection of unmigrated core location.

local save = require("core:module.runtime.save")
local load_module = require("core:module.runtime.load")

return {
    -- SAV-15/16 against the real pinned repository
    resolve_definition_id_against_real_repository_hits_directly = function()
        local id, err = load_module.resolve_definition_id("rh:location.city.tavern")
        assert(id == "rh:location.city.tavern", "a real, live definition must resolve to itself, got: " .. tostring(id))
        assert(err == nil)
    end,

    resolve_definition_id_against_real_repository_reports_unknown = function()
        local id, err = load_module.resolve_definition_id("rh:location.city.does_not_exist_zzz")
        assert(id == nil, "a definition absent from the real repository must not resolve")
        assert(err == "unknown", "got: " .. tostring(err))
    end,

    -- SAV-13/15/16 combined: full decode_and_prepare pipeline against a
    -- real container referencing a real, live definition — proves the
    -- rewrite pass is a correct no-op for an already-canonical reference.
    decode_and_prepare_roundtrips_state_with_live_reference = function()
        local state = { meta = {}, world = { current_location_id = "rh:location.city.tavern" } }
        local envelope = save.build_envelope(state, 1, "")
        local container = save.serialize_envelope(envelope)

        local decoded, err = load_module.decode_and_prepare(container)
        assert(decoded ~= nil, "decode_and_prepare must succeed for a valid container, got err=" .. tostring(err))
        assert(decoded.world.current_location_id == "rh:location.city.tavern",
            "a live reference must roundtrip unchanged")
    end,

    -- SAV-16: decode_and_prepare must fail, not silently drop, a dangling
    -- reference — proven against the real repository, no fixture needed.
    decode_and_prepare_rejects_dangling_reference = function()
        local state = { meta = {}, world = { current_location_id = "rh:location.city.does_not_exist_zzz" } }
        local envelope = save.build_envelope(state, 1, "")
        local container = save.serialize_envelope(envelope)

        local decoded, err = load_module.decode_and_prepare(container)
        assert(decoded == nil, "a dangling reference must reject the whole container, not just that field")
        assert(tostring(err):find("^SaveReferenceUnknown:") ~= nil, "got: " .. tostring(err))
    end,

    -- RH-12: Old save created before entity migration contains core:location.city.tavern.
    -- Because no redirect exists from engine to game, it must fail typed as SaveReferenceUnknown (not retired).
    old_save_with_unmigrated_core_location_fails_as_unknown = function()
        local state = { meta = {}, world = { current_location_id = "core:location.city.tavern" } }
        local envelope = save.build_envelope(state, 1, "")
        local container = save.serialize_envelope(envelope)

        local decoded, err = load_module.decode_and_prepare(container)
        assert(decoded == nil, "old save referencing core:location.city.tavern must fail to load")
        assert(tostring(err):find("^SaveReferenceUnknown:") ~= nil and tostring(err):find("core:location.city.tavern") ~= nil,
            "must fail with SaveReferenceUnknown, got: " .. tostring(err))
    end,
}
