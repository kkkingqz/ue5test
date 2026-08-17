return {
    modules = {
        {
            module_id = "sample:module.debug.start",
            source = "debug/start.lua",
            dependencies = {
                "core:module.presentation.screen_requests",
                "core:module.resources.text",
            },
            replaceable = false,
        },
    },
}
