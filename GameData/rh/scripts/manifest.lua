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
            module_id = "rh:module.runtime.session_start",
            source = "runtime/session_start.lua",
            dependencies = {
                "core:module.runtime.presentation_source",
                "rh:module.authoring.gameplay",
                "textsystem:module.presentation.location_presenter",
            },
            replaceable = false,
        },
    },
}
