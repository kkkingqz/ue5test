return {
    modules = {
        {
            module_id = "core:module.gameplay.root",
            source = "gameplay/root.lua",
            dependencies = {},
            replaceable = true,
        },
        {
            module_id = "test_mod:module.gameplay.extra",
            source = "gameplay/extra.lua",
            dependencies = {
                "core:module.runtime.stable_id",
            },
            replaceable = false,
        },
    },
}
