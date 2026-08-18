return {
    modules = {
        {
            module_id = "sample:module.authoring.gameplay",
            source = "authoring/gameplay.lua",
            dependencies = {},
            replaceable = false,
            authoring = true,
        },
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
