-- LSM-02: Lifecycle Phases & Registry Freeze Specification (ADR-0024, LuaRuntimeContract.md)
-- Verifies that runtime registries (validators, subscribers, services) are frozen
-- after initialization, that runtime lifecycle phase transitions are tracked,
-- and that the Lua sandbox prevents access to unsafe functions.

return {
    validator_registry_frozen_after_init = function()
        assert(game and game.commands and game.commands.validators, "game.commands.validators must exist")
        assert(game.commands.validators.is_frozen(), "validator registry must be frozen during active session")

        local ok, err = pcall(function()
            game.commands.validators.register(
                "core:validator.test.late_registration",
                function(_ctx) return true end
            )
        end)
        assert(not ok, "Registering a validator after registry freeze must fail")
        assert(string.find(tostring(err), "ValidatorRegistryFrozen") or string.find(tostring(err), "frozen"),
            "Error must indicate ValidatorRegistryFrozen, got: " .. tostring(err))
    end,

    subscriber_registry_frozen_after_init = function()
        assert(game and game.events and game.events.subscribers, "game.events.subscribers must exist")
        assert(game.events.subscribers.is_frozen(), "subscriber registry must be frozen during active session")

        local ok, err = pcall(function()
            game.events.subscribers.register(
                "core:subscriber.test.late_registration",
                "core:event.test.step",
                function(_env) end
            )
        end)
        assert(not ok, "Registering a subscriber after registry freeze must fail")
        assert(string.find(tostring(err), "SubscriberRegistryFrozen") or string.find(tostring(err), "frozen"),
            "Error must indicate SubscriberRegistryFrozen, got: " .. tostring(err))
    end,

    service_registry_frozen_after_init = function()
        assert(game and game.services, "game.services must exist")
        assert(game.services.is_frozen(), "service registry must be frozen during active session")

        local ok, err = pcall(function()
            game.services.register(
                "core:service.test.late_service",
                { dummy_method = function() end }
            )
        end)
        assert(not ok, "Registering a gameplay service after registry freeze must fail")
        assert(string.find(tostring(err), "ServiceRegistryFrozen") or string.find(tostring(err), "frozen"),
            "Error must indicate ServiceRegistryFrozen, got: " .. tostring(err))
    end,

    service_registry_lookup_and_require = function()
        assert(game and game.services, "game.services must exist")
        assert(game.services.exists("core:service.location"), "core:service.location must be registered in session")

        local loc_service = game.services.get("core:service.location")
        assert(type(loc_service) == "table", "game.services.get must return service table")
        assert(type(loc_service.travel) == "function", "location service must expose travel method")

        local required_service = game.services.require("core:service.location")
        assert(required_service == loc_service, "game.services.require must return identical service instance")

        local missing = game.services.get("core:service.nonexistent")
        assert(missing == nil, "game.services.get for missing service must return nil")

        local ok, err = pcall(function()
            game.services.require("core:service.nonexistent")
        end)
        assert(not ok, "game.services.require for missing service must throw error")
        assert(string.find(tostring(err), "ServiceNotFound"), "Error must indicate ServiceNotFound, got: " .. tostring(err))
    end,

    sandbox_removes_unsafe_primitives = function()
        assert(load == nil, "global 'load' must be deleted in Lua sandbox")
        assert(loadfile == nil, "global 'loadfile' must be deleted in Lua sandbox")
        assert(dofile == nil, "global 'dofile' must be deleted in Lua sandbox")
        assert(math.random == nil, "global 'math.random' must be deleted in Lua sandbox")
    end,

    runtime_phase_and_command_tracking = function()
        assert(game and game.runtime, "game.runtime must exist")
        assert(game.runtime.phase == "idle", "runtime phase must be idle between dispatches")
        assert(game.runtime.command_count == nil or type(game.runtime.command_count) == "number",
            "game.runtime.command_count must be nil or a number")
    end,
}
