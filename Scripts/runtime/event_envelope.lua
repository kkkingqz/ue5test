-- Event Envelope (GEW-06, DLA-04)
-- Canonical event envelope definition and validation for the gameplay EventBus.
-- Ensures immutable payloads, portable values only, and prevents runtime state
-- or object leakage across boundaries (CommandsAndEvents.md "Gameplay EventBus").

local stable_id = require("core:module.runtime.stable_id")
local portable_value = require("core:module.runtime.portable_value")

local M = {
    id = "core:module.runtime.event_envelope",
}

local function is_finite_number(n)
    return n == n and n ~= math.huge and n ~= -math.huge
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

    local payload = portable_value.deep_copy_and_freeze(payload_input, "payload", visited)

    local source = nil
    if spec.source ~= nil then
        if type(spec.source) ~= "table" then
            error("InvalidEventEnvelope: source must be a table", 2)
        end
        source = portable_value.deep_copy_and_freeze(spec.source, "source", visited)
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
