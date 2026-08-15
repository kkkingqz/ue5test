local M = {
    id = "core:module.runtime.instance_allocator"
}

-- Max safe integer limit for counters (2^53 - 1)
local MAX_COUNTER = 9007199254740991

function M.is_valid_kind(kind)
    if type(kind) ~= "string" or #kind == 0 or #kind > 64 then
        return false
    end
    return kind:match("^[a-z][a-z0-9_]*$") ~= nil
end

function M.parse(id)
    if type(id) ~= "string" or #id == 0 or #id > 192 then
        return nil, "InvalidInstanceId"
    end
    local kind, counter_str = id:match("^([a-z][a-z0-9_]*)@([1-9][0-9]*)$")
    if not kind or not counter_str then
        return nil, "InvalidInstanceId"
    end
    if #kind > 64 then
        return nil, "InvalidInstanceId"
    end
    local counter = tonumber(counter_str)
    if not counter or counter > MAX_COUNTER then
        return nil, "InvalidInstanceId"
    end
    return kind, counter
end

function M.is_valid(id)
    local kind = M.parse(id)
    return kind ~= nil
end

function M.format(kind, counter)
    if not M.is_valid_kind(kind) then
        error("Invalid instance kind: " .. tostring(kind), 2)
    end
    if type(counter) ~= "number" or math.type(counter) ~= "integer" or counter < 1 or counter > MAX_COUNTER then
        error("Invalid instance counter: " .. tostring(counter), 2)
    end
    return string.format("%s@%d", kind, counter)
end

function M.allocate(state, kind)
    if not M.is_valid_kind(kind) then
        error("InvalidInstanceId: invalid kind '" .. tostring(kind) .. "'", 2)
    end
    if type(state) ~= "table" or type(state.meta) ~= "table" then
        error("LuaStateValidationInvalid: state.meta must be a table", 2)
    end
    if state.meta.instance_counters == nil then
        state.meta.instance_counters = {}
    end
    if type(state.meta.instance_counters) ~= "table" then
        error("LuaStateValidationInvalid: state.meta.instance_counters must be a table", 2)
    end

    local current = state.meta.instance_counters[kind]
    if current == nil then
        current = 1
    end
    if type(current) ~= "number" or math.type(current) ~= "integer" or current < 1 then
        error("LuaStateValidationInvalid: invalid counter in state.meta.instance_counters for kind '" .. kind .. "'", 2)
    end
    if current >= MAX_COUNTER then
        error("InstanceCounterExhausted: counter exhausted for kind '" .. kind .. "'", 2)
    end

    state.meta.instance_counters[kind] = current + 1
    return string.format("%s@%d", kind, current)
end

return M
