local stable_id = require("core:module.runtime.stable_id")

local M = {}
local pending_screen = nil

function M.create(screen_id, fields)
    assert(stable_id.is_kind(screen_id, "screen"), "screen_id must be canonical")
    assert(type(fields) == "table", "fields must be an object")
    for field_id, field in pairs(fields) do
        assert(type(field_id) == "string" and field_id:match("^[a-z][a-z0-9_]*$"),
            "field_id must be canonical")
        assert(type(field) == "table", "field must be a table")
        assert(stable_id.is_kind(field.schema_id, "schema"), "field.schema_id must be canonical")
        assert(field.value ~= nil, "field.value is required")
    end

    return {
        screen_id = screen_id,
        fields = fields,
    }
end

function M.publish(screen)
    local ok_ctx, ctx_mod = pcall(require, "core:module.authoring.context")
    if ok_ctx and ctx_mod and ctx_mod.guard_validator_side_effect then
        ctx_mod.guard_validator_side_effect("show_screen")
    end
    assert(type(screen) == "table", "screen request must be a table")
    pending_screen = screen
end

function M.take_pending()
    local screen = pending_screen
    pending_screen = nil
    return screen
end

return M
