-- RAS-01..04, RAS-13..15: Field Contracts and Descriptors Specification (ADR-0032)
-- Verifies core:module.authoring.field descriptors, integration with authoring prototypes,
-- schema registration, schema composition, duplicate rejection, and write-time validation on ActorWrapper.

local field = require("core:module.authoring.field")
local authoring_context = require("core:module.authoring.context")
local properties = require("core:module.authoring.properties")
local actor_registry = require("core:module.runtime.actor_registry")

return {
    field_descriptor_factories = function()
        local non_neg = field.non_negative_integer()
        assert(non_neg.__gv2_field_descriptor == true)
        assert(non_neg.kind == "integer")
        assert(non_neg.min == 0)
        assert(non_neg.storage == "runtime_state")
        assert(non_neg.write_policy == "plain")
        assert(non_neg.override == false)

        local non_neg_override = field.non_negative_integer({ override = true })
        assert(non_neg_override.override == true)

        local pos_int = field.positive_integer()
        assert(pos_int.kind == "integer")
        assert(pos_int.min == 1)

        local int_f = field.integer({ min = -10, max = 100 })
        assert(int_f.kind == "integer")
        assert(int_f.min == -10)
        assert(int_f.max == 100)

        local num_f = field.number({ min = 0.5 })
        assert(num_f.kind == "number")
        assert(num_f.min == 0.5)

        local str_f = field.string({ min_length = 3, max_length = 20 })
        assert(str_f.kind == "string")
        assert(str_f.min_length == 3)
        assert(str_f.max_length == 20)

        local bool_f = field.boolean()
        assert(bool_f.kind == "boolean")

        local enum_f = field.enum({ "easy", "normal", "hard" })
        assert(enum_f.kind == "enum")
        assert(#enum_f.values == 3)

        local ref_def = field.ref_definition("location")
        assert(ref_def.kind == "ref_definition")
        assert(ref_def.target_kind == "location")

        local ref_inst = field.ref_instance("actor")
        assert(ref_inst.kind == "ref_instance")
        assert(ref_inst.target_kind == "actor")
    end,

    prototype_field_declaration_registers_schema = function()
        properties.with_isolated_state(function()
            local mod, env = authoring_context.create_authoring_environment("test_pkg", "test_pkg:authoring.test")

            -- In authoring env, field is available and assigning to Actor registers schema
            assert(env.field ~= nil, "field must be available in authoring env")
            assert(type(env.field.non_negative_integer) == "function")

            env.Actor.test_gold = env.field.non_negative_integer()
            env.Actor.test_stamina = env.field.non_negative_integer()

            local schema = properties.get_schema("Actor")
            assert(schema ~= nil, "Schema for Actor must be registered")
            assert(schema.fields.test_gold ~= nil, "test_gold must be in schema")
            assert(schema.fields.test_gold.kind == "integer")
            assert(schema.fields.test_gold.min == 0)
            assert(schema.fields.test_stamina ~= nil, "test_stamina must be in schema")
            assert(schema.fields.test_stamina.kind == "integer")
            assert(schema.fields.test_stamina.min == 0)
        end)
    end,

    duplicate_field_declaration_requires_override = function()
        properties.with_isolated_state(function()
            local mod, env = authoring_context.create_authoring_environment("test_pkg", "test_pkg:authoring.test")

            env.Actor.health = env.field.non_negative_integer()

            -- Duplicate declaration without override must fail (RAS-14)
            local ok, err = pcall(function()
                env.Actor.health = env.field.non_negative_integer()
            end)
            assert(not ok, "Duplicate field declaration without override must fail")
            assert(string.find(tostring(err), "FieldAlreadyDeclared") ~= nil,
                "Error must be FieldAlreadyDeclared, got: " .. tostring(err))

            -- Duplicate declaration with override = true must succeed
            local ok_override = pcall(function()
                env.Actor.health = env.field.positive_integer({ override = true })
            end)
            assert(ok_override, "Field override must succeed when override = true")

            local schema = properties.get_schema("Actor")
            assert(schema.fields.health.min == 1, "Overridden field should have min = 1")
        end)
    end,

    schema_composition_generic_and_discriminator = function()
        properties.with_isolated_state(function()
            local mod, env = authoring_context.create_authoring_environment("test_pkg", "test_pkg:authoring.test")

            -- Declare base field on generic Actor
            env.Actor.gold = env.field.non_negative_integer()

            -- Declare discriminator-specific schema for "character"
            properties.register_schema("character", {
                fields = {
                    level = env.field.positive_integer(),
                },
            })

            -- Effective schema for character must contain BOTH gold and level (RAS-13)
            local eff = properties.get_effective_schema("character", nil, "Actor")
            assert(eff ~= nil, "Effective schema must be resolved")
            assert(eff.fields.gold ~= nil, "gold must be present from generic Actor")
            assert(eff.fields.gold.min == 0)
            assert(eff.fields.level ~= nil, "level must be present from character discriminator")
            assert(eff.fields.level.min == 1)

            -- ActorWrapper for character must validate both fields
            local mock_state = {
                instance_id = "character@1",
                discriminator = "character",
            }
            local wrapper = actor_registry.wrap(mock_state)

            wrapper.gold = 50
            assert(mock_state.gold == 50)
            wrapper.level = 5
            assert(mock_state.level == 5)

            -- Invalid writes to either field must fail
            local ok1, err1 = pcall(function() wrapper.gold = -1 end)
            assert(not ok1, "Writing negative gold must fail")
            assert(string.find(tostring(err1), "FieldValidationConstraintFailed") ~= nil)

            local ok2, err2 = pcall(function() wrapper.level = 0 end)
            assert(not ok2, "Writing 0 to positive_integer level must fail")
            assert(string.find(tostring(err2), "FieldValidationConstraintFailed") ~= nil)
        end)
    end,

    schema_composition_narrows_constraints = function()
        properties.with_isolated_state(function()
            local mod, env = authoring_context.create_authoring_environment("test_pkg", "test_pkg:authoring.test")

            -- Generic Actor allows mana 0..100
            env.Actor.mana = env.field.integer({ min = 0, max = 100 })

            -- Discriminator "mage" narrows mana to 20..80
            properties.register_schema("mage", {
                fields = {
                    mana = env.field.integer({ min = 20, max = 80, override = true }),
                },
            })

            local eff = properties.get_effective_schema("mage", nil, "Actor")
            assert(eff.fields.mana.min == 20)
            assert(eff.fields.mana.max == 80)
        end)
    end,

    schema_composition_rejects_kind_mismatch = function()
        properties.with_isolated_state(function()
            local mod, env = authoring_context.create_authoring_environment("test_pkg", "test_pkg:authoring.test")

            env.Actor.data_ref = env.field.string()

            properties.register_schema("special_actor", {
                fields = {
                    data_ref = env.field.integer({ override = true }),
                },
            })

            local ok, err = pcall(function()
                properties.get_effective_schema("special_actor", nil, "Actor")
            end)
            assert(not ok, "Overriding field with different kind must throw error")
            assert(string.find(tostring(err), "InvalidFieldOverrideKindMismatch") ~= nil,
                "Error must be InvalidFieldOverrideKindMismatch, got: " .. tostring(err))
        end)
    end,

    unknown_discriminator_rejected_despite_generic_actor_schema = function()
        properties.with_isolated_state(function()
            local mod, env = authoring_context.create_authoring_environment("test_pkg", "test_pkg:authoring.test")

            -- Declare field on Actor
            env.Actor.gold = env.field.non_negative_integer()

            -- Wrapping an actor with an unregistered discriminator must FAIL (RAS-15)
            local mock_unknown = {
                instance_id = "unknown@1",
                discriminator = "typo_actor_type",
            }
            local ok, err = pcall(function()
                actor_registry.wrap(mock_unknown)
            end)
            assert(not ok, "Wrapping unknown discriminator must fail despite generic Actor schema")
            assert(string.find(tostring(err), "ActorTypeNotRegistered") ~= nil,
                "Error must be ActorTypeNotRegistered, got: " .. tostring(err))
        end)
    end,

    write_validation_on_actor_wrapper = function()
        properties.with_isolated_state(function()
            local mod, env = authoring_context.create_authoring_environment("test_pkg", "test_pkg:authoring.test")
            env.Actor.mana = env.field.non_negative_integer()
            env.Actor.title = env.field.string({ min_length = 2 })

            local mock_state = {
                instance_id = "character@1",
                discriminator = "character",
                definition_id = "sample:actor.character.hero",
                mana = 50,
            }

            -- Wrap actor using actor_registry
            local wrapper = actor_registry.wrap(mock_state)

            -- Valid writes
            wrapper.mana = 100
            assert(mock_state.mana == 100)

            wrapper.mana = 0
            assert(mock_state.mana == 0)

            wrapper.title = "Champion"
            assert(mock_state.title == "Champion")

            -- Invalid writes: negative integer
            local ok1, err1 = pcall(function()
                wrapper.mana = -5
            end)
            assert(not ok1, "Writing negative integer to non_negative_integer must fail")
            assert(string.find(tostring(err1), "FieldValidationConstraintFailed") ~= nil)

            -- Invalid writes: float
            local ok2, err2 = pcall(function()
                wrapper.mana = 3.14
            end)
            assert(not ok2, "Writing float to integer must fail")
            assert(string.find(tostring(err2), "FieldValidationTypeMismatch") ~= nil)

            -- Invalid writes: string to integer
            local ok3, err3 = pcall(function()
                wrapper.mana = "100"
            end)
            assert(not ok3, "Writing string to integer must fail")
            assert(string.find(tostring(err3), "FieldValidationTypeMismatch") ~= nil)

            -- Invalid writes: too short string
            local ok4, err4 = pcall(function()
                wrapper.title = "A"
            end)
            assert(not ok4, "Writing short string must fail constraint")
            assert(string.find(tostring(err4), "FieldValidationConstraintFailed") ~= nil)
        end)
    end,
}
