return {
    entry_module_id = "core:module.bootstrap.main",
    modules = {
        {
            module_id = "core:module.runtime.stable_id",
            source = "runtime/stable_id.lua",
            dependencies = {},
        },
        {
            module_id = "core:module.runtime.mutation_window",
            source = "runtime/mutation_window.lua",
            dependencies = {},
        },
        {
            module_id = "core:module.runtime.command_dispatcher",
            source = "runtime/command_dispatcher.lua",
            dependencies = {
                "core:module.runtime.mutation_window",
                "core:module.runtime.stable_id",
                "core:module.runtime.event_bus",
            },
        },
        {
            module_id = "core:module.runtime.state_validator",
            source = "runtime/state_validator.lua",
            dependencies = {
                "core:module.runtime.stable_id",
            },
        },
        {
            module_id = "core:module.runtime.instance_allocator",
            source = "runtime/instance_allocator.lua",
            dependencies = {},
        },
        {
            module_id = "core:module.runtime.canonical_codec",
            source = "runtime/canonical_codec.lua",
            dependencies = {},
        },
        {
            module_id = "core:module.runtime.state_hasher",
            source = "runtime/state_hasher.lua",
            dependencies = {
                "core:module.runtime.canonical_codec",
            },
        },
        {
            module_id = "core:module.runtime.migrate",
            source = "runtime/migrate.lua",
            dependencies = {},
        },
        {
            module_id = "core:module.runtime.save",
            source = "runtime/save.lua",
            dependencies = {
                "core:module.runtime.canonical_codec",
                "core:module.runtime.state_hasher",
                "core:module.runtime.migrate",
            },
        },
        {
            module_id = "core:module.runtime.load",
            source = "runtime/load.lua",
            dependencies = {
                "core:module.runtime.canonical_codec",
                "core:module.runtime.state_hasher",
                "core:module.runtime.save",
                "core:module.runtime.state_validator",
                "core:module.runtime.migrate",
            },
        },
        {
            module_id = "core:module.runtime.actor_registry",
            source = "runtime/actor_registry.lua",
            dependencies = {
                "core:module.runtime.instance_allocator",
            },
        },
        {
            module_id = "core:module.runtime.world",
            source = "runtime/world.lua",
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
                "core:module.runtime.state_hasher",
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
            module_id = "core:module.runtime.service_registry",
            source = "runtime/service_registry.lua",
            dependencies = {},
        },
        {
            module_id = "core:module.runtime.validator_registry",
            source = "runtime/validator_registry.lua",
            dependencies = {
                "core:module.runtime.stable_id",
            },
        },
        {
            module_id = "core:module.runtime.event_envelope",
            source = "runtime/event_envelope.lua",
            dependencies = {
                "core:module.runtime.stable_id",
            },
        },
        {
            module_id = "core:module.runtime.subscriber_registry",
            source = "runtime/subscriber_registry.lua",
            dependencies = {
                "core:module.runtime.stable_id",
            },
        },
        {
            module_id = "core:module.runtime.event_bus",
            source = "runtime/event_bus.lua",
            dependencies = {
                "core:module.runtime.event_envelope",
                "core:module.runtime.stable_id",
                "core:module.runtime.subscriber_registry",
            },
        },
        {
            module_id = "core:module.gameplay.location_service",
            source = "gameplay/location_service.lua",
            dependencies = {
                "core:module.runtime.stable_id",
            },
        },
        {
            module_id = "core:module.bootstrap.main",
            source = "bootstrap/main.lua",
            dependencies = {
                "core:module.boundary.entrypoints",
                "core:module.resources.service",
                "core:module.runtime.state_validator",
                "core:module.runtime.instance_allocator",
                "core:module.runtime.state_hasher",
                "core:module.runtime.migrate",
                "core:module.runtime.save",
                "core:module.runtime.load",
                "core:module.runtime.mutation_window",
                "core:module.runtime.actor_registry",
                "core:module.runtime.world",
                "core:module.runtime.service_registry",
                "core:module.runtime.validator_registry",
                "core:module.runtime.subscriber_registry",
                "core:module.runtime.event_envelope",
                "core:module.runtime.event_bus",
                "core:module.gameplay.location_service",
            },
        },
    },
}
