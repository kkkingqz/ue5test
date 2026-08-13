local ingress = require("core:module.boundary.ingress")
local outbound = require("core:module.boundary.outbound")

local M = {}

function M.install()
    ingress.install()
    outbound.install()
end

return M
