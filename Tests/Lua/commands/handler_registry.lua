-- CHR-03: Command Handler Registry test specs.
-- Tests creation, registration, override semantics, failure cases,
-- deterministic ID ordering, lifecycle freeze, and public facade protections.

local handler_reg_mod = require("core:module.runtime.handler_registry")

return {
    successful_registration_and_lookup = function()
        local registry = handler_reg_mod.create_registry()
        local fn = function() return true end

        local registered = registry.register("core:command.test.alpha", fn)
        assert(registered == fn, "register must return the handler function")
        assert(registry.get("core:command.test.alpha") == fn, "get must return registered handler")
        assert(registry.exists("core:command.test.alpha") == true, "exists must return true for registered handler")
        assert(registry["core:command.test.alpha"] == fn, "facade indexing must return registered handler")
    end,

    lookup_nonexistent_returns_nil_and_false = function()
        local registry = handler_reg_mod.create_registry()
        assert(registry.get("core:command.test.absent") == nil, "get must return nil for missing command")
        assert(registry.exists("core:command.test.absent") == false, "exists must return false for missing command")
        assert(registry["core:command.test.absent"] == nil, "facade index must return nil for missing command")
    end,

    invalid_command_id_rejected = function()
        local registry = handler_reg_mod.create_registry()
        local dummy = function() end

        local bad_ids = {
            123,
            {},
            "",
            "not_a_stable_id",
            "core:item.weapon.iron_sword",
            "core:validator.location.travel",
            "core:screen.main",
            ":command.foo",
        }

        for _, bad_id in ipairs(bad_ids) do
            local ok, err = pcall(function()
                registry.register(bad_id, dummy)
            end)
            assert(not ok, "register must reject invalid command id: " .. tostring(bad_id))
            assert(tostring(err):find("InvalidCommandId") ~= nil,
                "must raise InvalidCommandId, got: " .. tostring(err))
        end
    end,

    non_function_handler_rejected = function()
        local registry = handler_reg_mod.create_registry()
        local bad_handlers = {
            123,
            "string_handler",
            {},
            true,
        }

        for _, bad_h in ipairs(bad_handlers) do
            local ok, err = pcall(function()
                registry.register("core:command.test.dummy", bad_h)
            end)
            assert(not ok, "register must reject non-function handler: " .. tostring(bad_h))
            assert(tostring(err):find("InvalidCommandHandler") ~= nil,
                "must raise InvalidCommandHandler, got: " .. tostring(err))
        end
    end,

    duplicate_registration_without_override_rejected = function()
        local registry = handler_reg_mod.create_registry()
        local fn1 = function() return 1 end
        local fn2 = function() return 2 end

        registry.register("core:command.test.dup", fn1)

        local ok, err = pcall(function()
            registry.register("core:command.test.dup", fn2)
        end)
        assert(not ok, "register must reject duplicate registration without override")
        assert(tostring(err):find("CommandHandlerDuplicateRegistration") ~= nil,
            "must raise CommandHandlerDuplicateRegistration, got: " .. tostring(err))
        assert(registry.get("core:command.test.dup") == fn1, "previous handler must be preserved")
    end,

    override_replaces_existing_handler = function()
        local registry = handler_reg_mod.create_registry()
        local fn1 = function() return 1 end
        local fn2 = function() return 2 end

        registry.register("core:command.test.override_target", fn1)
        assert(registry.get("core:command.test.override_target") == fn1)

        registry.register("core:command.test.override_target", fn2, { override = true })
        assert(registry.get("core:command.test.override_target") == fn2, "override must replace previous handler")
    end,

    override_unregistered_command_rejected = function()
        local registry = handler_reg_mod.create_registry()
        local fn = function() return true end

        local ok, err = pcall(function()
            registry.register("core:command.test.never_registered", fn, { override = true })
        end)
        assert(not ok, "register must reject override on missing command")
        assert(tostring(err):find("CommandHandlerOverrideMissing") ~= nil,
            "must raise CommandHandlerOverrideMissing, got: " .. tostring(err))
    end,

    invalid_options_rejected = function()
        local registry = handler_reg_mod.create_registry()
        local fn = function() end

        local ok1, err1 = pcall(function()
            registry.register("core:command.test.bad_opt1", fn, "invalid_options")
        end)
        assert(not ok1 and tostring(err1):find("InvalidCommandHandlerOptions") ~= nil)

        local ok2, err2 = pcall(function()
            registry.register("core:command.test.bad_opt2", fn, { override = "not_a_bool" })
        end)
        assert(not ok2 and tostring(err2):find("InvalidCommandHandlerOptions") ~= nil)
    end,

    registration_after_freeze_rejected = function()
        local registry = handler_reg_mod.create_registry()
        local fn = function() end

        registry.register("core:command.test.early", fn)
        assert(registry.is_frozen() == false)

        registry.freeze()
        assert(registry.is_frozen() == true)

        -- Freeze is idempotent
        registry.freeze()
        assert(registry.is_frozen() == true)

        local ok, err = pcall(function()
            registry.register("core:command.test.late", fn)
        end)
        assert(not ok, "register must reject registration after freeze")
        assert(tostring(err):find("CommandHandlerRegistryFrozen") ~= nil,
            "must raise CommandHandlerRegistryFrozen, got: " .. tostring(err))
    end,

    ids_returns_deterministic_sequence = function()
        local registry = handler_reg_mod.create_registry()
        registry.register("core:command.test.first", function() end)
        registry.register("core:command.test.second", function() end)
        registry.register("core:command.test.third", function() end)

        local ids_before = registry.ids()
        assert(#ids_before == 3)
        assert(ids_before[1] == "core:command.test.first")
        assert(ids_before[2] == "core:command.test.second")
        assert(ids_before[3] == "core:command.test.third")

        registry.freeze()

        local ids_after = registry.ids()
        assert(#ids_after == 3)
        assert(ids_after[1] == "core:command.test.first")
        assert(ids_after[2] == "core:command.test.second")
        assert(ids_after[3] == "core:command.test.third")
    end,

    direct_assignment_disallowed = function()
        local registry = handler_reg_mod.create_registry()
        local ok, err = pcall(function()
            registry["core:command.test.direct"] = function() end
        end)
        assert(not ok, "direct assignment via facade must throw")
        assert(tostring(err):find("CommandHandlerRegistryDirectAssignmentDisallowed") ~= nil,
            "must raise CommandHandlerRegistryDirectAssignmentDisallowed, got: " .. tostring(err))
    end,

    to_string_returns_canonical_type_name = function()
        local registry = handler_reg_mod.create_registry()
        assert(tostring(registry) == "GameplayCommandHandlerRegistry")
    end,
}
