-- EAE-01..04: Entity Extension Registry Specification (ADR-0031)
-- Verifies game.entity_extensions registry, method registration, conflict validation,
-- effective method table compilation, and freeze lifecycle.

local entity_extension_registry = require("core:module.runtime.entity_extension_registry")

return {
    registry_mounted_and_frozen_in_session = function()
        assert(game and game.entity_extensions, "game.entity_extensions must be mounted on game facade")
        assert(type(game.entity_extensions.register) == "function", "register must be a function")
        assert(type(game.entity_extensions.get_method) == "function", "get_method must be a function")
        assert(type(game.entity_extensions.get_effective_methods) == "function", "get_effective_methods must be a function")
        assert(type(game.entity_extensions.describe) == "function", "describe must be a function")
        assert(game.entity_extensions.is_frozen() == true, "game.entity_extensions must be frozen during active session")

        local ok, err = pcall(function()
            game.entity_extensions.register("sample:module", "sample", "Actor", "late_method", function() end)
        end)
        assert(not ok, "Registering a method after freeze must fail")
        assert(string.find(tostring(err), "EntityExtensionRegistryFrozen") ~= nil,
            "Error must contain EntityExtensionRegistryFrozen, got: " .. tostring(err))

        local ok2, err2 = pcall(function()
            game.entity_extensions.Actor = {}
        end)
        assert(not ok2, "Direct assignment to entity_extensions facade must be disallowed")
        assert(string.find(tostring(err2), "EntityExtensionRegistryDirectAssignmentDisallowed") ~= nil,
            "Error must contain EntityExtensionRegistryDirectAssignmentDisallowed, got: " .. tostring(err2))
    end,

    method_registration_and_lookup = function()
        local reg = entity_extension_registry.create_registry()
        assert(reg.is_frozen() == false, "new registry must start unfrozen")

        reg.register("textsystem:authoring.actors", "textsystem", "Actor", "is_player", function(self)
            return self.discriminator == "player"
        end)
        reg.register("textsystem:authoring.actors", "textsystem", "Actor", "is_npc", function(self)
            return self.discriminator == "npc"
        end)
        reg.register("rh:authoring.actors", "rh", "Actor", "get_gold", function(self)
            return self.gold or 0
        end)
        reg.register("textsystem:authoring.locations", "textsystem", "Location", "is_safe", function(self)
            return (self.danger_level or 0) <= 0
        end)

        assert(type(reg.get_method("Actor", "is_player")) == "function")
        assert(type(reg.get_method("Actor", "get_gold")) == "function")
        assert(type(reg.get_method("Location", "is_safe")) == "function")
        assert(reg.get_method("Actor", "unknown") == nil)
        assert(reg.get_method("UnknownKind", "is_player") == nil)

        local mock_player = { discriminator = "player", gold = 42 }
        local is_player_fn = reg.get_method("Actor", "is_player")
        local get_gold_fn = reg.get_method("Actor", "get_gold")
        assert(is_player_fn(mock_player) == true)
        assert(get_gold_fn(mock_player) == 42)

        local kinds = reg.kinds()
        assert(#kinds == 2)
        assert(kinds[1] == "Actor")
        assert(kinds[2] == "Location")
    end,

    conflict_validation_different_packages = function()
        local reg = entity_extension_registry.create_registry()
        reg.register("rh:authoring.actors", "rh", "Actor", "add_gold", function(self, amt)
            self.gold = (self.gold or 0) + amt
        end)

        local ok, err = pcall(function()
            reg.register("mod_a:authoring.actors", "mod_a", "Actor", "add_gold", function(self, amt)
                self.gold = (self.gold or 0) + amt * 2
            end)
        end)
        assert(not ok, "Duplicate method name from different package must be rejected with conflict error")
        assert(string.find(tostring(err), "entity_extension.method_conflict") ~= nil,
            "Error must contain entity_extension.method_conflict, got: " .. tostring(err))
        assert(string.find(tostring(err), "add_gold") ~= nil)
        assert(string.find(tostring(err), "Actor") ~= nil)
        assert(string.find(tostring(err), "rh") ~= nil)
        assert(string.find(tostring(err), "mod_a") ~= nil)
    end,

    conflict_validation_different_modules_same_package = function()
        local reg = entity_extension_registry.create_registry()
        reg.register("rh:authoring.actors_a", "rh", "Actor", "calculate_power", function() return 10 end)

        local ok, err = pcall(function()
            reg.register("rh:authoring.actors_b", "rh", "Actor", "calculate_power", function() return 20 end)
        end)
        assert(not ok, "Duplicate method name from different module in same package must be rejected")
        assert(string.find(tostring(err), "entity_extension.method_conflict") ~= nil,
            "Error must contain entity_extension.method_conflict, got: " .. tostring(err))
    end,

    idempotent_registration_same_module = function()
        local reg = entity_extension_registry.create_registry()
        local fn1 = function() return 1 end
        local fn2 = function() return 2 end

        reg.register("rh:authoring.actors", "rh", "Actor", "test_method", fn1)
        -- Re-registering from same module/package updates the function without throwing conflict
        reg.register("rh:authoring.actors", "rh", "Actor", "test_method", fn2)

        local active_fn = reg.get_method("Actor", "test_method")
        assert(active_fn() == 2)
    end,

    invalid_registration_arguments_rejected = function()
        local reg = entity_extension_registry.create_registry()
        local valid_fn = function() end

        local ok1, err1 = pcall(function()
            reg.register(nil, "pkg", "Actor", "m", valid_fn)
        end)
        assert(not ok1 and string.find(tostring(err1), "InvalidSourceModule") ~= nil)

        local ok2, err2 = pcall(function()
            reg.register("src", "", "Actor", "m", valid_fn)
        end)
        assert(not ok2 and string.find(tostring(err2), "InvalidPackageId") ~= nil)

        local ok3, err3 = pcall(function()
            reg.register("src", "pkg", 123, "m", valid_fn)
        end)
        assert(not ok3 and string.find(tostring(err3), "InvalidEntityKind") ~= nil)

        local ok4, err4 = pcall(function()
            reg.register("src", "pkg", "Actor", "", valid_fn)
        end)
        assert(not ok4 and string.find(tostring(err4), "InvalidMethodName") ~= nil)

        local ok5, err5 = pcall(function()
            reg.register("src", "pkg", "Actor", "m", "not_a_function")
        end)
        assert(not ok5 and string.find(tostring(err5), "InvalidMethodImplementation") ~= nil)
    end,

    describe_introspection = function()
        local reg = entity_extension_registry.create_registry()
        reg.register("textsystem:authoring.actors", "textsystem", "Actor", "is_player", function() end)
        reg.register("rh:authoring.actors", "rh", "Actor", "add_gold", function() end)
        reg.register("rh:authoring.actors", "rh", "Actor", "spend_gold", function() end)

        local desc = reg.describe("Actor")
        assert(#desc == 3)
        assert(desc[1].method == "add_gold")
        assert(desc[1].source == "rh:authoring.actors")
        assert(desc[1].package_id == "rh")

        assert(desc[2].method == "is_player")
        assert(desc[2].source == "textsystem:authoring.actors")
        assert(desc[2].package_id == "textsystem")

        assert(desc[3].method == "spend_gold")
        assert(desc[3].source == "rh:authoring.actors")
        assert(desc[3].package_id == "rh")

        local empty_desc = reg.describe("UnknownKind")
        assert(type(empty_desc) == "table" and #empty_desc == 0)
    end,

    effective_method_table_compilation_and_freezing = function()
        local reg = entity_extension_registry.create_registry()
        reg.register("pkg:mod", "pkg", "Quest", "is_active", function(self)
            return self.state == "active"
        end)

        reg.freeze()
        assert(reg.is_frozen() == true)

        -- 1. Effective methods lookup
        local effective = reg.get_effective_methods("Quest")
        assert(type(effective.is_active) == "function")
        assert(effective.is_active({ state = "active" }) == true)
        assert(effective.is_active({ state = "completed" }) == false)

        -- 2. Modifying effective method table throws error
        local ok1, err1 = pcall(function()
            effective.is_active = function() return false end
        end)
        assert(not ok1, "Modifying effective method table after freeze must fail")
        assert(string.find(tostring(err1), "EffectiveMethodTableFrozen") ~= nil,
            "Error must contain EffectiveMethodTableFrozen, got: " .. tostring(err1))

        -- 3. Unknown kind effective methods
        local unknown_effective = reg.get_effective_methods("UnknownKind")
        assert(type(unknown_effective) == "table")
        assert(unknown_effective.anything == nil)

        local ok2, err2 = pcall(function()
            unknown_effective.test = function() end
        end)
        assert(not ok2, "Modifying unknown kind effective table after freeze must fail")
        assert(string.find(tostring(err2), "EffectiveMethodTableFrozen") ~= nil)
    end,

    test_isolation_helper = function()
        local reg = entity_extension_registry.create_registry()
        reg.register("base:mod", "base", "Actor", "base_method", function() return 100 end)

        reg.with_isolated_extensions(function()
            assert(type(reg.get_method("Actor", "base_method")) == "function")
            reg.register("test:mod", "test", "Actor", "test_only", function() return 200 end)
            assert(type(reg.get_method("Actor", "test_only")) == "function")
        end)

        -- After isolated block, base_method remains and test_only is gone
        assert(type(reg.get_method("Actor", "base_method")) == "function")
        assert(reg.get_method("Actor", "test_only") == nil)
    end,
}
