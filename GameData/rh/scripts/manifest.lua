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
            dependencies = {},
            replaceable = false,
            authoring = true,
        },
        {
            module_id = "rh:module.presentation.bootstrap",
            source = "presentation/bootstrap.lua",
            dependencies = {},
            replaceable = false,
        },
    },
}
