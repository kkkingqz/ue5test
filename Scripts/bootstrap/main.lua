local boundary = require("core:module.boundary.entrypoints")
local resources = require("core:module.resources.service")

boundary.install()

return {
    resources = resources,
}
