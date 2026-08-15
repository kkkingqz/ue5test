-- LSM-06: State Hasher Specification (ADR-0024, CanonicalStateAndSave.md)
-- Verifies pure Lua SHA-256 calculation, deterministic state serialization,
-- key order independence, and change detection sensitivity.

local state_hasher = require("core:module.runtime.state_hasher")

return {
    sha256_known_rfc_vectors = function()
        local empty_hash = state_hasher.sha256("")
        assert(empty_hash == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
            "SHA-256 of empty string mismatch: " .. tostring(empty_hash))

        local abc_hash = state_hasher.sha256("abc")
        assert(abc_hash == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
            "SHA-256 of 'abc' mismatch: " .. tostring(abc_hash))
    end,

    state_hasher_deterministic_independent_of_key_order = function()
        local state1 = {
            alpha = 1,
            beta = "two",
            gamma = { x = 10, y = 20 },
            delta = { 1, 2, 3 },
        }

        local state2 = {
            delta = { 1, 2, 3 },
            gamma = { y = 20, x = 10 },
            beta = "two",
            alpha = 1,
        }

        local hash1 = state_hasher.hash_state(state1)
        local hash2 = state_hasher.hash_state(state2)

        assert(hash1 == hash2, "Hashes must be identical regardless of insertion order: " .. tostring(hash1) .. " vs " .. tostring(hash2))
    end,

    state_hasher_changes_when_any_value_changes = function()
        local base = {
            meta = { schema_version = 1, count = 5 },
            world = { location = "core:location.market" },
            items = { "item@1", "item@2" },
        }
        local base_hash = state_hasher.hash_state(base)

        -- Scalar modification
        local modified_num = {
            meta = { schema_version = 1, count = 6 },
            world = { location = "core:location.market" },
            items = { "item@1", "item@2" },
        }
        assert(state_hasher.hash_state(modified_num) ~= base_hash, "Hash must change when number changes")

        -- String modification
        local modified_str = {
            meta = { schema_version = 1, count = 5 },
            world = { location = "core:location.tavern" },
            items = { "item@1", "item@2" },
        }
        assert(state_hasher.hash_state(modified_str) ~= base_hash, "Hash must change when string changes")

        -- Array item modification
        local modified_arr = {
            meta = { schema_version = 1, count = 5 },
            world = { location = "core:location.market" },
            items = { "item@1", "item@3" },
        }
        assert(state_hasher.hash_state(modified_arr) ~= base_hash, "Hash must change when array item changes")
    end,

    state_hasher_distinguishes_null_from_absent = function()
        local with_null = { key = nil }
        local with_val = { key = "val" }
        assert(state_hasher.hash_state(with_null) ~= state_hasher.hash_state(with_val),
            "State with value must differ from state with nil")
    end,
}
