local stable_id = require("core:module.runtime.stable_id")

local M = {}

function M.spec(text_id, args, style)
    assert(stable_id.is_kind(text_id, "text"), "text_id must be canonical")
    assert(args == nil or type(args) == "table", "text args must be a table")
    assert(style == nil or (type(style) == "string" and style:match("^[a-z][a-z0-9_]*$")),
        "text style must be a canonical local token")
    local result = {
        text_id = text_id,
        args = args or {},
    }
    if style ~= nil then
        result.style = style
    end
    return result
end

return M
