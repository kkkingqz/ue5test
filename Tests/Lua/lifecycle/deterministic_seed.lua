-- CFC-07A: Deterministic Seed & PRNG Specification (ADR-0024, CanonicalStateAndSave.md)
-- Verifies:
-- 1. Seed transport from host to Lua before bootstrap hooks, isolated from session generation.
-- 2. math.random and math.randomseed remain deleted.
-- 3. game.random facade is installed and protected.
-- 4. Golden test vectors for xoshiro128** PRNG and SHA-256 stream derivation.
-- 5. Stream isolation between distinct stream IDs.
-- 6. next_unit generates values in [0, 1).
-- 7. next_int generates values within [min, max] using unbiased rejection sampling.
-- 8. State persistence in meta.prng and state tree validation.

local random = require("core:module.runtime.random")
local state_validator = require("core:module.runtime.state_validator")

local function with_mock_state(seed_hex, fn)
    local old_state = _G.game.state
    local new_state = state_validator.create_empty_canonical_state()
    new_state.meta.seed_hex = seed_hex
    _G.game.state = new_state
    local ok, err = pcall(fn, new_state)
    _G.game.state = old_state
    if not ok then
        error(err, 0)
    end
end

return {
    seed_is_required_and_never_defaulted_to_zero = function()
        -- CFC-07A (ревью M1): раньше отсутствие seed заменялось нулями в трёх местах, поэтому
        -- прохождение молча шло по одному PRNG-потоку. Обе двери закрыты: пустое canonical
        -- state без известного seed построить нельзя, а state без meta.seed_hex не проходит
        -- validate_state -- то есть сейв без seed отвергает владелец кодирования, а не
        -- случайный более поздний вызов game.random.
        local ok, err = pcall(function()
            state_validator.create_empty_canonical_state("NOTHEX0123456789")
        end)
        assert(not ok and string.find(tostring(err), "InvalidSeedHex"),
            "create_empty_canonical_state must refuse a non-hex seed, got: " .. tostring(err))

        local tree = state_validator.create_empty_canonical_state("0123456789abcdef")
        assert(tree.meta.seed_hex == "0123456789abcdef", "explicit seed must reach meta.seed_hex")
        assert(tree.meta.seed_hex ~= "0000000000000000", "seed must never silently become zero")

        tree.meta.seed_hex = nil
        ok, err = pcall(function()
            state_validator.validate_state_tree(tree)
        end)
        assert(not ok and string.find(tostring(err), "meta.seed_hex"),
            "validate_state_tree must reject a canonical state without meta.seed_hex, got: " .. tostring(err))

        -- Neither an explicit seed nor a session seed: the empty state has no deterministic
        -- source at all, and that must be a refusal rather than a substituted constant.
        local previous_session_seed = _G.game.runtime.seed_hex
        _G.game.runtime.seed_hex = nil
        ok, err = pcall(function()
            state_validator.create_empty_canonical_state()
        end)
        _G.game.runtime.seed_hex = previous_session_seed
        assert(not ok and string.find(tostring(err), "InvalidSeedHex"),
            "create_empty_canonical_state must refuse when no seed is available anywhere, got: " .. tostring(err))
    end,

    seed_transport_and_generation_isolation = function()
        assert(_G.game ~= nil and _G.game.runtime ~= nil, "game.runtime must exist")
        local seed_hex = _G.game.runtime.seed_hex
        assert(type(seed_hex) == "string", "game.runtime.seed_hex must be a string")
        assert(#seed_hex == 16, "game.runtime.seed_hex must be 16 chars: " .. tostring(seed_hex))
        assert(seed_hex:match("^[0-9a-f]+$"), "game.runtime.seed_hex must be lowercase hex: " .. tostring(seed_hex))

        local gen = _G.game.runtime.session_generation
        assert(type(gen) == "number" and math.type(gen) == "integer", "game.runtime.session_generation must be an integer")
        assert(gen >= 1, "game.runtime.session_generation must be >= 1")

        -- math.random must remain deleted per LuaRuntimeContract
        assert(math.random == nil, "math.random must be nil")
        assert(math.randomseed == nil, "math.randomseed must be nil")
    end,

    random_facade_installed_and_protected = function()
        assert(_G.game.random ~= nil, "game.random facade must exist")
        assert(type(_G.game.random.next_u32) == "function", "game.random.next_u32 must be a function")
        assert(type(_G.game.random.next_unit) == "function", "game.random.next_unit must be a function")
        assert(type(_G.game.random.next_int) == "function", "game.random.next_int must be a function")

        -- Modifying game.random must be rejected by facade protection
        local ok_mod, _ = pcall(function()
            _G.game.random.custom = 123
        end)
        assert(not ok_mod, "assigning to game.random must be disallowed")

        local ok_overwrite, _ = pcall(function()
            _G.game.random = {}
        end)
        assert(not ok_overwrite, "overwriting game.random must be disallowed")
    end,

    golden_vector_seed_zero = function()
        -- Canonical vector from CanonicalStateAndSave.md:
        -- seed_hex: 0000000000000000, stream_id: core:random_stream.gameplay
        -- Initial words: b11c2782 47dc733c af0684fb b5b1f64c
        -- Outputs: e020c7cb f94e13e0 a66c9e06 086a074f 81883abc
        local derived = random.derive_stream("0000000000000000", "core:random_stream.gameplay")
        assert(derived.algorithm == "xoshiro128ss-v1", "algorithm must be xoshiro128ss-v1")
        assert(derived.s0 == "b11c2782", "s0 mismatch: " .. tostring(derived.s0))
        assert(derived.s1 == "47dc733c", "s1 mismatch: " .. tostring(derived.s1))
        assert(derived.s2 == "af0684fb", "s2 mismatch: " .. tostring(derived.s2))
        assert(derived.s3 == "b5b1f64c", "s3 mismatch: " .. tostring(derived.s3))

        with_mock_state("0000000000000000", function(state)
            local stream = "core:random_stream.gameplay"
            local expected = {
                0xe020c7cb,
                0xf94e13e0,
                0xa66c9e06,
                0x086a074f,
                0x81883abc,
            }
            for i, exp_val in ipairs(expected) do
                local actual = _G.game.random.next_u32(stream)
                assert(actual == exp_val, string.format("Output #%d mismatch: expected %08x, got %08x", i, exp_val, actual))
            end

            -- Ensure state was recorded in canonical state tree
            local st = state.meta.prng[stream]
            assert(st ~= nil, "stream state must be stored in state.meta.prng")
            assert(st.algorithm == "xoshiro128ss-v1")
            assert(type(st.s0) == "string" and #st.s0 == 8)
            assert(type(st.s1) == "string" and #st.s1 == 8)
            assert(type(st.s2) == "string" and #st.s2 == 8)
            assert(type(st.s3) == "string" and #st.s3 == 8)

            -- Validate state tree passes schema validation
            state_validator.validate_state_tree(state)
        end)
    end,

    golden_vector_seed_ffff = function()
        -- Canonical vector from CanonicalStateAndSave.md:
        -- seed_hex: ffffffffffffffff, stream_id: core:random_stream.gameplay
        -- Initial words: 9370d948 28ef89cd 478ed7a1 af65c35b
        -- Outputs: 0d9c8816 8a60ae26 1241ad6f 99d5616e 73735263
        local derived = random.derive_stream("ffffffffffffffff", "core:random_stream.gameplay")
        assert(derived.algorithm == "xoshiro128ss-v1", "algorithm must be xoshiro128ss-v1")
        assert(derived.s0 == "9370d948", "s0 mismatch: " .. tostring(derived.s0))
        assert(derived.s1 == "28ef89cd", "s1 mismatch: " .. tostring(derived.s1))
        assert(derived.s2 == "478ed7a1", "s2 mismatch: " .. tostring(derived.s2))
        assert(derived.s3 == "af65c35b", "s3 mismatch: " .. tostring(derived.s3))

        with_mock_state("ffffffffffffffff", function(state)
            local stream = "core:random_stream.gameplay"
            local expected = {
                0x0d9c8816,
                0x8a60ae26,
                0x1241ad6f,
                0x99d5616e,
                0x73735263,
            }
            for i, exp_val in ipairs(expected) do
                local actual = _G.game.random.next_u32(stream)
                assert(actual == exp_val, string.format("Output #%d mismatch: expected %08x, got %08x", i, exp_val, actual))
            end
            state_validator.validate_state_tree(state)
        end)
    end,

    stream_isolation = function()
        with_mock_state("000000000000002a", function(state)
            local s1 = "core:random_stream.stream_alpha"
            local s2 = "core:random_stream.stream_beta"

            -- Draw from s1
            local v1_first = _G.game.random.next_u32(s1)
            -- Draw from s2
            local v2_first = _G.game.random.next_u32(s2)
            assert(v1_first ~= v2_first, "different stream IDs should have different outputs")

            -- Draw 10 more from s2
            for _ = 1, 10 do
                _G.game.random.next_u32(s2)
            end

            -- Now draw second from s1
            local v1_second = _G.game.random.next_u32(s1)

            -- In a fresh state with only s1, second draw must match v1_second
            with_mock_state("000000000000002a", function(_)
                local fresh_first = _G.game.random.next_u32(s1)
                local fresh_second = _G.game.random.next_u32(s1)
                assert(fresh_first == v1_first, "stream s1 first draw must be repeatable")
                assert(fresh_second == v1_second, "stream s1 second draw must be independent of calls to s2")
            end)
        end)
    end,

    next_unit_range = function()
        with_mock_state("0000000000000042", function(_)
            local stream = "core:random_stream.unit_test"
            for _ = 1, 100 do
                local u = _G.game.random.next_unit(stream)
                assert(type(u) == "number", "next_unit must return a number")
                assert(u >= 0.0 and u < 1.0, "next_unit must be in [0, 1), got: " .. tostring(u))
            end
        end)
    end,

    next_int_range_and_rejection_sampling = function()
        with_mock_state("000000000000007b", function(_)
            local stream = "core:random_stream.int_test"

            -- Test range [1, 6] (die roll)
            for _ = 1, 100 do
                local val = _G.game.random.next_int(stream, 1, 6)
                assert(math.type(val) == "integer", "next_int must return integer")
                assert(val >= 1 and val <= 6, "val out of range [1, 6]: " .. tostring(val))
            end

            -- Test negative range [-20, -10]
            for _ = 1, 50 do
                local val = _G.game.random.next_int(stream, -20, -10)
                assert(val >= -20 and val <= -10, "val out of range [-20, -10]: " .. tostring(val))
            end

            -- Single element range [42, 42]
            local single = _G.game.random.next_int(stream, 42, 42)
            assert(single == 42, "single element range must return min")

            -- Error on min > max
            local ok_bad_range, _ = pcall(function()
                _G.game.random.next_int(stream, 10, 5)
            end)
            assert(not ok_bad_range, "min > max must error")

            -- Error on non-integer
            local ok_non_int, _ = pcall(function()
                _G.game.random.next_int(stream, 1.5, 6)
            end)
            assert(not ok_non_int, "float min must error")
        end)
    end,

    state_persistence_and_no_reseed = function()
        with_mock_state("0000000000000000", function(state)
            local stream = "core:random_stream.gameplay"
            -- Draw 3 values
            local out1 = _G.game.random.next_u32(stream)
            local out2 = _G.game.random.next_u32(stream)
            local out3 = _G.game.random.next_u32(stream)

            -- Snapshot current stream state
            local saved_s0 = state.meta.prng[stream].s0
            local saved_s1 = state.meta.prng[stream].s1
            local saved_s2 = state.meta.prng[stream].s2
            local saved_s3 = state.meta.prng[stream].s3

            -- Next draw gives out4
            local out4 = _G.game.random.next_u32(stream)

            -- Now simulate loading state from save without reseed:
            -- Create a new state, restore saved words, change root seed_hex to something completely different
            with_mock_state("9999999999999999", function(loaded_state)
                loaded_state.meta.prng[stream] = {
                    algorithm = "xoshiro128ss-v1",
                    s0 = saved_s0,
                    s1 = saved_s1,
                    s2 = saved_s2,
                    s3 = saved_s3,
                }

                -- Next draw from loaded state MUST match out4 despite different root seed_hex
                local loaded_out4 = _G.game.random.next_u32(stream)
                assert(loaded_out4 == out4, string.format("Resuming from loaded state must yield exact next value: expected %08x, got %08x", out4, loaded_out4))
            end)
        end)
    end,

    invalid_stream_id_rejected = function()
        local ok, err = pcall(function()
            random.derive_stream("0000000000000000", "invalid_not_a_stable_id")
        end)
        assert(not ok, "invalid stream_id must be rejected")
        assert(string.find(tostring(err), "InvalidRandomStreamId") ~= nil)

        local ok_seed, err_seed = pcall(function()
            random.derive_stream("not_hex", "core:random_stream.test")
        end)
        assert(not ok_seed, "invalid seed_hex must be rejected")
        assert(string.find(tostring(err_seed), "InvalidSeedHex") ~= nil)
    end,
}
