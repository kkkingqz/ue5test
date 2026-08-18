-- CBM-04..07: Actor Type Decorator & Extension Specification
-- Verifies registry API, decorator wrapping, identity invariants,
-- unregistered type handling, freeze behavior, and domain method delegation.

local actor_reg_mod = require("core:module.runtime.actor_registry")
local mutation_window = require("core:module.runtime.mutation_window")

return {
    register_type_and_lookup = function()
        local registry = actor_reg_mod.create_registry()
        local called = false
        local decorator = function(base)
            called = true
            return setmetatable({
                custom_method = function() return "ok" end,
            }, { __index = base })
        end

        registry.register_type("hero", decorator)
        local types = registry.types()
        assert(#types == 1 and types[1] == "hero", "types() must list registered discriminator")

        -- Wrapping with registered decorator
        local fake_state = { instance_id = "actor@1:1", definition_id = "core:actor.test", discriminator = "hero" }
        local wrapped = registry.wrap(fake_state)
        assert(wrapped ~= nil, "wrapper must be created")
        assert(called == true, "decorator must be called")
        assert(wrapped.custom_method() == "ok", "decorated method must be callable")
    end,

    register_type_rejection_cases = function()
        local registry = actor_reg_mod.create_registry()

        -- 1. Invalid discriminator (non-string or empty)
        local bad_discriminators = { 123, {}, "", false }
        for _, bad_d in ipairs(bad_discriminators) do
            local ok, err = pcall(function()
                registry.register_type(bad_d, function(b) return b end)
            end)
            assert(not ok, "register_type must reject bad discriminator: " .. tostring(bad_d))
            assert(tostring(err):find("InvalidActorDiscriminator") ~= nil,
                "must raise InvalidActorDiscriminator, got: " .. tostring(err))
        end

        -- 2. Non-function decorator
        local bad_decorators = { 123, "fn", {}, true }
        for _, bad_dec in ipairs(bad_decorators) do
            local ok, err = pcall(function()
                registry.register_type("test_type", bad_dec)
            end)
            assert(not ok, "register_type must reject non-function decorator: " .. tostring(bad_dec))
            assert(tostring(err):find("InvalidActorDecorator") ~= nil,
                "must raise InvalidActorDecorator, got: " .. tostring(err))
        end

        -- 3. Chained decorator composition across package layers
        registry.register_type("warrior", function(b)
            return setmetatable({ m1 = function() return 1 end }, { __index = b })
        end)
        registry.register_type("warrior", function(b)
            return setmetatable({ m2 = function() return 2 end }, { __index = b })
        end)
        local warrior = registry.wrap({ instance_id = "w@1", discriminator = "warrior" })
        assert(warrior.m1() == 1 and warrior.m2() == 2, "chained decorators must both provide their methods")
    end,

    freeze_prevents_late_registration = function()
        local registry = actor_reg_mod.create_registry()
        registry.register_type("mage", function(b) return b end)
        assert(registry.is_frozen() == false, "registry must not be frozen initially")

        registry.freeze()
        assert(registry.is_frozen() == true, "registry must be frozen after freeze()")

        local ok, err = pcall(function()
            registry.register_type("paladin", function(b) return b end)
        end)
        assert(not ok, "register_type after freeze must be rejected")
        assert(tostring(err):find("ActorTypeRegistryFrozen") ~= nil,
            "must raise ActorTypeRegistryFrozen, got: " .. tostring(err))
    end,

    decorator_invalid_return_rejected = function()
        local registry = actor_reg_mod.create_registry()
        registry.register_type("invalid_ret", function(_b)
            return 123 -- non-table
        end)

        local fake_state = { instance_id = "actor@1:1", definition_id = "core:actor.test", discriminator = "invalid_ret" }
        local ok, err = pcall(function()
            registry.wrap(fake_state)
        end)
        assert(not ok, "wrap must reject decorator returning non-table")
        assert(tostring(err):find("ActorDecoratorInvalid") ~= nil,
            "must raise ActorDecoratorInvalid, got: " .. tostring(err))
    end,

    identity_invariants_preserved = function()
        local registry = actor_reg_mod.create_registry()
        registry.register_type("hero", function(base)
            -- Attempting to forge identity fields in returned table
            return setmetatable({
                instance_id = "forged_id",
                definition_id = "forged_def",
                discriminator = "forged_disc",
                custom_fn = function() return "custom" end,
            }, { __index = base })
        end)

        local raw_state = { instance_id = "actor@1:42", definition_id = "core:actor.real", discriminator = "hero", hp = 100 }
        local wrapped = registry.wrap(raw_state)

        -- Identity fields MUST return actual state identity, not forged ones
        assert(wrapped.instance_id == "actor@1:42", "instance_id must not be overridden by decorator")
        assert(wrapped.definition_id == "core:actor.real", "definition_id must not be overridden by decorator")
        assert(wrapped.discriminator == "hero", "discriminator must not be overridden by decorator")
        assert(wrapped.custom_fn() == "custom", "custom methods must work")
        assert(wrapped.hp == 100, "state fields must delegate")

        -- State mutation through wrapper
        wrapped.hp = 80
        assert(raw_state.hp == 80, "state field mutation must propagate to underlying state")

        -- Attempting to mutate immutable identity fields must throw
        local ok_id, err_id = pcall(function()
            wrapped.instance_id = "new_id"
        end)
        assert(not ok_id, "writing to instance_id must be rejected")
        assert(tostring(err_id):find("ActorDiscriminatorImmutable") ~= nil)

        local ok_disc, err_disc = pcall(function()
            wrapped.discriminator = "new_disc"
        end)
        assert(not ok_disc, "writing to discriminator must be rejected")
        assert(tostring(err_disc):find("ActorDiscriminatorImmutable") ~= nil)
    end,

    unregistered_discriminator_handling = function()
        local registry = actor_reg_mod.create_registry()

        -- 1. Unregistered discriminator raises typed error ActorTypeNotRegistered
        local unreg_state = { instance_id = "actor@1:1", definition_id = "test:def", discriminator = "alien" }
        local ok_unreg, err_unreg = pcall(function()
            registry.wrap(unreg_state)
        end)
        assert(not ok_unreg, "unregistered discriminator must be rejected")
        assert(tostring(err_unreg):find("ActorTypeNotRegistered") ~= nil,
            "must raise ActorTypeNotRegistered, got: " .. tostring(err_unreg))

        -- 2. After registering decorator for discriminator, wrapping succeeds
        registry.register_type("alien", function(base)
            return {
                speak = function() return "greetings" end,
            }
        end)
        local wrapped = registry.wrap(unreg_state)
        assert(wrapped ~= nil, "registered discriminator must yield wrapper")
        assert(wrapped.speak() == "greetings", "decorator method must be callable")
        assert(wrapped.instance_id == "actor@1:1", "identity must be preserved")
    end,

    rh_actor_decorators_provide_economy_methods = function()
        -- In live environment, rh registers decorators for player and npc
        if game and game.instances and game.instances.actors then
            mutation_window.execute_in_window(function()
                local player = game.instances.actors.player()
                if not player and game.instances.actors.create and game.state and game.state.meta then
                    local hero = game.instances.actors.create("rh:actor.character.hero", { gold = 42 })
                    game.state.meta.player_actor_id = hero.instance_id
                    player = hero
                end

                if player then
                    assert(player.is_player() == true, "player actor is_player() must be true")
                    assert(player.is_npc() == false, "player actor is_npc() must be false")
                    assert(type(player.get_gold) == "function", "player actor must have get_gold method")
                    assert(type(player.add_gold) == "function", "player actor must have add_gold method")
                    assert(player.get_gold() == (player.gold or 0), "get_gold() must return gold")
                end
            end)
        end
    end,
}
