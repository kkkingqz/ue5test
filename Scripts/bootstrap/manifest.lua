return {
    entry_module_id = "core:module.bootstrap.main",
    modules = {
        {
            module_id = "core:module.runtime.stable_id",
            source = "runtime/stable_id.lua",
            dependencies = {},
        },
        {
            module_id = "core:module.runtime.command_dispatcher",
            source = "runtime/command_dispatcher.lua",
            dependencies = {},
        },
        {
            module_id = "core:module.gameplay.root",
            source = "gameplay/root.lua",
            dependencies = {},
        },
        {
            module_id = "core:module.presentation.screen_requests",
            source = "presentation/screen_requests.lua",
            dependencies = {
                "core:module.runtime.stable_id",
            },
        },
        {
            module_id = "core:module.resources.text",
            source = "resources/text.lua",
            dependencies = {
                "core:module.runtime.stable_id",
            },
        },
        {
            module_id = "core:module.resources.service",
            source = "resources/service.lua",
            dependencies = {
                "core:module.runtime.stable_id",
            },
        },
        {
            module_id = "core:module.debug.start",
            source = "debug/start.lua",
            dependencies = {
                "core:module.presentation.screen_requests",
                "core:module.resources.text",
            },
        },
        {
            module_id = "core:module.boundary.ingress",
            source = "boundary/ingress.lua",
            dependencies = {
                "core:module.runtime.command_dispatcher",
                "core:module.gameplay.root",
                "core:module.debug.start",
            },
        },
        {
            module_id = "core:module.boundary.outbound",
            source = "boundary/outbound.lua",
            dependencies = {
                "core:module.presentation.screen_requests",
            },
        },
        {
            module_id = "core:module.boundary.entrypoints",
            source = "boundary/entrypoints.lua",
            dependencies = {
                "core:module.boundary.ingress",
                "core:module.boundary.outbound",
            },
        },
        {
            module_id = "core:module.bootstrap.main",
            source = "bootstrap/main.lua",
            dependencies = {
                "core:module.boundary.entrypoints",
                "core:module.resources.service",
            },
        },
    },
}
