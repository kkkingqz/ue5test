return {
    modules = {
        {
            module_id = "core:module.gameplay.root",
            source = "gameplay/root.lua",
            dependencies = {
                "rh:module.services.economy",
            },
            replaceable = true,
        },
        {
            module_id = "rh:module.services.economy",
            source = "services/economy.lua",
            dependencies = {},
            replaceable = false,
        },
    },
}
