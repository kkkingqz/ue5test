-- SAV-02 / SAV-03: core:module.runtime.canonical_codec roundtrip and
-- corruption handling. Runs against the real production session (world/'s
-- session) — the codec is stateless pure functions, no fixture needed.

local codec = require("core:module.runtime.canonical_codec")

local function assert_roundtrip(value, description)
    local encoded = codec.serialize(value)
    local decoded = codec.deserialize(encoded)
    local re_encoded = codec.serialize(decoded)
    assert(re_encoded == encoded,
        description .. ": roundtrip must reproduce the exact same canonical string, got '"
            .. re_encoded .. "' vs '" .. encoded .. "'")
end

return {
    -- SAV-04: an accidental encoding change must not go unnoticed. This
    -- pins the exact canonical string for a fixed representative value; if
    -- the encoding changes (tag set, delimiters, float representation,
    -- key sort order), this fails immediately on the same commit. A
    -- deliberate encoding change updates the pinned string here together
    -- with bumping canonical_codec.M.VERSION and meta.save_version
    -- (CanonicalStateAndSave.md "Кодек и save_version") — never one alone.
    codec_version_and_encoding_are_pinned = function()
        assert(codec.VERSION == 1,
            "canonical_codec.VERSION changed — this must be a deliberate bump together with "
                .. "meta.save_version, not an accidental side effect")
        local fixed = { a = 1, b = "hi", c = { 1, 2 }, d = true, e = game.null, f = 2.5 }
        local encoded = codec.serialize(fixed)
        assert(encoded == "{6:1:ai1;1:bs2:hi1:c[2:i1;i2;]1:db11:en1:fd4004000000000000}",
            "canonical encoding of a fixed representative value changed unexpectedly, got: " .. encoded)
    end,

    null_roundtrips_as_game_null = function()
        local encoded = codec.serialize(game.null)
        assert(encoded == "n", "game.null must serialize to 'n'")
        local decoded = codec.deserialize(encoded)
        assert(decoded == game.null, "'n' must deserialize back to game.null, not Lua nil or an empty table")
    end,

    bool_roundtrips = function()
        assert_roundtrip(true, "true")
        assert_roundtrip(false, "false")
    end,

    integer_roundtrips = function()
        assert_roundtrip(0, "zero")
        assert_roundtrip(1, "positive")
        assert_roundtrip(-1, "negative")
        assert_roundtrip(9223372036854775807, "max int64")
        assert_roundtrip(-9223372036854775808, "min int64")
    end,

    finite_double_roundtrips = function()
        assert_roundtrip(0.5, "half")
        assert_roundtrip(-0.5, "negative half")
        assert_roundtrip(3.14159, "pi-ish")
        local encoded_zero = codec.serialize(0.0)
        local decoded_zero = codec.deserialize(encoded_zero)
        assert(math.type(decoded_zero) == "float", "a serialized float must decode back as a float, not an integer")
        assert(decoded_zero == 0.0, "0.0 must roundtrip to 0.0")
    end,

    string_with_arbitrary_bytes_roundtrips = function()
        assert_roundtrip("", "empty string")
        assert_roundtrip("hello", "plain ascii")
        assert_roundtrip("a:b]c}d;e\0f\nbytes", "string containing every codec delimiter byte and a NUL/newline")
    end,

    dense_array_roundtrips = function()
        assert_roundtrip({}, "empty array is ambiguous with empty object, but {} with no keys must still roundtrip")
        assert_roundtrip({ 1, 2, 3 }, "dense integer-keyed array")
        assert_roundtrip({ "a", "b", { 1, 2 } }, "nested array")
    end,

    string_key_object_roundtrips_and_sorts_keys = function()
        assert_roundtrip({ a = 1, b = 2 }, "already-sorted object")
        local encoded = codec.serialize({ z = 1, a = 2, m = 3 })
        -- Keys must appear in sorted order regardless of construction order.
        local a_pos = encoded:find("1:a", 1, true)
        local m_pos = encoded:find("1:m", 1, true)
        local z_pos = encoded:find("1:z", 1, true)
        assert(a_pos and m_pos and z_pos and a_pos < m_pos and m_pos < z_pos,
            "object keys must serialize in sorted order regardless of insertion order")
        assert_roundtrip({ z = 1, a = 2, m = 3 }, "out-of-order-constructed object")
    end,

    nested_state_shaped_tree_roundtrips = function()
        assert_roundtrip({
            meta = { schema_version = 1, save_id = "abc" },
            actors = { ["actor@1"] = { instance_id = "actor@1", gold = 10, dead = false, note = game.null } },
            item_instances = {},
            world = { current_location_id = "core:location.city.market" },
        }, "a canonical-state-shaped tree")
    end,

    truncated_input_is_a_typed_corruption_error = function()
        local ok, err = pcall(function() codec.deserialize("s5:hi") end)
        assert(not ok, "a string claiming length 5 but providing only 2 bytes must fail")
        assert(tostring(err):find("CanonicalCodecCorrupt") ~= nil, "the error must be typed CanonicalCodecCorrupt, got: " .. tostring(err))
    end,

    unknown_tag_is_a_typed_corruption_error = function()
        local ok, err = pcall(function() codec.deserialize("q") end)
        assert(not ok, "an unknown type tag must fail")
        assert(tostring(err):find("CanonicalCodecCorrupt") ~= nil, "got: " .. tostring(err))
    end,

    mismatched_array_count_is_a_typed_corruption_error = function()
        -- Declares 3 elements but only provides 1; the parser runs out of
        -- input trying to read the 2nd, well before reaching ']'.
        local ok, err = pcall(function() codec.deserialize("[3:i1;]") end)
        assert(not ok, "an array count that overstates the actual element count must fail")
        assert(tostring(err):find("CanonicalCodecCorrupt") ~= nil, "got: " .. tostring(err))
    end,

    mismatched_object_key_count_is_a_typed_corruption_error = function()
        local ok, err = pcall(function() codec.deserialize("{2:1:ai1;}") end)
        assert(not ok, "an object key count that overstates the actual key count must fail")
        assert(tostring(err):find("CanonicalCodecCorrupt") ~= nil, "got: " .. tostring(err))
    end,

    trailing_bytes_after_top_level_value_is_a_typed_corruption_error = function()
        local ok, err = pcall(function() codec.deserialize("i1;garbage") end)
        assert(not ok, "trailing bytes after a complete top-level value must fail")
        assert(tostring(err):find("CanonicalCodecCorrupt") ~= nil, "got: " .. tostring(err))
    end,

    corrupt_input_never_returns_a_partial_tree = function()
        -- decode_value raises via error() as soon as the 2nd array element
        -- fails to parse; Lua error() unwinds the whole recursive descent,
        -- so pcall must observe only (false, error), never a partial table
        -- as a spurious extra return value.
        local results = { pcall(function() return codec.deserialize("[2:i1;q]") end) }
        assert(#results == 2, "a corrupted decode must produce exactly (false, error), never a partial value")
        assert(results[1] == false, "decode of corrupted input must fail")
    end,

    corruption_error_reports_a_byte_position = function()
        local ok, err = pcall(function() codec.deserialize("q") end)
        assert(not ok, "expected failure")
        assert(tostring(err):find("byte offset") ~= nil, "the error must report a byte offset, got: " .. tostring(err))
    end,
}
