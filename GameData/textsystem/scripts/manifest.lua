return {
    modules = {
        {
            module_id = "textsystem:module.gameplay.actors",
            source = "gameplay/actors.lua",
            dependencies = {
                "core:module.authoring.context",
                "core:module.authoring.properties",
                "core:module.runtime.state_validator",
            },
            replaceable = false,
        },
    },
}
