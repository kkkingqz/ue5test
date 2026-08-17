-- rh package gameplay root (replaces core:module.gameplay.root)
-- Integrates rh package gameplay modules into the runtime module graph.

local base = require_base()
local travel = require("rh:module.gameplay.travel")

local M = setmetatable({
    id = "core:module.gameplay.root",
}, { __index = base })

function M.register(ctx)
    if base.register then
        base.register(ctx)
    end
    if travel and travel.register_handlers then
        travel.register_handlers(ctx)
    end
end

return M
