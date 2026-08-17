-- rh package gameplay root (replaces core:module.gameplay.root)
-- Integrates rh package gameplay modules into the runtime module graph.

local base = require_base()

local M = setmetatable({
    id = "core:module.gameplay.root",
}, { __index = base })

function M.register(ctx)
    if base.register then
        base.register(ctx)
    end
end

return M
