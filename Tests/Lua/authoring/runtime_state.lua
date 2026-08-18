-- DLA-14, DLA-15, DLA-16: Universal Runtime State, Sparse Materialization & Actor Location Specification (ADR-0027)
-- Verifies:
--   1. Universal definition runtime state section (state.definitions)
--   2. Dynamic state section registration and freeze
--   3. Rejection of non-existent definition IDs in state.definitions
--   4. Sparse materialization: unwritten properties read schema default
--   5. Explicit reset() removes overrides and restores sparse default
--   6. Definition wrapper decorators and domain methods
--   7. Location as actor property, world.current_location read-only accessor, and travel service mutation

local mutation_window = require("core:module.runtime.mutation_window")
local state_validator = require("core:module.runtime.state_validator")
local properties = require("core:module.authoring.properties")
local actor_registry = require("core:module.runtime.actor_registry")
local authoring_context = require("core:module.authoring.context")
local canonical_codec = require("core:module.runtime.canonical_codec")
local state_hasher = require("core:module.runtime.state_hasher")
local save_module = require("core:module.runtime.save")
local load_module = require("core:module.runtime.load")
local location_service = require("core:module.gameplay.location_service")

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
            world = {},
            quests = {},
            mods = {},
            definitions = {},
        },
        repository = {
            get = function(id)
                if id == "core:actor.hero" then
                    return {
                        id = "core:actor.hero",
                        discriminator = "hero_type",
                        data = { name = "Hero" },
                    }
                elseif id == "rh:location.city.market" then
                    return {
                        id = "rh:location.city.market",
                        discriminator = "location",
                        data = {
                            name = "Market Square",
                            connected_location_ids = { "rh:location.city.tavern" },
                        },
                    }
                elseif id == "rh:location.city.tavern" then
                    return {
                        id = "rh:location.city.tavern",
                        discriminator = "location",
                        data = {
                            name = "City Tavern",
                            connected_location_ids = { "rh:location.city.market" },
                        },
                    }
                end
                return nil
            end,
            exists = function(id)
                return id == "core:actor.hero" or id == "rh:location.city.market" or id == "rh:location.city.tavern"
            end,
        },
    }

    properties.clear_for_test()
    state_validator.clear_for_test()
    local registry = actor_registry.create_registry()
    local service_registry = require("core:module.runtime.service_registry")
    service_registry.register()
    _G.game.instances = {
        actors = registry,
    }
    local world_module = require("core:module.runtime.world")
    world_module.register()

    local ok, err = pcall(fn, registry)
    _G.game = prev_game
    properties.clear_for_test()
    state_validator.clear_for_test()
    if not ok then
        error(err)
    end
end

return {
    universal_definition_state_section_in_canonical_state = function()
        run_with_mock_environment(function(registry)
            -- 1. Create empty canonical state contains definitions
            local empty_state = state_validator.create_empty_canonical_state()
            assert(type(empty_state.definitions) == "table", "create_empty_canonical_state must initialize definitions section")

            -- 2. State tree with valid definition in state.definitions passes validation
            local state = {
                meta = { schema_version = 1, save_version = 1, save_id = "", instance_counters = {}, prng = {}, time = {} },
                player = {},
                actors = {},
                item_instances = {},
                world = {},
                quests = {},
                mods = {},
                definitions = {
                    ["rh:location.city.market"] = {
                        tax_rate = 10,
                    },
                },
            }
            local ok_valid = pcall(function()
                state_validator.validate_state_tree(state)
            end)
            assert(ok_valid, "Valid definition in state.definitions must pass validation")

            -- 3. Nonexistent definition in state.definitions must be rejected
            local state_bad = {
                meta = { schema_version = 1, save_version = 1, save_id = "", instance_counters = {}, prng = {}, time = {} },
                player = {},
                actors = {},
                item_instances = {},
                world = {},
                quests = {},
                mods = {},
                definitions = {
                    ["rh:location.city.nonexistent"] = {
                        tax_rate = 10,
                    },
                },
            }
            local ok_bad, err_bad = pcall(function()
                state_validator.validate_state_tree(state_bad)
            end)
            assert(not ok_bad, "Nonexistent definition in state.definitions must be rejected")
            assert(string.find(tostring(err_bad), "not found in pinned repository"), "Expected repository resolution error")

            -- 4. Dynamic section registration
            state_validator.register_section("custom_mod_section")
            assert(state_validator.is_canonical_section("custom_mod_section") == true, "Registered custom section must be recognized as canonical")
        end)
    end,

    sparse_materialization_and_reset = function()
        run_with_mock_environment(function(registry)
            properties.register_schema("location", {
                fields = {
                    name = { kind = "string", storage = "definition", write_policy = "read_only" },
                    tax_rate = { kind = "int64", storage = "runtime_state", write_policy = "plain", default = 5, min = 0, max = 100 },
                    treasury = { kind = "int64", storage = "runtime_state", write_policy = "managed", operations = { "deposit", "withdraw" } },
                },
            })

            local market = properties.wrap_definition("rh:location.city.market")
            assert(market ~= nil, "wrap_definition must return wrapper")

            -- 1. Read definition property
            assert(market.name == "Market Square", "Static definition property read failed")

            -- 2. Read unmaterialized runtime property (sparse) -> returns schema default
            assert(market.tax_rate == 5, "Unmaterialized property must return schema default (5)")
            assert(game.state.definitions["rh:location.city.market"] == nil, "State must remain sparse before write")

            -- 3. Write plain runtime property -> materializes into state
            mutation_window.execute_in_window(function()
                market.tax_rate = 15
            end)
            assert(market.tax_rate == 15, "Written property must return updated value")
            assert(game.state.definitions["rh:location.city.market"] ~= nil, "State must be materialized")
            assert(game.state.definitions["rh:location.city.market"].tax_rate == 15, "State value must match")

            -- 4. Write violating schema constraints -> rejected, state unchanged
            local write_err = false
            mutation_window.execute_in_window(function()
                local ok, err = pcall(function()
                    market.tax_rate = 150
                end)
                if not ok then
                    write_err = true
                    assert(string.find(tostring(err), "FieldValidationConstraintFailed"), "Expected constraint error")
                end
            end)
            assert(write_err, "Constraint violation must be rejected")
            assert(market.tax_rate == 15, "State must remain unchanged")

            -- 5. Write to definition field -> rejected
            local def_write_err = false
            mutation_window.execute_in_window(function()
                local ok, err = pcall(function()
                    market.name = "New Market"
                end)
                if not ok then
                    def_write_err = true
                    assert(string.find(tostring(err), "Cannot modify definition field"), "Expected definition write rejection")
                end
            end)
            assert(def_write_err, "Definition field write must be rejected")

            -- 6. Write to managed field -> rejected with operations hint
            local managed_err = false
            mutation_window.execute_in_window(function()
                local ok, err = pcall(function()
                    market.treasury = 500
                end)
                if not ok then
                    managed_err = true
                    assert(string.find(tostring(err), "Cannot assign directly to managed field 'treasury'"), "Expected managed field error")
                    assert(string.find(tostring(err), "deposit, withdraw"), "Expected operations hint")
                end
            end)
            assert(managed_err, "Managed field write must be rejected")

            -- 7. Reset property -> removes override and restores schema default
            mutation_window.execute_in_window(function()
                market:reset("tax_rate")
            end)
            assert(game.state.definitions["rh:location.city.market"].tax_rate == nil, "reset() must remove key from state")
            assert(market.tax_rate == 5, "Property must return schema default (5) after reset()")
        end)
    end,

    definition_wrapper_decorator_and_context_access = function()
        run_with_mock_environment(function(registry)
            properties.register_schema("location", {
                fields = {
                    name = { kind = "string", storage = "definition", write_policy = "read_only" },
                },
            })

            properties.register_definition_type("location", function(base)
                return {
                    is_market = function(self)
                        return base.name == "Market Square"
                    end,
                }
            end)

            local rh = authoring_context.gameplay("rh")
            local market = rh.location("city.market")
            assert(market ~= nil, "rh.location shortcut must return definition wrapper")
            assert(market.name == "Market Square", "Definition field read via shortcut failed")
            assert(market:is_market() == true, "Decorated method on definition wrapper failed")

            local def_market = rh.def.location("city.market")
            assert(def_market:is_market() == true, "rh.def.location access failed")
        end)
    end,

    location_as_actor_property_and_readonly_world = function()
        run_with_mock_environment(function(registry)
            registry.register_type("hero_type", function(base) return {} end)

            local hero = mutation_window.execute_in_window(function()
                local h = registry.create("core:actor.hero", {
                    current_location_id = "rh:location.city.market",
                })
                game.state.meta.player_actor_id = h.instance_id
                return h
            end)

            local world = game.instances.world()
            assert(world ~= nil, "world instance must be available")

            -- 1. world.current_location_id reads from player actor
            assert(world.current_location_id == "rh:location.city.market", "world.current_location_id must read from player actor")

            -- 2. Direct write to world.current_location_id is rejected
            local write_world_err = false
            mutation_window.execute_in_window(function()
                local ok, err = pcall(function()
                    world.current_location_id = "rh:location.city.tavern"
                end)
                if not ok then
                    write_world_err = true
                    assert(string.find(tostring(err), "WorldLocationReadOnly"), "Expected WorldLocationReadOnly error")
                end
            end)
            assert(write_world_err, "Direct write to world.current_location_id must be rejected")

            -- 3. Location service updates location on player actor
            location_service.register()
            local s = game.services.get("core:service.location")
            assert(s ~= nil, "location service must be available")

            local res = mutation_window.execute_in_window(function()
                return s.travel("rh:location.city.tavern")
            end)
            assert(res.ok == true, "travel must succeed")
            assert(hero.current_location_id == "rh:location.city.tavern", "Player actor location must be updated")
            assert(world.current_location_id == "rh:location.city.tavern", "world.current_location_id must reflect updated player location")
            assert(game.state.world.current_location_id == nil, "state.world.current_location_id must NOT exist in state.world")
        end)
    end,
}
