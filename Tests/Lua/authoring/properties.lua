-- DLA-10, DLA-11, DLA-12, DLA-13: Designer Properties Specification (ADR-0027, Properties.md)
-- Verifies:
--   1. Schema storage and write_policy attributes
--   2. Authoring property wrapper: reading definition fields works, writing is rejected without state change or revision increment
--   3. Plain fields schema validation before state write (bounds, enum, types)
--   4. Collection wrappers with per-element validation
--   5. Managed fields: direct assignment rejected with declared domain operations hint
--   6. Freeze phase verification of declared operations on managed fields (MissingDomainOperation)
--   7. Two reference types: ref_definition (resolves definition table) and ref_instance (resolves fresh disposable ActorWrapper)
--   8. Registry-oriented referential integrity on actor removal (ActorHasDependentReferences)

local mutation_window = require("core:module.runtime.mutation_window")
local actor_registry = require("core:module.runtime.actor_registry")
local properties = require("core:module.authoring.properties")
local handler_registry = require("core:module.runtime.handler_registry")

local function run_with_mock_environment(fn)
    local prev_game = _G.game
    _G.game = {
        state = {
            meta = {
                next_instance_id = 10,
                player_actor_id = nil,
            },
            actors = {},
            item_instances = {},
            quests = {},
        },
        repository = {
            get = function(id)
                if id == "core:actor.hero" then
                    return {
                        id = "core:actor.hero",
                        discriminator = "hero_type",
                        data = {
                            base_speed = 300,
                            archetype = "warrior",
                            faction_id = "core:faction.heroes",
                        },
                    }
                elseif id == "core:actor.npc" then
                    return {
                        id = "core:actor.npc",
                        discriminator = "npc_type",
                        data = {
                            base_speed = 100,
                            archetype = "peasant",
                        },
                    }
                elseif id == "core:faction.heroes" then
                    return {
                        id = "core:faction.heroes",
                        data = { name = "Heroes Guild" },
                    }
                elseif id == "core:item.iron_sword" then
                    return {
                        id = "core:item.iron_sword",
                        data = { name = "Iron Sword" },
                    }
                end
                return nil
            end,
            exists = function(id)
                return id == "core:actor.hero" or id == "core:actor.npc" or id == "core:faction.heroes" or id == "core:item.iron_sword"
            end,
        },
    }

    local registry = actor_registry.create_registry()
    _G.game.instances = {
        actors = registry,
    }

    local ok, err = pcall(function()
        properties.with_isolated_state(function()
            fn(registry)
        end)
    end)
    _G.game = prev_game
    if not ok then
        error(err)
    end
end

return {
    definition_field_read_succeeds_and_write_is_rejected = function()
        run_with_mock_environment(function(registry)
            properties.register_schema("hero_type", {
                fields = {
                    base_speed = { kind = "int64", storage = "definition", write_policy = "read_only" },
                    faction_id = { kind = "ref_definition", target_kind = "faction", storage = "definition", write_policy = "read_only" },
                    morale = { kind = "int64", storage = "runtime_state", write_policy = "plain", min = 0, max = 100 },
                },
            })

            registry.register_type("hero_type", function(base)
                return {
                    get_morale = function() return base.morale end,
                }
            end)

            local hero = mutation_window.execute_in_window(function()
                return registry.create("core:actor.hero", { morale = 50 })
            end)

            assert(hero ~= nil, "Hero creation failed")
            assert(hero.base_speed == 300, "Definition field read failed")
            assert(hero.morale == 50, "Runtime plain field read failed")

            local rev_before = mutation_window.write_revision()
            local write_def_err = false
            mutation_window.execute_in_window(function()
                local ok, err = pcall(function()
                    hero.base_speed = 400
                end)
                if not ok then
                    write_def_err = true
                    assert(string.find(tostring(err), "Cannot modify definition field"), "Expected Cannot modify definition field error, got: " .. tostring(err))
                end
            end)

            assert(write_def_err, "Writing to definition field must throw error")
            assert(mutation_window.write_revision() == rev_before, "Write revision must not increment on rejected definition write")
        end)
    end,

    plain_field_validates_schema_before_mutation = function()
        run_with_mock_environment(function(registry)
            properties.register_schema("hero_type", {
                fields = {
                    level = { kind = "int64", storage = "runtime_state", write_policy = "plain", min = 1, max = 99 },
                    stance = { kind = "enum", values = { "aggressive", "defensive", "neutral" }, storage = "runtime_state", write_policy = "plain" },
                },
            })

            registry.register_type("hero_type", function(base) return {} end)

            local hero = mutation_window.execute_in_window(function()
                return registry.create("core:actor.hero", { level = 1, stance = "neutral" })
            end)

            -- Valid writes
            mutation_window.execute_in_window(function()
                hero.level = 10
                hero.stance = "aggressive"
            end)
            assert(hero.level == 10)
            assert(hero.stance == "aggressive")

            -- Invalid level (exceeds max)
            local rev_before = mutation_window.write_revision()
            local err_bounds = false
            mutation_window.execute_in_window(function()
                local ok, err = pcall(function()
                    hero.level = 150
                end)
                if not ok then
                    err_bounds = true
                    assert(string.find(tostring(err), "FieldValidationConstraintFailed"), "Expected constraint failure error")
                end
            end)
            assert(err_bounds, "Invalid bounds must be rejected")
            assert(hero.level == 10, "State must not be modified on validation failure")
            assert(mutation_window.write_revision() == rev_before, "Write revision must not increment")

            -- Invalid enum
            local err_enum = false
            mutation_window.execute_in_window(function()
                local ok, err = pcall(function()
                    hero.stance = "flying"
                end)
                if not ok then
                    err_enum = true
                    assert(string.find(tostring(err), "FieldValidationEnumMismatch"), "Expected enum mismatch error")
                end
            end)
            assert(err_enum, "Invalid enum must be rejected")
            assert(hero.stance == "aggressive", "State must remain unchanged")
            assert(mutation_window.write_revision() == rev_before, "Write revision must not increment")

            -- Direct programmer write to game.state bypasses schema validation but respects mutation window
            mutation_window.execute_in_window(function()
                game.state.actors[hero.instance_id].level = 999
            end)
            assert(game.state.actors[hero.instance_id].level == 999)
        end)
    end,

    managed_field_rejects_direct_assignment_with_domain_operations_hint = function()
        run_with_mock_environment(function(registry)
            properties.register_schema("hero_type", {
                fields = {
                    gold = { kind = "int64", storage = "runtime_state", write_policy = "managed", operations = { "add_gold", "spend_gold" } },
                },
            })

            registry.register_type("hero_type", function(base)
                return {
                    add_gold = function(self, amount)
                        assert(amount > 0, "Amount must be positive")
                        base.get_state().gold = (base.get_state().gold or 0) + amount
                    end,
                    spend_gold = function(self, amount)
                        assert((base.get_state().gold or 0) >= amount, "Insufficient gold")
                        base.get_state().gold = base.get_state().gold - amount
                    end,
                }
            end)

            local hero = mutation_window.execute_in_window(function()
                return registry.create("core:actor.hero", { gold = 100 })
            end)

            assert(hero.gold == 100)

            -- Direct write to managed field must be rejected
            local rev_before = mutation_window.write_revision()
            local err_managed = false
            mutation_window.execute_in_window(function()
                local ok, err = pcall(function()
                    hero.gold = 200
                end)
                if not ok then
                    err_managed = true
                    assert(string.find(tostring(err), "Cannot assign directly to managed field 'gold'"), "Expected managed field error, got: " .. tostring(err))
                    assert(string.find(tostring(err), "add_gold, spend_gold"), "Expected operations hint in error")
                end
            end)
            assert(err_managed, "Direct assignment to managed field must fail")
            assert(mutation_window.write_revision() == rev_before)

            -- Using domain operations works
            mutation_window.execute_in_window(function()
                hero:add_gold(50)
                hero:spend_gold(30)
            end)
            assert(hero.gold == 120)
        end)
    end,

    missing_domain_operation_on_freeze_raises_error = function()
        run_with_mock_environment(function(registry)
            properties.register_schema("npc_type", {
                fields = {
                    energy = { kind = "int64", storage = "runtime_state", write_policy = "managed", operations = { "restore_energy", "drain_energy" } },
                },
            })

            -- Decorator defines only restore_energy, missing drain_energy!
            registry.register_type("npc_type", function(base)
                return {
                    restore_energy = function(self) end,
                }
            end)

            local err_freeze = false
            local ok, err = pcall(function()
                registry.freeze()
            end)
            if not ok then
                err_freeze = true
                assert(string.find(tostring(err), "MissingDomainOperation"), "Expected MissingDomainOperation error, got: " .. tostring(err))
                assert(string.find(tostring(err), "drain_energy"), "Expected missing operation name in error")
            end
            assert(err_freeze, "Freeze must fail when declared domain operation is missing on decorator")
        end)
    end,

    two_reference_types_and_collection_wrappers = function()
        run_with_mock_environment(function(registry)
            properties.register_schema("hero_type", {
                fields = {
                    guild = { kind = "ref_definition", target_kind = "faction", storage = "runtime_state", write_policy = "plain" },
                    companion = { kind = "ref_instance", target_kind = "actor", storage = "runtime_state", write_policy = "plain" },
                    inventory = { kind = "array", items = { kind = "ref_definition", target_kind = "item" }, storage = "runtime_state", write_policy = "plain" },
                },
            })
            properties.register_schema("npc_type", {
                fields = {},
            })

            registry.register_type("hero_type", function(base) return {} end)
            registry.register_type("npc_type", function(base) return {} end)

            local hero, npc = mutation_window.execute_in_window(function()
                local h = registry.create("core:actor.hero", {})
                local n = registry.create("core:actor.npc", {})
                return h, n
            end)

            mutation_window.execute_in_window(function()
                -- Assigning ref_definition with string
                hero.guild = "core:faction.heroes"
                -- Assigning ref_instance with companion wrapper
                hero.companion = npc
            end)

            -- Accessing ref_definition returns definition table
            local faction_def = hero.guild
            assert(type(faction_def) == "table" and faction_def.data and faction_def.data.name == "Heroes Guild", "ref_definition resolution failed")

            -- Accessing ref_instance returns fresh ActorWrapper
            local comp_wrapper = hero.companion
            assert(comp_wrapper ~= nil and comp_wrapper.instance_id == npc.instance_id, "ref_instance resolution failed")

            -- Collection wrapper test
            mutation_window.execute_in_window(function()
                hero.inventory.add("core:item.iron_sword")
                assert(hero.inventory.count() == 1)
                assert(hero.inventory[1] ~= nil and hero.inventory[1].id == "core:item.iron_sword")
                hero.inventory.remove("core:item.iron_sword")
                assert(hero.inventory.count() == 0)
            end)
        end)
    end,

    referential_integrity_on_actor_removal = function()
        run_with_mock_environment(function(registry)
            properties.register_schema("hero_type", {
                fields = {
                    target_enemy = { kind = "ref_instance", target_kind = "actor", storage = "runtime_state", write_policy = "plain" },
                },
            })
            properties.register_schema("npc_type", {
                fields = {},
            })

            registry.register_type("hero_type", function(base) return {} end)
            registry.register_type("npc_type", function(base) return {} end)

            local hero, npc = mutation_window.execute_in_window(function()
                local h = registry.create("core:actor.hero", {})
                local n = registry.create("core:actor.npc", {})
                h.target_enemy = n
                return h, n
            end)

            -- Attempting to remove npc while hero references it must fail
            local rev_before = mutation_window.write_revision()
            local err_ref_integrity = false
            mutation_window.execute_in_window(function()
                local ok, err = pcall(function()
                    registry.remove(npc.instance_id)
                end)
                if not ok then
                    err_ref_integrity = true
                    assert(string.find(tostring(err), "ActorHasDependentReferences"), "Expected ActorHasDependentReferences error, got: " .. tostring(err))
                    assert(string.find(tostring(err), hero.instance_id), "Expected reference holder actor in error")
                end
            end)
            assert(err_ref_integrity, "Removing referenced actor must fail")
            assert(registry.exists(npc.instance_id), "Actor must still exist")

            -- Clear reference and remove again
            mutation_window.execute_in_window(function()
                hero.target_enemy = nil
                local removed = registry.remove(npc.instance_id)
                assert(removed == true, "Actor removal should succeed after clearing reference")
            end)
            assert(not registry.exists(npc.instance_id), "Actor should be removed")
        end)
    end,
}
