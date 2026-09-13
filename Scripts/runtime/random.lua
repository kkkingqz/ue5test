local state_hasher = require("core:module.runtime.state_hasher")
local stable_id = require("core:module.runtime.stable_id")

local M = {
    id = "core:module.runtime.random",
}

local ALGORITHM_TAG = "xoshiro128ss-v1"
local TWO_POW_32 = 4294967296

local function rotl32(x, k)
    return (((x << k) & 0xffffffff) | ((x & 0xffffffff) >> (32 - k))) & 0xffffffff
end

M.rotl32 = rotl32

function M.derive_stream(seed_hex, stream_id)
    if type(seed_hex) ~= "string" or not seed_hex:match("^[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]$") then
        error("InvalidSeedHex: seed_hex must be exactly 16 lowercase hex characters: " .. tostring(seed_hex), 2)
    end
    if type(stream_id) ~= "string" or not stable_id.is_kind(stream_id, "random_stream") then
        error("InvalidRandomStreamId: stream_id must be a Stable ID of kind 'random_stream': " .. tostring(stream_id), 2)
    end

    local digest = state_hasher.sha256("gv2-prng-v1\0" .. seed_hex .. "\0" .. stream_id)
    local s0 = tonumber(string.sub(digest, 1, 8), 16)
    local s1 = tonumber(string.sub(digest, 9, 16), 16)
    local s2 = tonumber(string.sub(digest, 17, 24), 16)
    local s3 = tonumber(string.sub(digest, 25, 32), 16)

    -- All-zero state defensive check replaces s3 with 00000001
    if s0 == 0 and s1 == 0 and s2 == 0 and s3 == 0 then
        s3 = 1
    end

    return {
        algorithm = ALGORITHM_TAG,
        s0 = string.format("%08x", s0),
        s1 = string.format("%08x", s1),
        s2 = string.format("%08x", s2),
        s3 = string.format("%08x", s3),
    }
end

local function get_stream_state(stream_id)
    if not _G.game or not _G.game.state then
        error("NoActiveGameState: game.state is not available for random stream access", 3)
    end
    local state = _G.game.state
    if not state.meta or type(state.meta) ~= "table" then
        error("MalformedCanonicalState: state.meta is missing or not a table", 3)
    end
    if not state.meta.prng or type(state.meta.prng) ~= "table" then
        state.meta.prng = {}
    end

    local stream_state = state.meta.prng[stream_id]
    if stream_state == nil then
        local seed_hex = nil
        if state.meta and type(state.meta.seed_hex) == "string" then
            seed_hex = state.meta.seed_hex
        elseif _G.game.runtime and type(_G.game.runtime.seed_hex) == "string" then
            seed_hex = _G.game.runtime.seed_hex
        end
        stream_state = M.derive_stream(seed_hex, stream_id)
        state.meta.prng[stream_id] = stream_state
    end

    return stream_state
end

M.get_stream_state = get_stream_state

function M.next_u32(stream_id)
    if type(stream_id) ~= "string" or not stable_id.is_kind(stream_id, "random_stream") then
        error("InvalidRandomStreamId: stream_id must be a Stable ID of kind 'random_stream': " .. tostring(stream_id), 2)
    end

    local stream_state = get_stream_state(stream_id)
    local s0 = tonumber(stream_state.s0, 16)
    local s1 = tonumber(stream_state.s1, 16)
    local s2 = tonumber(stream_state.s2, 16)
    local s3 = tonumber(stream_state.s3, 16)

    -- xoshiro128** algorithm
    local result = (rotl32((s1 * 5) & 0xffffffff, 7) * 9) & 0xffffffff
    local t = (s1 << 9) & 0xffffffff

    s2 = (s2 ~ s0) & 0xffffffff
    s3 = (s3 ~ s1) & 0xffffffff
    s1 = (s1 ~ s2) & 0xffffffff
    s0 = (s0 ~ s3) & 0xffffffff
    s2 = (s2 ~ t) & 0xffffffff
    s3 = rotl32(s3, 11)

    stream_state.s0 = string.format("%08x", s0)
    stream_state.s1 = string.format("%08x", s1)
    stream_state.s2 = string.format("%08x", s2)
    stream_state.s3 = string.format("%08x", s3)

    return result
end

function M.next_unit(stream_id)
    return M.next_u32(stream_id) / TWO_POW_32
end

function M.next_int(stream_id, min_val, max_val)
    if type(min_val) ~= "number" or math.type(min_val) ~= "integer" then
        error("InvalidRandomRange: min must be an integer: " .. tostring(min_val), 2)
    end
    if type(max_val) ~= "number" or math.type(max_val) ~= "integer" then
        error("InvalidRandomRange: max must be an integer: " .. tostring(max_val), 2)
    end
    if min_val > max_val then
        error("InvalidRandomRange: min (" .. tostring(min_val) .. ") cannot be greater than max (" .. tostring(max_val) .. ")", 2)
    end

    local span = max_val - min_val + 1
    if span > TWO_POW_32 then
        error("InvalidRandomRange: span exceeds 2^32 (" .. tostring(span) .. ")", 2)
    end

    if span == TWO_POW_32 then
        return min_val + M.next_u32(stream_id)
    end

    -- Rejection sampling to prevent modulo bias
    local limit = TWO_POW_32 - (TWO_POW_32 % span)
    while true do
        local r = M.next_u32(stream_id)
        if r < limit then
            return min_val + (r % span)
        end
    end
end

return M
