-- CFC-05A: Canonical State Composition Specification
-- Verifies:
-- 1. Valid module contributions to canonical sections.
-- 2. Mod namespace isolation (module can only contribute to its namespace or module ID in mods).
-- 3. Nested table merging in meta (instance_counters, prng, time).
-- 4. Collision exemptions (meta.schema_version, save_version, save_id).
-- 5. Collision rejection (duplicate keys across modules in sections).
-- 6. Rejection of invalid types (non-string keys, unknown sections, non-table sections, metatables).
-- 7. Hook execution lifecycle handling (invalid hook, hook runtime error, non-table return).
-- 8. Non-pollution on failure.

local state_composition = require("core:module.runtime.state_composition")
local state_validator = require("core:module.runtime.state_validator")

return {
    state_composition_module_present = function()
        assert(state_composition ~= nil, "core:module.runtime.state_composition must be loadable")
        assert(type(state_composition.compose_default_state) == "function", "compose_default_state must be a function")
        assert(type(state_composition.merge_contribution) == "function", "merge_contribution must be a function")
    end,

    valid_contributions_across_sections = function()
        local ctx = { session_generation = 1 }
        local modules = {
            {
                module_id = "test_pkg:module.player_setup",
                create_default_state = function(c)
                    assert(c.session_generation == 1)
                    return {
                        player = { level = 1, class = "warrior" },
                        world = { zone = "town" },
                    }
                end,
            },
            {
                module_id = "test_pkg:module.actor_setup",
                create_default_state = function(c)
                    return {
                        actors = { ["actor@1"] = { name = "Hero" } },
                        quests = { ["quest:main"] = { step = 0 } },
                    }
                end,
            },
        }

        local tree, fault = state_composition.compose_default_state(ctx, modules)
        assert(tree ~= nil, "compose_default_state must succeed: " .. tostring(fault and fault.message))
        assert(fault == nil, "fault must be nil on success")

        assert(tree.player.level == 1)
        assert(tree.player.class == "warrior")
        assert(tree.world.zone == "town")
        assert(tree.actors["actor@1"].name == "Hero")
        assert(tree.quests["quest:main"].step == 0)

        -- Standard empty canonical sections must be present
        assert(type(tree.meta) == "table")
        assert(type(tree.item_instances) == "table")
        assert(type(tree.mods) == "table")
        assert(type(tree.definitions) == "table")
    end,

    mod_namespace_isolation_valid = function()
        local ctx = { session_generation = 1 }
        local modules = {
            {
                module_id = "magic_mod:module.spells",
                create_default_state = function(c)
                    return {
                        mods = {
                            magic_mod = { mana_pool = 100 },
                        },
                    }
                end,
            },
            {
                module_id = "tech_mod:module.power",
                create_default_state = function(c)
                    return {
                        mods = {
                            ["tech_mod:module.power"] = { voltage = 220 },
                        },
                    }
                end,
            },
        }

        local tree, fault = state_composition.compose_default_state(ctx, modules)
        assert(tree ~= nil, "compose_default_state must succeed: " .. tostring(fault and fault.message))
        assert(tree.mods.magic_mod.mana_pool == 100)
        assert(tree.mods["tech_mod:module.power"].voltage == 220)
    end,

    mod_namespace_isolation_forbidden = function()
        local ctx = { session_generation = 1 }
        local modules = {
            {
                module_id = "stealth_mod:module.cloak",
                create_default_state = function(c)
                    return {
                        mods = {
                            forbidden_mod = { secret = 42 },
                        },
                    }
                end,
            },
        }

        local tree, fault = state_composition.compose_default_state(ctx, modules)
        assert(tree == nil, "must fail when contributing to forbidden mod namespace")
        assert(fault ~= nil, "fault must be returned")
        assert(fault.code == "LuaModuleDefaultStateInvalid", "fault code mismatch: " .. tostring(fault.code))
        assert(string.find(fault.message, "forbidden mod section"), "fault message mismatch: " .. tostring(fault.message))
    end,

    mod_section_non_string_key_rejected = function()
        local ctx = { session_generation = 1 }
        local modules = {
            {
                module_id = "stealth_mod:module.cloak",
                create_default_state = function(c)
                    return {
                        mods = {
                            [1] = { secret = 42 },
                        },
                    }
                end,
            },
        }

        local tree, fault = state_composition.compose_default_state(ctx, modules)
        assert(tree == nil, "must fail when mod key is not a string")
        assert(fault ~= nil)
        assert(fault.code == "LuaModuleDefaultStateInvalid")
        assert(string.find(fault.message, "Mod state keys must be string mod IDs"), "fault message: " .. tostring(fault.message))
    end,

    nested_meta_tables_merged = function()
        local ctx = { session_generation = 1 }
        local modules = {
            {
                module_id = "test:mod_a",
                create_default_state = function(c)
                    return {
                        meta = {
                            instance_counters = { actor = 10 },
                            prng = { combat = "seed_1" },
                            time = { ticks = 50 },
                        },
                    }
                end,
            },
            {
                module_id = "test:mod_b",
                create_default_state = function(c)
                    return {
                        meta = {
                            instance_counters = { item = 25 },
                            prng = { loot = "seed_2" },
                            time = { day = 2 },
                        },
                    }
                end,
            },
        }

        local tree, fault = state_composition.compose_default_state(ctx, modules)
        assert(tree ~= nil, "compose_default_state must succeed: " .. tostring(fault and fault.message))
        assert(tree.meta.instance_counters.actor == 10)
        assert(tree.meta.instance_counters.item == 25)
        assert(tree.meta.prng.combat == "seed_1")
        assert(tree.meta.prng.loot == "seed_2")
        assert(tree.meta.time.ticks == 50)
        assert(tree.meta.time.day == 2)
    end,

    collision_exemptions_allowed = function()
        local ctx = { session_generation = 1 }
        local modules = {
            {
                module_id = "test:mod_a",
                create_default_state = function(c)
                    return {
                        meta = {
                            schema_version = 1,
                            save_version = 1,
                            save_id = "custom_save",
                        },
                    }
                end,
            },
        }

        local tree, fault = state_composition.compose_default_state(ctx, modules)
        assert(tree ~= nil, "exempt keys must not trigger collision: " .. tostring(fault and fault.message))
        assert(tree.meta.schema_version == 1)
        assert(tree.meta.save_version == 1)
        assert(tree.meta.save_id == "custom_save")
    end,

    duplicate_key_collision_rejected = function()
        local ctx = { session_generation = 1 }
        local modules = {
            {
                module_id = "test:mod_a",
                create_default_state = function(c)
                    return {
                        player = { stamina = 100 },
                    }
                end,
            },
            {
                module_id = "test:mod_b",
                create_default_state = function(c)
                    return {
                        player = { stamina = 200 },
                    }
                end,
            },
        }

        local tree, fault = state_composition.compose_default_state(ctx, modules)
        assert(tree == nil, "must fail on duplicate key collision")
        assert(fault ~= nil)
        assert(fault.code == "LuaModuleDefaultStateInvalid")
        assert(string.find(fault.message, "Duplicate state contribution key 'stamina' in section 'player'"), "fault message: " .. tostring(fault.message))
    end,

    non_string_section_key_rejected = function()
        local target = state_validator.create_empty_canonical_state()
        local contrib = { [42] = {} }
        local ok, fault = state_composition.merge_contribution(target, contrib, "test:mod")
        assert(ok == false)
        assert(fault.code == "LuaModuleDefaultStateInvalid")
        assert(string.find(fault.message, "State contribution section keys must be strings"))
    end,

    unknown_canonical_section_rejected = function()
        local target = state_validator.create_empty_canonical_state()
        local contrib = { unknown_section_xyz = {} }
        local ok, fault = state_composition.merge_contribution(target, contrib, "test:mod")
        assert(ok == false)
        assert(fault.code == "LuaModuleDefaultStateInvalid")
        assert(string.find(fault.message, "Unknown canonical state section 'unknown_section_xyz'"))
    end,

    non_table_section_value_rejected = function()
        local target = state_validator.create_empty_canonical_state()
        local contrib = { player = "scalar_not_table" }
        local ok, fault = state_composition.merge_contribution(target, contrib, "test:mod")
        assert(ok == false)
        assert(fault.code == "LuaModuleDefaultStateInvalid")
        assert(string.find(fault.message, "must be a table"))
    end,

    metatable_on_section_value_rejected = function()
        local target = state_validator.create_empty_canonical_state()
        local bad_section = setmetatable({}, { __index = {} })
        local contrib = { player = bad_section }
        local ok, fault = state_composition.merge_contribution(target, contrib, "test:mod")
        assert(ok == false)
        assert(fault.code == "LuaStateValidationInvalid")
        assert(string.find(fault.message, "cannot have a metatable"))
    end,

    invalid_hook_type_rejected = function()
        local ctx = { session_generation = 1 }
        local modules = {
            {
                module_id = "test:bad_hook",
                create_default_state = "not_a_function",
            },
        }

        local tree, fault = state_composition.compose_default_state(ctx, modules)
        assert(tree == nil)
        assert(fault ~= nil)
        assert(fault.code == "LuaModuleLifecycleInvalid")
        assert(string.find(fault.message, "must be a function"))
    end,

    hook_runtime_error_handled = function()
        local ctx = { session_generation = 1 }
        local modules = {
            {
                module_id = "test:throwing_hook",
                create_default_state = function()
                    error("Intentional hook crash")
                end,
            },
        }

        local tree, fault = state_composition.compose_default_state(ctx, modules)
        assert(tree == nil)
        assert(fault ~= nil)
        assert(fault.code == "LuaModuleLifecycleError")
        assert(string.find(fault.message, "Module create_default_state failed:"), "fault message: " .. tostring(fault.message))
    end,

    invalid_hook_return_type_rejected = function()
        local ctx = { session_generation = 1 }
        local modules = {
            {
                module_id = "test:bad_return",
                create_default_state = function()
                    return "not_a_table_or_nil"
                end,
            },
        }

        local tree, fault = state_composition.compose_default_state(ctx, modules)
        assert(tree == nil)
        assert(fault ~= nil)
        assert(fault.code == "LuaModuleDefaultStateInvalid")
        assert(string.find(fault.message, "must return a table or nil"))
    end,

    missing_module_export_table_rejected = function()
        local ctx = { session_generation = 1 }
        local modules = {
            {
                module_id = "test:missing_export",
                module = "not_a_table",
            },
        }

        local tree, fault = state_composition.compose_default_state(ctx, modules)
        assert(tree == nil)
        assert(fault ~= nil)
        assert(fault.code == "LuaModuleLifecycleError")
        assert(string.find(fault.message, "Module export table is missing"))
    end,

    nil_hook_or_nil_return_is_noop = function()
        local ctx = { session_generation = 1 }
        local modules = {
            {
                module_id = "test:no_hook",
                module = {},
            },
            {
                module_id = "test:nil_return",
                create_default_state = function()
                    return nil
                end,
            },
        }

        local tree, fault = state_composition.compose_default_state(ctx, modules)
        assert(tree ~= nil)
        assert(fault == nil)
        assert(type(tree.meta) == "table")
    end,

    non_pollution_on_failure = function()
        local ctx = { session_generation = 1 }
        local modules = {
            {
                module_id = "test:mod_ok",
                create_default_state = function()
                    return { player = { ok_field = true } }
                end,
            },
            {
                module_id = "test:mod_bad",
                create_default_state = function()
                    error("Crash in second module")
                end,
            },
        }

        local tree, fault = state_composition.compose_default_state(ctx, modules)
        assert(tree == nil, "composition must return nil tree on failure")
        assert(fault ~= nil)
        assert(fault.code == "LuaModuleLifecycleError")

        -- Running session game.state must remain untouched and valid
        if _G.game and _G.game.state then
            assert(_G.game.state.player.ok_field == nil, "running session state must not be polluted by failed composition")
        end
    end,
}
