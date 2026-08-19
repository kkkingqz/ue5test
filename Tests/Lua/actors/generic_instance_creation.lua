-- RAS-05..07, RAS-16: Generic Instance Creation and Kind Registry Specification (ADR-0032)
-- Verifies dynamic instance kind registration, section name derivation, instances.create facade,
-- definition/owner normalization, repository validation, and authoring _ENV integration.

local instance_registry = require("core:module.runtime.instance_registry")
local authoring_context = require("core:module.authoring.context")
local mutation_window = require("core:module.runtime.mutation_window")
local actor_registry = require("core:module.runtime.actor_registry")

local function setup_mock_game(kinds)
    local mock_state = {
        meta = {
            instance_counters = {},
        },
        actors = {},
    }
    local mock_repo = {
        ["rh:item.misc.herb"] = { id = "rh:item.misc.herb", kind = "item", data = { name = "Herb", price = 5 } },
        ["rh:item.weapon.iron_sword"] = { id = "rh:item.weapon.iron_sword", kind = "item", data = { name = "Sword", price = 20 } },
    }
    local reg = instance_registry.create_registry()
    if kinds then
        for _, k in ipairs(kinds) do
            reg.register_kind(k)
        end
    end

    local actors_reg = actor_registry.create_registry()

    _G.game = {
        state = mock_state,
        repository = {
            get = function(id) return mock_repo[id] end,
            exists = function(id) return mock_repo[id] ~= nil end,
        },
        instances = {
            register_kind = reg.register_kind,
            is_registered_kind = reg.is_registered_kind,
            get_section_name = reg.get_section_name,
            create = reg.create,
            freeze = reg.freeze,
            kinds = reg.kinds,
            actors = actors_reg,
        },
    }
    return _G.game, reg
end

local function cleanup_mock_game()
    _G.game = nil
end

return {
    instance_kind_registry_derives_section_name = function()
        local g, reg = setup_mock_game()
        reg.register_kind("item")
        reg.register_kind("quest")
        reg.register_kind("vehicle", { section_name = "custom_vehicles" })

        assert(reg.is_registered_kind("item") == true)
        assert(reg.is_registered_kind("quest") == true)
        assert(reg.is_registered_kind("vehicle") == true)
        assert(reg.is_registered_kind("actor") == true, "actor must always be recognized")
        assert(reg.is_registered_kind("unknown_kind") == false)

        assert(reg.get_section_name("item") == "item_instances")
        assert(reg.get_section_name("quest") == "quest_instances")
        assert(reg.get_section_name("vehicle") == "custom_vehicles")
        assert(reg.get_section_name("actor") == "actors")

        cleanup_mock_game()
    end,

    instance_kind_registry_freeze_rejects_late_registration = function()
        local g, reg = setup_mock_game({ "item" })
        reg.freeze()
        assert(reg.is_frozen() == true)

        local ok, err = pcall(function()
            reg.register_kind("late_kind")
        end)
        assert(not ok, "Registering kind after freeze must fail")
        assert(string.find(tostring(err), "InstanceKindRegistryFrozen") ~= nil,
            "Error must be InstanceKindRegistryFrozen, got: " .. tostring(err))

        cleanup_mock_game()
    end,

    generic_create_rejects_unknown_kind = function()
        local g, reg = setup_mock_game({ "item" })
        mutation_window.open()

        local ok, err = pcall(function()
            g.instances.create("unknown_kind", { definition = "rh:item.misc.herb" })
        end)
        mutation_window.close()

        assert(not ok, "Creating unknown instance kind must fail")
        assert(string.find(tostring(err), "UnknownInstanceKind") ~= nil,
            "Error must be UnknownInstanceKind, got: " .. tostring(err))

        cleanup_mock_game()
    end,

    generic_create_creates_instance_in_derived_section = function()
        local g, reg = setup_mock_game({ "item" })
        mutation_window.open()

        local item_id = g.instances.create("item", {
            definition = "rh:item.misc.herb",
            owner = "character@1",
            quantity = 3,
        })
        mutation_window.close()

        assert(item_id == "item@1", "Allocated item_id should be item@1, got " .. tostring(item_id))
        assert(g.state.item_instances ~= nil, "item_instances section must exist")
        local record = g.state.item_instances[item_id]
        assert(record ~= nil, "Item record must be stored in state.item_instances")
        assert(record.instance_id == "item@1")
        assert(record.definition_id == "rh:item.misc.herb")
        assert(record.owner_id == "character@1")
        assert(record.quantity == 3)

        cleanup_mock_game()
    end,

    normalization_of_definition_and_owner_references = function()
        local g, reg = setup_mock_game({ "item" })

        -- Mock actor wrapper
        local mock_actor_wrapper = {
            instance_id = "character@42",
        }

        -- Mock definition handle
        local mock_def_handle = {
            id = "rh:item.weapon.iron_sword",
            definition_id = "rh:item.weapon.iron_sword",
        }

        mutation_window.open()
        local item_id = g.instances.create("item", {
            definition = mock_def_handle,
            owner = mock_actor_wrapper,
        })
        mutation_window.close()

        local record = g.state.item_instances[item_id]
        assert(record.definition_id == "rh:item.weapon.iron_sword", "Definition ID must be normalized from table")
        assert(record.owner_id == "character@42", "Owner ID must be normalized from actor wrapper")

        -- Non-existent definition must be rejected
        mutation_window.open()
        local ok, err = pcall(function()
            g.instances.create("item", {
                definition = "rh:item.nonexistent.fake",
            })
        end)
        mutation_window.close()

        assert(not ok, "Creating instance with non-existent definition must fail")
        assert(string.find(tostring(err), "InstanceDefinitionNotFound") ~= nil,
            "Error must be InstanceDefinitionNotFound, got: " .. tostring(err))

        cleanup_mock_game()
    end,

    authoring_environment_instances_facade_and_write_protection = function()
        local g, reg = setup_mock_game({ "item" })
        local mod, env = authoring_context.create_authoring_environment("test_pkg", "test_pkg:authoring.test")

        assert(env.instances ~= nil, "instances facade must be in authoring env")
        assert(type(env.instances.create) == "function", "instances.create must be a function")
        assert(type(env.instances.register_kind) == "function", "instances.register_kind must be a function")

        -- Direct reassignment of instances in authoring environment must be rejected
        local ok, err = pcall(function()
            env.instances = {}
        end)
        assert(not ok, "Overwriting instances in authoring env must fail")
        assert(string.find(tostring(err), "AuthoringGlobalWriteDisallowed") ~= nil)

        -- Calling instances.create from authoring env works
        mutation_window.open()
        local item_id = env.instances.create("item", {
            definition = "rh:item.misc.herb",
        })
        mutation_window.close()
        assert(item_id == "item@1")

        cleanup_mock_game()
    end,
}
