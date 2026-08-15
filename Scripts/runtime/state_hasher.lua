local M = {
    id = "core:module.runtime.state_hasher",
}

local K = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
}

local function ror(x, n)
    return (((x & 0xffffffff) >> n) | ((x & 0xffffffff) << (32 - n))) & 0xffffffff
end

function M.sha256(msg)
    local bit_len = #msg * 8
    local pad_len = (56 - (#msg + 1) % 64) % 64
    local padded = msg .. "\x80" .. string.rep("\x00", pad_len) .. string.pack(">I8", bit_len)

    local h0 = 0x6a09e667
    local h1 = 0xbb67ae85
    local h2 = 0x3c6ef372
    local h3 = 0xa54ff53a
    local h4 = 0x510e527f
    local h5 = 0x9b05688c
    local h6 = 0x1f83d9ab
    local h7 = 0x5be0cd19

    local words = {}
    for chunk_offset = 1, #padded, 64 do
        for i = 0, 15 do
            words[i + 1] = string.unpack(">I4", padded, chunk_offset + i * 4)
        end
        for i = 16, 63 do
            local w_minus_15 = words[i - 15 + 1]
            local s0 = (ror(w_minus_15, 7) ~ ror(w_minus_15, 18) ~ (w_minus_15 >> 3)) & 0xffffffff
            local w_minus_2 = words[i - 2 + 1]
            local s1 = (ror(w_minus_2, 17) ~ ror(w_minus_2, 19) ~ (w_minus_2 >> 10)) & 0xffffffff
            words[i + 1] = (words[i - 16 + 1] + s0 + words[i - 7 + 1] + s1) & 0xffffffff
        end

        local a = h0
        local b = h1
        local c = h2
        local d = h3
        local e = h4
        local f = h5
        local g = h6
        local h = h7

        for i = 0, 63 do
            local s1 = (ror(e, 6) ~ ror(e, 11) ~ ror(e, 25)) & 0xffffffff
            local ch = ((e & f) ~ ((~e & 0xffffffff) & g)) & 0xffffffff
            local temp1 = (h + s1 + ch + K[i + 1] + words[i + 1]) & 0xffffffff
            local s0 = (ror(a, 2) ~ ror(a, 13) ~ ror(a, 22)) & 0xffffffff
            local maj = ((a & b) ~ (a & c) ~ (b & c)) & 0xffffffff
            local temp2 = (s0 + maj) & 0xffffffff

            h = g
            g = f
            f = e
            e = (d + temp1) & 0xffffffff
            d = c
            c = b
            b = a
            a = (temp1 + temp2) & 0xffffffff
        end

        h0 = (h0 + a) & 0xffffffff
        h1 = (h1 + b) & 0xffffffff
        h2 = (h2 + c) & 0xffffffff
        h3 = (h3 + d) & 0xffffffff
        h4 = (h4 + e) & 0xffffffff
        h5 = (h5 + f) & 0xffffffff
        h6 = (h6 + g) & 0xffffffff
        h7 = (h7 + h) & 0xffffffff
    end

    return string.format("%08x%08x%08x%08x%08x%08x%08x%08x", h0, h1, h2, h3, h4, h5, h6, h7)
end

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

function M.hash_state(state)
    if type(state) ~= "table" then
        error("LuaStateHashError: state must be a table")
    end
    local canonical = M.serialize(state)
    return M.sha256(canonical)
end

return M
