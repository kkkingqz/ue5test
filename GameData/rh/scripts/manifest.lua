return {
    modules = {
        {
            module_id = "core:module.gameplay.root",
            source = "gameplay/root.lua",
            dependencies = {
                "rh:module.services.economy",
                "rh:module.gameplay.travel",
            },
            replaceable = true,
        },
        {
            module_id = "rh:module.services.economy",
            source = "services/economy.lua",
            dependencies = {},
            replaceable = false,
        },
        {
            module_id = "rh:module.gameplay.travel",
            source = "gameplay/travel.lua",
            dependencies = {
                "core:module.runtime.stable_id",
            },
            replaceable = false,
        },
    },
}
