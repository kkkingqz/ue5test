local M = {
    id = "core:module.runtime.stable_id",
}

local function is_segment(value)
    return type(value) == "string"
        and #value >= 1
        and #value <= 64
        and value:match("^[a-z][a-z0-9_]*$") ~= nil
end

function M.is_kind(value, expected_kind)
    if type(value) ~= "string" or #value > 192 or not is_segment(expected_kind) then
        return false
    end

    local namespace, kind, path = value:match("^([^:]+):([^.]+)%.(.+)$")
    if not is_segment(namespace)
        or kind ~= expected_kind
        or path == nil
        or path:sub(1, 1) == "."
        or path:sub(-1) == "."
        or path:find("..", 1, true) ~= nil
    then
        return false
    end

    local consumed = 0
    for segment in path:gmatch("[^.]+") do
        if not is_segment(segment) then
            return false
        end
        consumed = consumed + #segment
    end

    local separators = select(2, path:gsub("%.", ""))
    return consumed + separators == #path
end

return M
