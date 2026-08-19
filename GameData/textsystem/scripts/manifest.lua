return {
    modules = {
        {
            module_id = "textsystem:module.gameplay.actors",
            source = "gameplay/actors.lua",
            dependencies = {
                "core:module.runtime.state_validator",
            },
            replaceable = false,
            authoring = true,
        },
        {
            module_id = "textsystem:module.presentation.location_presenter",
            source = "presentation/location_presenter.lua",
            dependencies = {
                "core:module.authoring.context",
            },
            replaceable = false,
        },
    },
}
