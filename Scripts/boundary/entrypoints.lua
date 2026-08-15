local ingress = require("core:module.boundary.ingress")
local outbound = require("core:module.boundary.outbound")

local M = {
    id = "core:module.boundary.entrypoints",
    ingress = ingress,
    outbound = outbound,
}

return M
