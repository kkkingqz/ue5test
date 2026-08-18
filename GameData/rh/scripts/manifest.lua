return {
    modules = {
        {
            module_id = "rh:module.authoring.gameplay",
            source = "authoring/gameplay.lua",
            dependencies = {},
            replaceable = false,
            authoring = true,
        },
        {
            module_id = "rh:module.gameplay.actors",
            source = "gameplay/actors.lua",
            dependencies = {
                "core:module.authoring.context",
                "core:module.runtime.instance_allocator",
            },
            replaceable = false,
        },
        {
            module_id = "rh:module.presentation.location_screen",
            source = "presentation/location_screen.lua",
            dependencies = {
                "core:module.authoring.context",
            },
            replaceable = false,
        },
    },
}
