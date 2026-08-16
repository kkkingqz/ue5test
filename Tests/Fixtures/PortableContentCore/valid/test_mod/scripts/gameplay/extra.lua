-- test_mod extra gameplay module (PKG-16/19)
-- Demonstrates a new module in a mod's own namespace.

local stable_id = require("core:module.runtime.stable_id")

local M = {
    id = "test_mod:module.gameplay.extra",
}

function M.is_extra_id(id)
    return stable_id.is_kind(id, "screen")
end

return M
