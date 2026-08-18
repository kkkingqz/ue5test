return {
    entry_module_id = "core:module.bootstrap.main",
    modules = {
        {
            module_id = "core:module.runtime.stable_id",
            source = "runtime/stable_id.lua",
            dependencies = {},
            replaceable = false,
        },
        {
            module_id = "core:module.runtime.mutation_window",
            source = "runtime/mutation_window.lua",
            dependencies = {},
            replaceable = false,
        },
        {
            module_id = "core:module.runtime.portable_value",
            source = "runtime/portable_value.lua",
            dependencies = {},
            replaceable = false,
        },
        {
            module_id = "core:module.runtime.command_dispatcher",
            source = "runtime/command_dispatcher.lua",
            dependencies = {
                "core:module.runtime.mutation_window",
                "core:module.runtime.stable_id",
                "core:module.runtime.event_bus",
                "core:module.runtime.portable_value",
            },
            replaceable = false,
        },
        {
            module_id = "core:module.runtime.state_validator",
            source = "runtime/state_validator.lua",
            dependencies = {
                "core:module.runtime.stable_id",
            },
            replaceable = false,
        },
        {
            module_id = "core:module.runtime.instance_allocator",
            source = "runtime/instance_allocator.lua",
            dependencies = {},
            replaceable = false,
        },
        {
            module_id = "core:module.runtime.canonical_codec",
            source = "runtime/canonical_codec.lua",
            dependencies = {},
            replaceable = false,
        },
        {
            module_id = "core:module.runtime.state_hasher",
            source = "runtime/state_hasher.lua",
            dependencies = {
                "core:module.runtime.canonical_codec",
            },
            replaceable = false,
        },
        {
            module_id = "core:module.runtime.migrate",
            source = "runtime/migrate.lua",
            dependencies = {},
            replaceable = false,
        },
        {
            module_id = "core:module.runtime.save",
            source = "runtime/save.lua",
            dependencies = {
                "core:module.runtime.canonical_codec",
                "core:module.runtime.state_hasher",
                "core:module.runtime.migrate",
            },
            replaceable = false,
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
            replaceable = false,
        },
        {
            module_id = "core:module.runtime.actor_registry",
            source = "runtime/actor_registry.lua",
            dependencies = {
                "core:module.runtime.instance_allocator",
                "core:module.authoring.properties",
            },
            replaceable = false,
        },
        {
            module_id = "core:module.runtime.world",
            source = "runtime/world.lua",
            dependencies = {},
            replaceable = false,
        },
        {
            module_id = "core:module.presentation.screen_requests",
            source = "presentation/screen_requests.lua",
            dependencies = {
                "core:module.runtime.stable_id",
            },
            replaceable = false,
        },
        {
            module_id = "core:module.runtime.presentation_source",
            source = "runtime/presentation_source.lua",
            dependencies = {
                "core:module.presentation.screen_requests",
            },
            replaceable = false,
        },
        {
            module_id = "core:module.resources.text",
            source = "resources/text.lua",
            dependencies = {
                "core:module.runtime.stable_id",
            },
            replaceable = false,
        },
        {
            module_id = "core:module.resources.service",
            source = "resources/service.lua",
            dependencies = {
                "core:module.runtime.stable_id",
            },
            replaceable = false,
        },
        {
            module_id = "core:module.boundary.ingress",
            source = "boundary/ingress.lua",
            dependencies = {
                "core:module.runtime.command_dispatcher",
            },
            replaceable = false,
        },
        {
            module_id = "core:module.boundary.outbound",
            source = "boundary/outbound.lua",
            dependencies = {
                "core:module.presentation.screen_requests",
                "core:module.runtime.state_hasher",
            },
            replaceable = false,
        },
        {
            module_id = "core:module.boundary.entrypoints",
            source = "boundary/entrypoints.lua",
            dependencies = {
                "core:module.boundary.ingress",
                "core:module.boundary.outbound",
            },
            replaceable = false,
        },
        {
            module_id = "core:module.runtime.service_registry",
            source = "runtime/service_registry.lua",
            dependencies = {},
            replaceable = false,
        },
        {
            module_id = "core:module.runtime.validator_registry",
            source = "runtime/validator_registry.lua",
            dependencies = {
                "core:module.runtime.stable_id",
            },
            replaceable = false,
        },
        {
            module_id = "core:module.runtime.handler_registry",
            source = "runtime/handler_registry.lua",
            dependencies = {
                "core:module.runtime.stable_id",
            },
            replaceable = false,
        },
        {
            module_id = "core:module.runtime.action_registry",
            source = "runtime/action_registry.lua",
            dependencies = {
                "core:module.runtime.stable_id",
            },
            replaceable = false,
        },
        {
            module_id = "core:module.runtime.event_envelope",
            source = "runtime/event_envelope.lua",
            dependencies = {
                "core:module.runtime.stable_id",
                "core:module.runtime.portable_value",
            },
            replaceable = false,
        },
        {
            module_id = "core:module.runtime.subscriber_registry",
            source = "runtime/subscriber_registry.lua",
            dependencies = {
                "core:module.runtime.stable_id",
            },
            replaceable = false,
        },
        {
            module_id = "core:module.runtime.event_bus",
            source = "runtime/event_bus.lua",
            dependencies = {
                "core:module.runtime.event_envelope",
                "core:module.runtime.stable_id",
                "core:module.runtime.subscriber_registry",
            },
            replaceable = false,
        },
        {
            module_id = "core:module.authoring.properties",
            source = "authoring/properties.lua",
            dependencies = {
                "core:module.runtime.stable_id",
            },
            replaceable = false,
        },
        {
            module_id = "core:module.authoring.tagged_ref",
            source = "authoring/tagged_ref.lua",
            dependencies = {
                "core:module.runtime.stable_id",
                "core:module.runtime.portable_value",
            },
            replaceable = false,
        },
        {
            module_id = "core:module.authoring.commands",
            source = "authoring/commands.lua",
            dependencies = {
                "core:module.runtime.stable_id",
                "core:module.runtime.mutation_window",
                "core:module.authoring.tagged_ref",
            },
            replaceable = false,
        },
        {
            module_id = "core:module.authoring.presentation",
            source = "authoring/presentation.lua",
            dependencies = {
                "core:module.runtime.stable_id",
                "core:module.runtime.portable_value",
                "core:module.authoring.tagged_ref",
                "core:module.resources.text",
                "core:module.presentation.screen_requests",
            },
            replaceable = false,
        },
        {
            module_id = "core:module.authoring.context",
            source = "authoring/context.lua",
            dependencies = {
                "core:module.runtime.stable_id",
                "core:module.runtime.mutation_window",
                "core:module.runtime.portable_value",
                "core:module.authoring.tagged_ref",
                "core:module.authoring.commands",
                "core:module.authoring.properties",
                "core:module.authoring.presentation",
            },
            replaceable = false,
        },
        {
            module_id = "core:module.bootstrap.main",
            source = "bootstrap/main.lua",
            dependencies = {
                "core:module.boundary.entrypoints",
                "core:module.resources.service",
                "core:module.resources.text",
                "core:module.runtime.state_validator",
                "core:module.runtime.instance_allocator",
                "core:module.runtime.state_hasher",
                "core:module.runtime.migrate",
                "core:module.runtime.save",
                "core:module.runtime.load",
                "core:module.runtime.mutation_window",
                "core:module.runtime.portable_value",
                "core:module.runtime.actor_registry",
                "core:module.runtime.world",
                "core:module.runtime.service_registry",
                "core:module.runtime.action_registry",
                "core:module.runtime.validator_registry",
                "core:module.runtime.handler_registry",
                "core:module.runtime.subscriber_registry",
                "core:module.runtime.event_envelope",
                "core:module.runtime.event_bus",
                "core:module.authoring.properties",
                "core:module.authoring.tagged_ref",
                "core:module.authoring.commands",
                "core:module.authoring.presentation",
                "core:module.authoring.context",
                "core:module.runtime.presentation_source",
            },
            replaceable = false,
        },
    },
}
