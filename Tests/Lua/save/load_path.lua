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

    -- SAV-15/16: rewrite_references updates definition_id through redirects
    rewrite_references_updates_definition_id = function()
        local repo = fake_repository(
            { ["core:item.test.new_sword"] = true }, {},
            { ["core:item.test.old_sword"] = "core:item.test.new_sword" })
        local tree = { meta = {}, item = { definition_id = "core:item.test.old_sword" } }
        local ok, err = load_module.rewrite_references(tree, "state", repo)
        assert(ok, "rewrite must succeed")
        assert(tree.item.definition_id == "core:item.test.new_sword", "definition_id must be rewritten")
    end,

    -- SAV-16: rewrite_references rejects dangling reference
    rewrite_references_rejects_dangling_reference = function()
        local repo = fake_repository({}, {}, {})
        local tree = { meta = {}, item = { definition_id = "core:item.test.does_not_exist" } }
        local ok, err = load_module.rewrite_references(tree, "state", repo)
        assert(not ok, "dangling reference must fail")
        assert(tostring(err):find("SaveReferenceUnknown") ~= nil, "got: " .. tostring(err))
    end,
}
