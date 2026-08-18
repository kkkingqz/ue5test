return {
    modules = {
        {
            module_id = "core:module.gameplay.root",
            source = "gameplay/root.lua",
            dependencies = {
                "rh:module.gameplay.actors",
                "rh:module.gameplay.shop",
                "rh:module.gameplay.time",
                "rh:module.gameplay.travel",
                "rh:module.gameplay.work",
                "rh:module.presentation.location_screen",
                "rh:module.services.economy",
            },
            replaceable = true,
        },
        {
            module_id = "rh:module.gameplay.actors",
            source = "gameplay/actors.lua",
            dependencies = {
                "core:module.authoring.context",
                "core:module.authoring.properties",
                "core:module.runtime.instance_allocator",
                "core:module.runtime.state_validator",
            },
            replaceable = false,
        },
        {
            module_id = "rh:module.gameplay.shop",
            source = "gameplay/shop.lua",
            dependencies = {
                "core:module.authoring.context",
                "rh:module.presentation.location_screen",
            },
            replaceable = false,
        },
        {
            module_id = "rh:module.gameplay.time",
            source = "gameplay/time.lua",
            dependencies = {
                "core:module.authoring.context",
                "rh:module.presentation.location_screen",
            },
            replaceable = false,
        },
        {
            module_id = "rh:module.gameplay.travel",
            source = "gameplay/travel.lua",
            dependencies = {
                "core:module.authoring.context",
            },
            replaceable = false,
        },
        {
            module_id = "rh:module.gameplay.work",
            source = "gameplay/work.lua",
            dependencies = {
                "core:module.authoring.context",
                "rh:module.presentation.location_screen",
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
        {
            module_id = "rh:module.services.economy",
            source = "services/economy.lua",
            dependencies = {},
            replaceable = false,
        },
    },
}
