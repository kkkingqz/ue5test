-- rh package gameplay root (replaces core:module.gameplay.root)
-- Integrates rh package gameplay modules into the runtime module graph.

local base = require_base()
local travel = require("rh:module.gameplay.travel")
local shop = require("rh:module.gameplay.shop")
local time = require("rh:module.gameplay.time")
local work = require("rh:module.gameplay.work")
local location_screen = require("rh:module.presentation.location_screen")

local M = setmetatable({
    id = "core:module.gameplay.root",
}, { __index = base })

function M.register(ctx)
    if base.register then
        base.register(ctx)
    end
    if location_screen and location_screen.register_handlers then
        location_screen.register_handlers(ctx)
    end
    if travel and travel.register_handlers then
        travel.register_handlers(ctx)
    end
    if shop and shop.register_handlers then
        shop.register_handlers(ctx, location_screen.build_and_publish_screen)
    end
    if time and time.register_handlers then
        time.register_handlers(ctx, location_screen.build_and_publish_screen)
    end
    if work and work.register_handlers then
        work.register_handlers(ctx, location_screen.build_and_publish_screen)
    end
end

return M
