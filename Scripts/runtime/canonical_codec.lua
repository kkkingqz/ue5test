-- Canonical Codec (SAV-01, plan SaveAndLoad).
-- Reversible canonical encoding for any portable Lua value tree (the same
-- shape as canonical gameplay state): every type is tagged, strings and
-- object keys are length-prefixed, containers are counted. This is the one
-- encoding shared by state hashing (core:module.runtime.state_hasher) and
-- the save container (ADR-0021) — a second implementation is forbidden.
--
-- Format (SAV-04: this is save-compatibility-relevant; a change here
-- requires raising both M.VERSION and meta.save_version):
--   nil / game.null      -> "n"
--   boolean               -> "b1" | "b0"
--   integer                -> "i<decimal>;"
--   finite float           -> "d<16 lowercase hex chars>" (IEEE754 big-endian bits)
--   string                  -> "s<byte length>:<raw bytes>"
--   dense array (int keys)  -> "[<count>:<serialized element>...]"
--   string-key object       -> "{<key count>:<key length>:<key bytes><serialized value>...}"

local M = {
    id = "core:module.runtime.canonical_codec",
}

-- Bumped whenever the encoding itself changes (tag set, length/count
-- framing, float representation). Independent of save_version, which
-- versions the save *container*; a codec version bump forces a
-- save_version bump too (SAV-04), never the reverse.
M.VERSION = 1

function M.serialize(val)
    local t = type(val)
    if val == nil then
        return "n"
    end
    if game and game.null and val == game.null then
        return "n"
    end
    if t == "boolean" then
        return val and "b1" or "b0"
    end
    if t == "number" then
        if math.type(val) == "integer" then
            return "i" .. tostring(val) .. ";"
        else
            local packed = string.pack(">d", val)
            local bits = string.unpack(">I8", packed)
            return "d" .. string.format("%016x", bits)
        end
    end
    if t == "string" then
        return "s" .. tostring(#val) .. ":" .. val
    end
    if t == "table" then
        local count = 0
        local has_int = false
        local has_str = false
        for k, _ in pairs(val) do
            count = count + 1
            if type(k) == "number" then
                has_int = true
            else
                has_str = true
            end
        end

        if has_int and not has_str then
            local parts = { "[" .. tostring(#val) .. ":" }
            for i = 1, #val do
                parts[#parts + 1] = M.serialize(val[i])
            end
            parts[#parts + 1] = "]"
            return table.concat(parts)
        else
            local keys = {}
            for k, _ in pairs(val) do
                keys[#keys + 1] = tostring(k)
            end
            table.sort(keys)
            local parts = { "{" .. tostring(#keys) .. ":" }
            for _, k in ipairs(keys) do
                parts[#parts + 1] = tostring(#k) .. ":" .. k
                parts[#parts + 1] = M.serialize(val[k])
            end
            parts[#parts + 1] = "}"
            return table.concat(parts)
        end
    end
    error("LuaStateHashError: unsupported state value type '" .. t .. "'")
end

-- SAV-02/SAV-03: reverse operation. `fail` raises a typed, positioned error
-- on any malformed input (truncation, unknown tag, declared length/count
-- mismatch, trailing bytes) instead of returning a partially-built tree —
-- error() unwinds the whole recursive descent, so no partial table can ever
-- reach the caller.
local function fail(pos, detail)
    error("CanonicalCodecCorrupt: " .. detail .. " at byte offset " .. tostring(pos), 0)
end

local function read_unsigned(str, pos, stop_char, what)
    local stop = str:find(stop_char, pos, true)
    if not stop then
        fail(pos, what .. " missing '" .. stop_char .. "'")
    end
    local digits = str:sub(pos, stop - 1)
    if not digits:match("^%d+$") then
        fail(pos, what .. " is not a non-negative integer")
    end
    return tonumber(digits), stop + 1
end

local function hex_to_bytes(hex)
    return (hex:gsub("..", function(byte_hex)
        return string.char(tonumber(byte_hex, 16))
    end))
end

local decode_value

local function decode_array(str, pos)
    local count, next_pos = read_unsigned(str, pos, ":", "array count")
    if count > #str - next_pos + 1 then
        fail(pos, "array count exceeds remaining input")
    end
    pos = next_pos
    local result = {}
    for i = 1, count do
        result[i], pos = decode_value(str, pos)
    end
    if str:sub(pos, pos) ~= "]" then
        fail(pos, "array missing closing ']'")
    end
    return result, pos + 1
end

local function decode_object(str, pos)
    local count, next_pos = read_unsigned(str, pos, ":", "object key count")
    if count > #str - next_pos + 1 then
        fail(pos, "object key count exceeds remaining input")
    end
    pos = next_pos
    local result = {}
    for _ = 1, count do
        local key_len
        key_len, pos = read_unsigned(str, pos, ":", "object key length")
        if key_len > #str - pos + 1 then
            fail(pos, "object key length exceeds remaining input")
        end
        local key = str:sub(pos, pos + key_len - 1)
        pos = pos + key_len
        local value
        value, pos = decode_value(str, pos)
        result[key] = value
    end
    if str:sub(pos, pos) ~= "}" then
        fail(pos, "object missing closing '}'")
    end
    return result, pos + 1
end

decode_value = function(str, pos)
    local tag = str:sub(pos, pos)
    if tag == "" then
        fail(pos, "unexpected end of input")
    end

    if tag == "n" then
        return game.null, pos + 1
    end

    if tag == "b" then
        local flag = str:sub(pos + 1, pos + 1)
        if flag == "1" then return true, pos + 2 end
        if flag == "0" then return false, pos + 2 end
        fail(pos, "invalid boolean tag")
    end

    if tag == "i" then
        local semicolon = str:find(";", pos + 1, true)
        if not semicolon then
            fail(pos, "integer missing terminating ';'")
        end
        local digits = str:sub(pos + 1, semicolon - 1)
        if not digits:match("^%-?%d+$") then
            fail(pos, "integer is not a valid decimal literal")
        end
        local value = math.tointeger(tonumber(digits))
        if value == nil then
            fail(pos, "integer literal out of range")
        end
        return value, semicolon + 1
    end

    if tag == "d" then
        local hex = str:sub(pos + 1, pos + 16)
        if #hex ~= 16 or not hex:match("^%x+$") then
            fail(pos, "float requires 16 hex digits")
        end
        local value = string.unpack(">d", hex_to_bytes(hex))
        return value, pos + 17
    end

    if tag == "s" then
        local len, next_pos = read_unsigned(str, pos + 1, ":", "string length")
        if len > #str - next_pos + 1 then
            fail(pos, "string length exceeds remaining input")
        end
        local value = str:sub(next_pos, next_pos + len - 1)
        return value, next_pos + len
    end

    if tag == "[" then
        return decode_array(str, pos + 1)
    end

    if tag == "{" then
        return decode_object(str, pos + 1)
    end

    fail(pos, "unknown type tag '" .. tag .. "'")
end

function M.deserialize(str)
    if type(str) ~= "string" then
        error("CanonicalCodecCorrupt: input must be a string", 0)
    end
    if #str == 0 then
        fail(1, "unexpected end of input")
    end
    local value, pos = decode_value(str, 1)
    if pos ~= #str + 1 then
        fail(pos, "trailing bytes after top-level value")
    end
    return value
end

return M
