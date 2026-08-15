-- TAS-13: isolated bootstrap manifest for Tests/Lua/commands/*.lua specs.
-- Not Scripts/bootstrap/manifest.lua — this loads only the four real
-- runtime modules a command-validator test needs, plus a test-only driver
-- that registers test-scoped validators/commands during the "register"
-- phase (before the validator registry freezes). The real production
-- bootstrap cannot be reused here: by the time any spec runs, its
-- validator registry is already frozen and empty (no production validator
-- exists yet), so there would be nothing to invoke.
return {
    entry_module_id = "core:module.test.command_validator_specs_driver",
    modules = {
        {
            module_id = "core:module.runtime.mutation_window",
            source = "runtime/mutation_window.lua",
            dependencies = {},
        },
        {
            module_id = "core:module.runtime.stable_id",
            source = "runtime/stable_id.lua",
            dependencies = {},
        },
        {
            module_id = "core:module.runtime.validator_registry",
            source = "runtime/validator_registry.lua",
            dependencies = { "core:module.runtime.stable_id" },
        },
        {
            module_id = "core:module.runtime.event_envelope",
            source = "runtime/event_envelope.lua",
            dependencies = { "core:module.runtime.stable_id" },
        },
        {
            module_id = "core:module.runtime.subscriber_registry",
            source = "runtime/subscriber_registry.lua",
            dependencies = { "core:module.runtime.stable_id" },
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
            module_id = "core:module.runtime.command_dispatcher",
            source = "runtime/command_dispatcher.lua",
            dependencies = {
                "core:module.runtime.mutation_window",
                "core:module.runtime.stable_id",
                "core:module.runtime.event_bus",
            },
        },
        {
            module_id = "core:module.test.command_validator_specs_driver",
            source = "test/command_validator_specs_driver.lua",
            dependencies = {
                "core:module.runtime.command_dispatcher",
                "core:module.runtime.validator_registry",
                "core:module.runtime.event_bus",
            },
        },
    },
}
