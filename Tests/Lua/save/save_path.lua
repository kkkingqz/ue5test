-- SAV-08/09/10/11: save container envelope, safe-point policy, write
-- orchestration, and write failure semantics
-- (core:module.runtime.save). Runs against the real production session —
-- Headless/Source/main.cpp and GV2LuaSpecRunnerHostTests.cpp both wire a
-- real (throwaway, temp-dir-rooted) FFilesystemSaveSlotStorage into this
-- subtree's session, so game.save_slots.write exercises the actual host
-- primitive, not a mock.

local canonical_codec = require("core:module.runtime.canonical_codec")
local state_hasher = require("core:module.runtime.state_hasher")
local save = require("core:module.runtime.save")

return {
    -- SAV-08
    envelope_has_expected_fields_and_roundtrips_through_codec = function()
        local state = { a = 1, b = "hi" }
        local envelope = save.build_envelope(state, 7, "repo_hash_example")
        assert(envelope.format_version == save.SAVE_VERSION, "format_version must equal SAVE_VERSION")
        assert(envelope.codec_version == canonical_codec.VERSION, "codec_version must equal canonical_codec.VERSION")
        assert(envelope.save_version == save.SAVE_VERSION, "save_version must equal SAVE_VERSION")
        assert(envelope.save_id == 7, "save_id must be passed through unchanged")
        assert(envelope.repository_content_hash == "repo_hash_example",
            "repository_content_hash must be passed through unchanged")
        assert(type(envelope.payload) == "string", "payload must be a string")

        local container = save.serialize_envelope(envelope)
        local decoded = canonical_codec.deserialize(container)
        assert(decoded.format_version == envelope.format_version, "container must roundtrip format_version")
        assert(decoded.save_id == envelope.save_id, "container must roundtrip save_id")
        assert(decoded.payload == envelope.payload, "container must roundtrip payload bytes exactly")
    end,

    -- SAV-08
    envelope_integrity_matches_canonical_state_hash = function()
        local state = { x = 1, y = { 1, 2, 3 }, z = "value" }
        local envelope = save.build_envelope(state, 1, "")
        assert(envelope.integrity == state_hasher.hash_state(state),
            "integrity must equal the canonical hash of state")
    end,

    -- SAV-08
    envelope_payload_deserializes_back_to_original_state = function()
        local state = { list = { 1, 2, 3 }, nested = { flag = true, name = "n" } }
        local envelope = save.build_envelope(state, 1, "")
        local decoded_state = canonical_codec.deserialize(envelope.payload)
        assert(canonical_codec.serialize(decoded_state) == canonical_codec.serialize(state),
            "payload must decode back to a canonically-identical state tree")
    end,

    -- SAV-10
    repeated_build_of_same_state_gives_byte_identical_container = function()
        local state = { a = 1, b = { 2, 3 }, c = "same" }
        local first = save.serialize_envelope(save.build_envelope(state, 42, "hash_x"))
        local second = save.serialize_envelope(save.build_envelope(state, 42, "hash_x"))
        assert(first == second, "identical inputs must produce a byte-identical container")
    end,

    -- SAV-09
    is_safe_point_true_at_baseline = function()
        assert(save.is_safe_point(), "spec execution itself must be a safe point (idle, empty queues)")
    end,

    -- SAV-09
    save_rejected_during_executing_command_phase = function()
        local previous_phase = game.runtime.phase
        game.runtime.phase = "executing_command"
        local ok, err = save.save("probe_slot_a", 1, "")
        game.runtime.phase = previous_phase
        assert(ok == false, "save must be rejected while phase is executing_command")
        assert(err == "SaveNotAtSafePoint", "rejection must be the typed SaveNotAtSafePoint error, got: " .. tostring(err))
    end,

    -- SAV-09
    save_rejected_during_pumping_events_phase = function()
        local previous_phase = game.runtime.phase
        game.runtime.phase = "pumping_events"
        local ok, err = save.save("probe_slot_b", 1, "")
        game.runtime.phase = previous_phase
        assert(ok == false, "save must be rejected while phase is pumping_events")
        assert(err == "SaveNotAtSafePoint", "rejection must be the typed SaveNotAtSafePoint error, got: " .. tostring(err))
    end,

    -- SAV-09
    save_rejected_when_command_queue_not_empty = function()
        game.commands.enqueue({ command_id = "core:command.test.save_safe_point_probe" })
        local ok, err = save.save("probe_slot_c", 1, "")
        game.commands.clear_queue()
        assert(ok == false, "save must be rejected while the command queue is not empty")
        assert(err == "SaveNotAtSafePoint", "rejection must be the typed SaveNotAtSafePoint error, got: " .. tostring(err))
    end,

    -- SAV-10
    save_writes_container_through_slot_storage = function()
        local ok, err = save.save("save_path_roundtrip_slot", 1, "repo_hash")
        assert(ok == true, "save must succeed at a safe point with storage available, got err=" .. tostring(err))
    end,

    -- SAV-10
    save_does_not_mutate_canonical_state = function()
        local before = canonical_codec.serialize(game.state)
        local ok = save.save("save_path_no_mutation_slot", 1, "repo_hash")
        assert(ok == true, "save must succeed for this case to be meaningful")
        local after = canonical_codec.serialize(game.state)
        assert(before == after, "save must never mutate game.state")
    end,

    -- SAV-11
    save_rejects_invalid_slot_id_with_typed_write_failure = function()
        local ok, err = save.save("Invalid Slot Id", 1, "repo_hash")
        assert(ok == false, "save must fail for a slot id outside the allowed grammar")
        assert(err == "SaveWriteFailed:failure",
            "failure must be the typed SaveWriteFailed error, got: " .. tostring(err))
    end,

    -- SAV-11
    failed_save_does_not_consume_save_id = function()
        local previous_phase = game.runtime.phase
        game.runtime.phase = "executing_command"
        local failed_ok = save.save("save_id_not_consumed_slot", 99, "repo_hash")
        game.runtime.phase = previous_phase
        assert(failed_ok == false, "first attempt must fail (forced unsafe phase)")

        local retry_ok = save.save("save_id_not_consumed_slot", 99, "repo_hash")
        assert(retry_ok == true,
            "retrying the exact same save_id after a failed attempt must succeed — save_id is never consumed by a failure")
    end,
}
