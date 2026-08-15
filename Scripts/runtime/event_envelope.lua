-- Event Envelope (GEW-06)
-- Canonical event envelope definition and validation for the gameplay EventBus.
-- Ensures immutable payloads, portable values only, and prevents runtime state
-- or object leakage across boundaries (CommandsAndEvents.md "Gameplay EventBus").

local stable_id = require("core:module.runtime.stable_id")

local M = {
    id = "core:module.runtime.event_envelope",
}

local function is_finite_number(n)
    return n == n and n ~= math.huge and n ~= -math.huge
end

-- Forward declaration
local deep_copy_and_freeze

local function freeze_table(raw_tbl, path)
    local proxy = {}
    local mt = {
        __index = raw_tbl,
        __newindex = function(_, k, _)
            error("EventEnvelopeImmutable: attempt to modify read-only event payload at " .. path .. "." .. tostring(k), 2)
        end,
        __pairs = function(_) return pairs(raw_tbl) end,
        __ipairs = function(_) return ipairs(raw_tbl) end,
        __len = function(_) return #raw_tbl end,
        __tostring = function(_) return "EventPayloadProxy(" .. path .. ")" end,
        __metatable = false,
    }
    return setmetatable(proxy, mt)
end

deep_copy_and_freeze = function(val, path, visited)
    local t = type(val)

    if t == "nil" or t == "boolean" or t == "string" then
        return val
    end

    if t == "number" then
        if not is_finite_number(val) then
            error("InvalidEventPayload: non-finite number at " .. path, 2)
        end
        return val
    end

    if t == "table" then
        if game and game.null and val == game.null then
            return val
        end

        local mt = getmetatable(val)
        if mt ~= nil then
            error("InvalidEventPayload: runtime object or wrapper with metatable is not allowed at " .. path, 2)
        end

        if visited[val] then
            error("InvalidEventPayload: cyclic or shared table reference detected at " .. path, 2)
        end
        visited[val] = true

        -- Validate keys: either dense array (1..N) or string-key dictionary
        local count = 0
        local has_int = false
        local has_str = false
        for k, _ in pairs(val) do
            count = count + 1
            if type(k) == "number" then
                if math.type(k) ~= "integer" or k < 1 then
                    error("InvalidEventPayload: non-positive integer key at " .. path, 2)
                end
                has_int = true
            elseif type(k) == "string" then
                if k == "" then
                    error("InvalidEventPayload: empty string key at " .. path, 2)
                end
                has_str = true
            else
                error("InvalidEventPayload: unsupported key type '" .. type(k) .. "' at " .. path, 2)
            end
        end

        if has_int and has_str then
            error("InvalidEventPayload: mixed array and map keys at " .. path, 2)
        end

        local copy = {}
        if has_int then
            for i = 1, count do
                if val[i] == nil then
                    error("InvalidEventPayload: sparse array key gap at " .. path .. "[" .. i .. "]", 2)
                end
                copy[i] = deep_copy_and_freeze(val[i], path .. "[" .. i .. "]", visited)
            end
        else
            for k, v in pairs(val) do
                copy[k] = deep_copy_and_freeze(v, path .. "." .. k, visited)
            end
        end

        visited[val] = nil
        return freeze_table(copy, path)
    end

    error("InvalidEventPayload: unsupported value type '" .. t .. "' at " .. path, 2)
end

local ENVELOPE_MARKER = setmetatable({}, { __mode = "k" })

function M.is_envelope(val)
    return type(val) == "table" and ENVELOPE_MARKER[val] == true
end

function M.create(spec)
    if type(spec) ~= "table" then
        error("InvalidEventEnvelope: spec must be a table", 2)
    end

    local event_id = spec.event_id
    if type(event_id) ~= "string" or not stable_id.is_kind(event_id, "event") then
        error("InvalidEventEnvelope: event_id must be a canonical Stable ID of kind 'event', got '" .. tostring(event_id) .. "'", 2)
    end

    local schema_version = spec.schema_version
    if schema_version == nil then
        schema_version = 1
    elseif type(schema_version) ~= "number" or math.type(schema_version) ~= "integer" or schema_version < 1 then
        error("InvalidEventEnvelope: schema_version must be a positive integer, got " .. tostring(schema_version), 2)
    end

    local correlation_id = spec.correlation_id
    if correlation_id ~= nil and type(correlation_id) ~= "string" then
        error("InvalidEventEnvelope: correlation_id must be a string", 2)
    end

    local causation_id = spec.causation_id
    if causation_id ~= nil and type(causation_id) ~= "string" then
        error("InvalidEventEnvelope: causation_id must be a string", 2)
    end

    local sequence = spec.sequence
    if sequence ~= nil and (type(sequence) ~= "number" or math.type(sequence) ~= "integer" or sequence < 0) then
        error("InvalidEventEnvelope: sequence must be a non-negative integer", 2)
    end

    local timestamp = spec.timestamp
    if timestamp ~= nil and (type(timestamp) ~= "number" or not is_finite_number(timestamp)) then
        error("InvalidEventEnvelope: timestamp must be a finite number", 2)
    end

    local visited = {}

    local payload_input = spec.payload
    if payload_input == nil then
        payload_input = {}
    elseif type(payload_input) ~= "table" then
        error("InvalidEventEnvelope: payload must be a table", 2)
    end

    local payload = deep_copy_and_freeze(payload_input, "payload", visited)

    local source = nil
    if spec.source ~= nil then
        if type(spec.source) ~= "table" then
            error("InvalidEventEnvelope: source must be a table", 2)
        end
        source = deep_copy_and_freeze(spec.source, "source", visited)
    end

    local raw_envelope = {
        event_id = event_id,
        schema_version = schema_version,
        payload = payload,
        correlation_id = correlation_id,
        causation_id = causation_id,
        source = source,
        sequence = sequence,
        timestamp = timestamp,
    }

    local envelope = {}
    ENVELOPE_MARKER[envelope] = true

    local mt = {
        __index = raw_envelope,
        __newindex = function(_, k, _)
            error("EventEnvelopeImmutable: attempt to modify read-only event envelope field '" .. tostring(k) .. "'", 2)
        end,
        __pairs = function(_) return pairs(raw_envelope) end,
        __tostring = function(_) return "EventEnvelope(" .. event_id .. ")" end,
        __metatable = false,
    }

    return setmetatable(envelope, mt)
end

return M
