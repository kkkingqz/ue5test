-- test_mod gameplay root override (PKG-17/19)
-- Demonstrates module extension using require_base() and metatable prototype inheritance.

local base = require_base()

local M = setmetatable({
    id = "core:module.gameplay.root",
    is_test_mod_override = true,
}, { __index = base })

function M.handle_command(request)
    if request and request.command_id == "test_mod:command.ping" then
        return {
            ok = true,
            value = { pong = true },
        }
    end
    return base.handle_command(request)
end

function M.register(ctx)
    if base.register then
        base.register(ctx)
    end
end

return M
