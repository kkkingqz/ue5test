-- TWH-08: Verifies that sample package state is stored in canonical game.state (game.state.sample_debug)
-- and survives save serialization/deserialization as well as contributing to canonical state hashing.

local canonical_codec = require("core:module.runtime.canonical_codec")
local state_hasher = require("core:module.runtime.state_hasher")
local save = require("core:module.runtime.save")

return {
    sample_debug_state_roundtrips_through_save_and_load = function()
        local state = {
            sample_debug = {
                checkbox_checked = true,
                selected_class = "mage",
                player_name = "Hero",
            },
        }

        local envelope = save.build_envelope(state, 1, "repo_hash_sample")
        local container = save.serialize_envelope(envelope)
        local decoded_envelope = canonical_codec.deserialize(container)
        local decoded_state = canonical_codec.deserialize(decoded_envelope.payload)

        assert(decoded_state.sample_debug ~= nil, "decoded state must contain sample_debug section")
        assert(decoded_state.sample_debug.checkbox_checked == true, "checkbox_checked must be preserved")
        assert(decoded_state.sample_debug.selected_class == "mage", "selected_class must be preserved")
        assert(decoded_state.sample_debug.player_name == "Hero", "player_name must be preserved")
    end,

    sample_debug_state_affects_canonical_state_hash = function()
        local state_a = {
            sample_debug = {
                checkbox_checked = false,
                selected_class = "warrior",
                player_name = "Player",
            },
        }

        local state_b = {
            sample_debug = {
                checkbox_checked = true,
                selected_class = "warrior",
                player_name = "Player",
            },
        }

        local hash_a = state_hasher.hash_state(state_a)
        local hash_b = state_hasher.hash_state(state_b)

        assert(hash_a ~= hash_b, "mutating sample_debug state must produce a different state_hash")
    end,
}
