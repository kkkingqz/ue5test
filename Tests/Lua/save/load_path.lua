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

    -- CFC-10: preflight_bytes accepts well-formed container
    preflight_bytes_accepts_valid_container = function()
        local envelope = save.build_envelope({
            meta = { version = 1 },
            prng = { stream_1 = { state = 12345 } },
        }, 1, "repo_hash")
        local container = save.serialize_envelope(envelope)
        local ok, err = load_module.preflight_bytes(container)
        assert(ok == true, "preflight_bytes must accept valid container, got err=" .. tostring(err))
    end,

    -- CFC-10: preflight_bytes rejects missing required package
    preflight_bytes_rejects_missing_package = function()
        local envelope = save.build_envelope({ meta = {} }, 1, "repo_hash")
        envelope.packages = { "pkg:missing_package_that_does_not_exist" }
        local container = save.serialize_envelope(envelope)
        local ok, err = load_module.preflight_bytes(container)
        assert(ok == false, "preflight_bytes must reject missing package")
        assert(tostring(err):find("SaveMissingPackage") ~= nil, "error must indicate SaveMissingPackage, got: " .. tostring(err))
    end,

    -- CFC-10: preflight_bytes rejects corrupt / invalid bytes
    preflight_bytes_rejects_corrupted_container = function()
        local ok1, err1 = load_module.preflight_bytes("not a valid canonical container")
        assert(ok1 == false, "garbage bytes must be rejected")
        assert(err1 == "SaveContainerCorrupt", "got err=" .. tostring(err1))

        local ok2, err2 = load_module.preflight_bytes(nil)
        assert(ok2 == false, "nil must be rejected")
        assert(err2 == "SaveContainerCorrupt", "got err=" .. tostring(err2))
    end,

    -- CFC-10: preflight_bytes rejects dangling reference in payload
    preflight_bytes_rejects_dangling_reference = function()
        local envelope = save.build_envelope({
            meta = {},
            item = { definition_id = "core:item.test.nonexistent_ref" },
        }, 1, "repo_hash")
        local container = save.serialize_envelope(envelope)
        local ok, err = load_module.preflight_bytes(container)
        assert(ok == false, "preflight_bytes must reject payload with dangling definition reference")
        assert(tostring(err):find("SaveReferenceUnknown") ~= nil, "got err=" .. tostring(err))
    end,

    -- CFC-10: preflight_bytes is completely read-only on active session
    preflight_bytes_does_not_mutate_state_or_registries_or_queues = function()
        local outbound = require("core:module.boundary.outbound")
        local initial_state = game and game.state
        local initial_pending = game and game.runtime and game.runtime.pending_section_migrations
        local initial_queue_len = outbound.get_queue_length()

        local envelope = save.build_envelope({
            meta = { player_name = "Original" },
        }, 1, "repo_hash")
        envelope.section_versions = { meta = 1 }
        local container = save.serialize_envelope(envelope)

        local ok, err = load_module.preflight_bytes(container)
        assert(ok == true, "preflight_bytes must succeed: " .. tostring(err))

        assert(game.state == initial_state, "game.state must not be mutated by preflight_bytes")
        if game.runtime then
            assert(game.runtime.pending_section_migrations == initial_pending,
                "game.runtime.pending_section_migrations must not be assigned by preflight_bytes")
        end
        assert(outbound.get_queue_length() == initial_queue_len,
            "outbound queue length must not change during preflight_bytes")
    end,

    -- CFC-10: outbound request_load validates slot_id and revision
    bridge_request_load_validation = function()
        local outbound = require("core:module.boundary.outbound")
        local ok1, err1 = outbound.request_load("", "current")
        assert(ok1 == false and err1 == "InvalidSaveSlotId", "empty slot must be rejected")

        local ok2, err2 = outbound.request_load("bad slot!", "current")
        assert(ok2 == false and err2 == "InvalidSaveSlotId", "slot with space/symbols must be rejected")

        local ok3, err3 = outbound.request_load("valid_slot", "future_rev")
        assert(ok3 == false and err3 == "InvalidSaveSlotRevision", "invalid revision must be rejected")

        outbound.clear_queue()
        local ok4, err4 = outbound.request_load("valid_slot", "previous")
        assert(ok4 == true, "valid_slot and previous revision must succeed: " .. tostring(err4))
        local pending = outbound.take_pending_requests()
        assert(pending ~= nil and #pending == 1, "must enqueue 1 request")
        assert(pending[1].kind == "load", "kind must be load")
        assert(pending[1].slot_id == "valid_slot", "slot_id must match")
        assert(pending[1].revision == "previous", "revision must match")
    end,

    -- CFC-10: core:command.session.load command handler routes to game.bridge.request_load
    session_controls_load_handler = function()
        local outbound = require("core:module.boundary.outbound")
        local dispatcher_mod = require("core:module.runtime.command_dispatcher")
        outbound.clear_queue()

        local dispatcher = dispatcher_mod.new()
        dispatcher.dispatch({
            command_id = "core:command.session.load",
            args = { slot_id = "ui_load_slot", revision = "current" },
        })
        local res = game and game.runtime and game.runtime.last_command_result
        assert(res ~= nil and res.ok == true, "command must succeed: " .. tostring(res and res.error and res.error.code))

        local pending = outbound.take_pending_requests()
        assert(pending ~= nil and #pending == 1, "must enqueue 1 request")
        assert(pending[1].kind == "load", "kind must be load")
        assert(pending[1].slot_id == "ui_load_slot", "slot_id must match")
        assert(pending[1].revision == "current", "revision must match")

        -- Rejection tests
        dispatcher.dispatch({
            command_id = "core:command.session.load",
            args = { slot_id = "", revision = "current" },
        })
        local bad_res = game and game.runtime and game.runtime.last_command_result
        assert(bad_res ~= nil and bad_res.ok == false, "command with empty slot_id must fail")
        assert(bad_res.error.code == "core:error.load.invalid_slot", "expected invalid_slot error")
        assert(outbound.take_pending_requests() == nil, "no requests enqueued on failure")

        dispatcher.dispatch({
            command_id = "core:command.session.load",
            args = { slot_id = "some_slot", revision = "invalid" },
        })
        local bad_rev_res = game and game.runtime and game.runtime.last_command_result
        assert(bad_rev_res ~= nil and bad_rev_res.ok == false, "command with invalid revision must fail")
        assert(bad_rev_res.error.code == "core:error.load.invalid_revision", "expected invalid_revision error")
        assert(outbound.take_pending_requests() == nil, "no requests enqueued on failure")
    end,
}
