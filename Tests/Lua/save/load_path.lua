-- SAV-13/15/16: save container preflight, redirect-chain resolution, and
-- referential integrity (core:module.runtime.load). Runs on the real
-- production session (same GameData/core + Scripts/bootstrap as world/) —
-- resolve_definition_id accepts an injectable repository_get precisely so
-- chain/tombstone/cycle behavior can be exercised here without needing a
-- dedicated fixture repository with real redirects.

local canonical_codec = require("core:module.runtime.canonical_codec")
local save = require("core:module.runtime.save")
local load_module = require("core:module.runtime.load")

local function fake_repository(live_ids, tombstoned_ids, redirects)
    return function(id)
        if live_ids[id] then
            return { id = id }, nil
        end
        if tombstoned_ids[id] then
            return nil, { code = "tombstoned" }
        end
        local next_id = redirects[id]
        if next_id then
            return nil, { code = "not_found", canonical_id = next_id }
        end
        return nil, { code = "not_found" }
    end
end

return {
    -- SAV-13
    preflight_accepts_well_formed_envelope = function()
        local envelope = save.build_envelope({ meta = {} }, 1, "repo_hash")
        local container = save.serialize_envelope(envelope)
        local decoded, err = load_module.preflight(container)
        assert(decoded ~= nil, "well-formed container must pass preflight, got err=" .. tostring(err))
        assert(decoded.save_id == 1, "preflight must return the decoded envelope")
    end,

    -- SAV-13
    preflight_rejects_non_string_input = function()
        local decoded, err = load_module.preflight(nil)
        assert(decoded == nil, "nil input must be rejected")
        assert(err == "SaveContainerCorrupt", "got: " .. tostring(err))
    end,

    -- SAV-13
    preflight_rejects_malformed_container_bytes = function()
        local decoded, err = load_module.preflight("not a canonical codec container")
        assert(decoded == nil, "garbage bytes must be rejected")
        assert(err == "SaveContainerCorrupt", "got: " .. tostring(err))
    end,

    -- SAV-13
    preflight_rejects_unknown_format_version = function()
        local envelope = save.build_envelope({ meta = {} }, 1, "")
        envelope.format_version = envelope.format_version + 1
        local container = save.serialize_envelope(envelope)
        local decoded, err = load_module.preflight(container)
        assert(decoded == nil, "future format_version must be rejected")
        assert(err == "SaveFormatVersionUnknown", "got: " .. tostring(err))
    end,

    -- SAV-13
    preflight_rejects_save_version_downgrade = function()
        local envelope = save.build_envelope({ meta = {} }, 1, "")
        envelope.save_version = envelope.save_version + 1
        local container = save.serialize_envelope(envelope)
        local decoded, err = load_module.preflight(container)
        assert(decoded == nil, "a save_version newer than this build must be rejected")
        assert(err == "SaveVersionDowngradeUnsupported", "got: " .. tostring(err))
    end,

    -- SAV-13
    preflight_rejects_integrity_mismatch = function()
        local envelope = save.build_envelope({ meta = {} }, 1, "")
        envelope.integrity = envelope.integrity:gsub("^.", function(c)
            return c == "0" and "1" or "0"
        end)
        local container = save.serialize_envelope(envelope)
        local decoded, err = load_module.preflight(container)
        assert(decoded == nil, "a tampered integrity field must be rejected")
        assert(err == "SaveIntegrityMismatch", "got: " .. tostring(err))
    end,

    -- SAV-15
    resolve_definition_id_direct_hit = function()
        local repo = fake_repository({ ["core:item.test.sword"] = true }, {}, {})
        local id, err = load_module.resolve_definition_id("core:item.test.sword", repo)
        assert(id == "core:item.test.sword", "a live id must resolve to itself")
        assert(err == nil)
    end,

    -- SAV-15
    resolve_definition_id_single_redirect = function()
        local repo = fake_repository(
            { ["core:item.test.new_sword"] = true }, {},
            { ["core:item.test.old_sword"] = "core:item.test.new_sword" })
        local id, err = load_module.resolve_definition_id("core:item.test.old_sword", repo)
        assert(id == "core:item.test.new_sword", "a single redirect must resolve to its target")
        assert(err == nil)
    end,

    -- SAV-15: chain longer than one step
    resolve_definition_id_multi_hop_redirect_chain = function()
        local repo = fake_repository(
            { ["core:item.test.final"] = true }, {},
            {
                ["core:item.test.a"] = "core:item.test.b",
                ["core:item.test.b"] = "core:item.test.c",
                ["core:item.test.c"] = "core:item.test.final",
            })
        local id, err = load_module.resolve_definition_id("core:item.test.a", repo)
        assert(id == "core:item.test.final", "a 3-hop redirect chain must resolve to its final target, got: " .. tostring(id))
        assert(err == nil)
    end,

    -- SAV-16
    resolve_definition_id_tombstoned_is_retired = function()
        local repo = fake_repository({}, { ["core:item.test.gone"] = true }, {})
        local id, err = load_module.resolve_definition_id("core:item.test.gone", repo)
        assert(id == nil, "a tombstoned id must not resolve")
        assert(err == "retired", "got: " .. tostring(err))
    end,

    -- SAV-16
    resolve_definition_id_unknown_is_unknown = function()
        local repo = fake_repository({}, {}, {})
        local id, err = load_module.resolve_definition_id("core:item.test.never_existed", repo)
        assert(id == nil, "an id with no entry anywhere must not resolve")
        assert(err == "unknown", "got: " .. tostring(err))
    end,

    -- SAV-15: defends against a redirect cycle
    resolve_definition_id_cycle_is_unknown = function()
        local repo = fake_repository({}, {}, {
            ["core:item.test.x"] = "core:item.test.y",
            ["core:item.test.y"] = "core:item.test.x",
        })
        local id, err = load_module.resolve_definition_id("core:item.test.x", repo)
        assert(id == nil, "a redirect cycle must never resolve")
        assert(err == "unknown", "got: " .. tostring(err))
    end,

    -- SAV-15/16 against the real pinned repository (no fixture needed)
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
        local state = { meta = {}, world = { current_location_id = "core:location.city.does_not_exist_zzz" } }
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
