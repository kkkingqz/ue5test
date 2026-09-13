-- CFC-05: Mandatory Registry Sealing Phase Specification
-- Verifies:
-- 1. All engine registries are sealed and report is_frozen() == true.
-- 2. Late registration is uniformly rejected by every frozen registry.
-- 3. Read-only facade slots prevent slot replacement and ad hoc additions.
-- 4. Descriptor order matches contract sequence.
-- 5. Sealing validation logic correctly catches errors and false predicates.

local registry_lifecycle = require("core:module.bootstrap.registry_lifecycle")
local state_validator = require("core:module.runtime.state_validator")
local authoring_context = require("core:module.authoring.context")
local authoring_properties = require("core:module.authoring.properties")
local service_registry = require("core:module.runtime.service_registry")
local handler_registry = require("core:module.runtime.handler_registry")

return {
    registry_lifecycle_module_present = function()
        assert(registry_lifecycle ~= nil, "core:module.bootstrap.registry_lifecycle must be loadable")
        assert(type(registry_lifecycle.install) == "function", "install must be a function")
        assert(type(registry_lifecycle.seal) == "function", "seal must be a function")
        assert(type(registry_lifecycle.is_sealed) == "function", "is_sealed must be a function")
        assert(registry_lifecycle.is_sealed() == true, "registry_lifecycle must report is_sealed() == true in active session")
    end,

    descriptor_matches_contract_order = function()
        local expected = {
            "authoring",
            "services",
            "actions",
            "entity_extensions",
            "commands.validators",
            "commands.handlers",
            "events.subscribers",
            "events",
            "instances.actors",
            "instances",
            "presentation",
            "state_validator",
        }

        local desc = registry_lifecycle.descriptor
        assert(type(desc) == "table", "descriptor must be an array table")
        assert(#desc == #expected, "descriptor length mismatch: expected " .. #expected .. ", got " .. #desc)

        for i, exp in ipairs(expected) do
            assert(desc[i].facade_path == exp, "descriptor order mismatch at index " .. i .. ": expected '" .. exp .. "', got '" .. tostring(desc[i].facade_path) .. "'")
            assert(type(desc[i].seal) == "function", "entry " .. exp .. " must have seal function")
            assert(type(desc[i].is_frozen) == "function", "entry " .. exp .. " must have is_frozen predicate")
            if desc[i].path then
                assert(type(desc[i].create) == "function", "facade entry " .. exp .. " must have create function")
            end
        end
    end,

    install_derives_from_descriptor = function()
        local desc = registry_lifecycle.descriptor
        for _, entry in ipairs(desc) do
            if entry.path then
                local cur = game
                for _, seg in ipairs(entry.path) do
                    assert(type(cur) == "table", "segment '" .. seg .. "' must exist on facade for entry '" .. entry.facade_path .. "'")
                    cur = cur[seg]
                end
                assert(cur ~= nil, "registry object for '" .. entry.facade_path .. "' must be installed on game facade")
            end
        end
    end,

    all_registries_frozen_in_active_session = function()
        assert(game.services and game.services.is_frozen() == true, "game.services must be frozen")
        assert(game.actions and game.actions.is_frozen() == true, "game.actions must be frozen")
        assert(game.entity_extensions and game.entity_extensions.is_frozen() == true, "game.entity_extensions must be frozen")
        assert(game.commands and game.commands.validators and game.commands.validators.is_frozen() == true, "game.commands.validators must be frozen")
        assert(game.commands and game.commands.handlers and game.commands.handlers.is_frozen() == true, "game.commands.handlers must be frozen")
        assert(game.events and game.events.subscribers and game.events.subscribers.is_frozen() == true, "game.events.subscribers must be frozen")
        assert(game.events and game.events.is_frozen() == true, "game.events must be frozen")
        assert(game.instances and game.instances.actors and game.instances.actors.is_frozen() == true, "game.instances.actors must be frozen")
        assert(game.instances and game.instances.is_frozen() == true, "game.instances must be frozen")
        assert(game.presentation and game.presentation.is_frozen() == true, "game.presentation must be frozen")
        assert(state_validator.is_frozen() == true, "state_validator must be frozen")
        assert(authoring_context.is_frozen() == true, "authoring_context must be frozen")
        assert(authoring_properties.is_frozen() == true, "authoring_properties must be frozen")
    end,

    late_registration_rejected_across_all_registries = function()
        -- Services
        local ok, err = pcall(function()
            game.services.register("core:service.test.late", { ping = function() end })
        end)
        assert(not ok and string.find(tostring(err), "ServiceRegistryFrozen"), "late service register must fail with ServiceRegistryFrozen")

        -- Actions
        ok, err = pcall(function()
            game.actions.register("core:action.test.late", { execute = function() end })
        end)
        assert(not ok and string.find(tostring(err), "ActionRegistryFrozen"), "late action register must fail with ActionRegistryFrozen")

        -- Entity extensions
        ok, err = pcall(function()
            game.entity_extensions.register("core:module.test.mod", "core", "actor", "late_method", function() end)
        end)
        assert(not ok and string.find(tostring(err), "EntityExtensionRegistryFrozen"), "late entity extension register must fail with EntityExtensionRegistryFrozen")

        -- Validators
        ok, err = pcall(function()
            game.commands.validators.register("core:validator.test.late", function() return true end)
        end)
        assert(not ok and string.find(tostring(err), "ValidatorRegistryFrozen"), "late validator register must fail with ValidatorRegistryFrozen")

        -- Handlers
        ok, err = pcall(function()
            game.commands.handlers.register("core:command.test.late", function() end)
        end)
        assert(not ok and string.find(tostring(err), "CommandHandlerRegistryFrozen"), "late handler register must fail with CommandHandlerRegistryFrozen")

        -- Subscribers
        ok, err = pcall(function()
            game.events.subscribers.register("core:subscriber.test.late", "core:event.test.step", function() end)
        end)
        assert(not ok and string.find(tostring(err), "SubscriberRegistryFrozen"), "late subscriber register must fail with SubscriberRegistryFrozen")

        -- Actors
        ok, err = pcall(function()
            game.instances.actors.register_type("test_disc", function(b) return b end)
        end)
        assert(not ok and string.find(tostring(err), "ActorTypeRegistryFrozen"), "late actor register must fail with ActorTypeRegistryFrozen")

        -- Instances
        ok, err = pcall(function()
            game.instances.register_kind("test_kind")
        end)
        assert(not ok and string.find(tostring(err), "InstanceKindRegistryFrozen"), "late instance kind register must fail with InstanceKindRegistryFrozen")

        -- Presentation source
        ok, err = pcall(function()
            game.presentation.register_source(function() return nil end)
        end)
        assert(not ok and string.find(tostring(err), "PresentationSourceRegistryFrozen"), "late presentation register must fail with PresentationSourceRegistryFrozen")

        -- State validator reference fields
        ok, err = pcall(function()
            state_validator.register_reference_field("test_ref_field", "actor")
        end)
        assert(not ok and string.find(tostring(err), "StateReferenceFieldRegistryFrozen"), "late ref field register must fail with StateReferenceFieldRegistryFrozen")

        -- Authoring properties
        ok, err = pcall(function()
            authoring_properties.register_definition_type("test_target", function(b) return b end)
        end)
        assert(not ok and string.find(tostring(err), "DefinitionTypeRegistryFrozen"), "late def type register must fail with DefinitionTypeRegistryFrozen")
    end,

    facade_read_only_protection = function()
        -- Attempting to overwrite existing registry slot on game
        local ok, err = pcall(function()
            game.services = {}
        end)
        assert(not ok and string.find(tostring(err), "FacadeSlotAssignmentDisallowed"),
            "overwriting game.services must fail with FacadeSlotAssignmentDisallowed, got: " .. tostring(err))

        ok, err = pcall(function()
            game.commands = {}
        end)
        assert(not ok and string.find(tostring(err), "FacadeSlotAssignmentDisallowed"),
            "overwriting game.commands must fail with FacadeSlotAssignmentDisallowed, got: " .. tostring(err))

        -- Attempting to add ad hoc slot to game facade
        ok, err = pcall(function()
            game.unauthorized_registry = {}
        end)
        assert(not ok and string.find(tostring(err), "FacadeSlotAssignmentDisallowed"),
            "adding ad hoc slot to game must fail with FacadeSlotAssignmentDisallowed, got: " .. tostring(err))

        -- Attempting to overwrite slot in sub-facade
        ok, err = pcall(function()
            game.commands.validators = {}
        end)
        assert(not ok and string.find(tostring(err), "FacadeSlotAssignmentDisallowed"),
            "overwriting game.commands.validators must fail with FacadeSlotAssignmentDisallowed, got: " .. tostring(err))

        ok, err = pcall(function()
            game.events.subscribers = {}
        end)
        assert(not ok and string.find(tostring(err), "FacadeSlotAssignmentDisallowed"),
            "overwriting game.events.subscribers must fail with FacadeSlotAssignmentDisallowed, got: " .. tostring(err))

        ok, err = pcall(function()
            game.instances.actors = {}
        end)
        assert(not ok and string.find(tostring(err), "FacadeSlotAssignmentDisallowed"),
            "overwriting game.instances.actors must fail with FacadeSlotAssignmentDisallowed, got: " .. tostring(err))

        -- Attempting to add ad hoc property to sub-facade
        ok, err = pcall(function()
            game.commands.extra = true
        end)
        assert(not ok and string.find(tostring(err), "FacadeSlotAssignmentDisallowed"),
            "adding property to game.commands must fail with FacadeSlotAssignmentDisallowed, got: " .. tostring(err))
    end,

    isolation_capability_is_single_use = function()
        -- B3: the isolation handle is a capability, not a getter. core:module.runtime.test_isolation
        -- took it during bootstrap, so every later caller -- including a package module that
        -- declares core:module.bootstrap.registry_lifecycle as a dependency -- gets nil and is
        -- refused by with_isolated_facade_slot.
        assert(registry_lifecycle.get_isolation_handle == nil,
            "registry_lifecycle must not expose a plain isolation-handle getter")

        local second = registry_lifecycle.take_isolation_handle()
        assert(second == nil, "the isolation handle must be takeable only once, got: " .. tostring(second))

        local ok, err = pcall(function()
            registry_lifecycle.with_isolated_facade_slot(second, game, "services", {}, function() end)
        end)
        assert(not ok and string.find(tostring(err), "UnauthorizedIsolation"),
            "a caller without the handle must be refused with UnauthorizedIsolation, got: " .. tostring(err))

        -- The sealed registry is still the one every reader sees.
        assert(game.services.is_frozen() == true, "game.services must remain frozen after the refused isolation attempt")
    end,

    facade_rawset_protection_and_package_isolation = function()
        -- 1. Attempting rawset directly into game facade must fail
        local ok, err = pcall(function()
            rawset(game, "services", {})
        end)
        assert(not ok and string.find(tostring(err), "FacadeSlotAssignmentDisallowed"),
            "rawset on game.services must fail with FacadeSlotAssignmentDisallowed, got: " .. tostring(err))

        ok, err = pcall(function()
            rawset(game, "unauthorized_slot", {})
        end)
        assert(not ok and string.find(tostring(err), "FacadeSlotAssignmentDisallowed"),
            "rawset of ad hoc slot on game must fail with FacadeSlotAssignmentDisallowed, got: " .. tostring(err))

        ok, err = pcall(function()
            rawset(_G.game, "actions", {})
        end)
        assert(not ok and string.find(tostring(err), "FacadeSlotAssignmentDisallowed"),
            "rawset on _G.game.actions must fail with FacadeSlotAssignmentDisallowed, got: " .. tostring(err))

        -- 2. Attempting rawset into protected sub-facades must fail
        ok, err = pcall(function()
            rawset(game.commands, "handlers", {})
        end)
        assert(not ok and string.find(tostring(err), "FacadeSlotAssignmentDisallowed"),
            "rawset on game.commands.handlers must fail with FacadeSlotAssignmentDisallowed, got: " .. tostring(err))

        ok, err = pcall(function()
            rawset(game.events, "subscribers", {})
        end)
        assert(not ok and string.find(tostring(err), "FacadeSlotAssignmentDisallowed"),
            "rawset on game.events.subscribers must fail with FacadeSlotAssignmentDisallowed, got: " .. tostring(err))

        ok, err = pcall(function()
            rawset(game.instances, "actors", {})
        end)
        assert(not ok and string.find(tostring(err), "FacadeSlotAssignmentDisallowed"),
            "rawset on game.instances.actors must fail with FacadeSlotAssignmentDisallowed, got: " .. tostring(err))

        -- 3. Verify production modules do not export with_isolated_* helpers to live VM
        assert(service_registry.with_isolated_services == nil,
            "service_registry must not export with_isolated_services in production")
        assert(handler_registry.with_isolated_handlers == nil,
            "handler_registry must not export with_isolated_handlers in production")
        assert(authoring_context.with_isolated_validators == nil,
            "authoring_context must not export with_isolated_validators in production")
        assert(authoring_context.with_isolated_context == nil,
            "authoring_context must not export with_isolated_context in production")

        -- 4. Verify unauthorized code cannot call registry_lifecycle.with_isolated_facade_slot without valid handle
        ok, err = pcall(function()
            registry_lifecycle.with_isolated_facade_slot(nil, game, "services", {}, function() end)
        end)
        assert(not ok and string.find(tostring(err), "UnauthorizedIsolation"),
            "calling with_isolated_facade_slot with nil handle must fail with UnauthorizedIsolation, got: " .. tostring(err))

        ok, err = pcall(function()
            registry_lifecycle.with_isolated_facade_slot({}, game, "services", {}, function() end)
        end)
        assert(not ok and string.find(tostring(err), "UnauthorizedIsolation"),
            "calling with_isolated_facade_slot with forged handle must fail with UnauthorizedIsolation, got: " .. tostring(err))

        -- 5. Ordinary rawset on non-facade tables must work without hindrance
        local regular_tbl = {}
        rawset(regular_tbl, "test_key", 42)
        assert(regular_tbl.test_key == 42, "rawset must work normally on non-facade tables")
    end,
}
