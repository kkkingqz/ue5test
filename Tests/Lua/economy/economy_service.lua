-- TGS-02 / TGS-03: Economy Service & Actor Stamina / Gold Specification
-- Verifies player stamina and gold state fields, canonical codec round-trip,
-- and rh:service.economy transactions with typed errors.

local state_validator = require("core:module.runtime.state_validator")
local canonical_codec = require("core:module.runtime.canonical_codec")
local mutation_window = require("core:module.runtime.mutation_window")

local function ensure_player()
    local player = game.instances.actors.player()
    if player then
        return player
    end
    local hero = game.instances.actors.create("rh:actor.character.hero", {
        stamina = 20,
        gold = 50,
    })
    game.state.meta.player_actor_id = hero.instance_id
    return hero
end

return {
    actor_stamina_and_gold_state_fields = function()
        mutation_window.execute_in_window(function()
            local player = ensure_player()
            assert(player ~= nil, "player actor must exist")

            player.stamina = 20
            player.gold = 50

            assert(player.stamina == 20, "stamina must be accessible via wrapper")
            assert(player.gold == 50, "gold must be accessible via wrapper")

            -- Validate state structure (raw unwrapped tree)
            local raw_state = mutation_window.unwrap_state(game.state)
            local ok_valid, err_valid = pcall(function()
                state_validator.validate_state_tree(raw_state)
            end)
            assert(ok_valid, "state with stamina and gold must pass validation: " .. tostring(err_valid))

            -- Verify round-trip encoding and decoding
            local encoded = canonical_codec.serialize(raw_state)
            local decoded = canonical_codec.deserialize(encoded)
            assert(decoded.actors[player.instance_id].stamina == 20, "decoded stamina must match")
            assert(decoded.actors[player.instance_id].gold == 50, "decoded gold must match")

            -- Decoded state is also a valid raw state tree
            state_validator.validate_state_tree(decoded)
        end)
    end,

    economy_service_registration_and_queries = function()
        local economy = game.services.get("rh:service.economy")
        assert(economy ~= nil, "rh:service.economy must be registered in game.services")

        mutation_window.execute_in_window(function()
            local player = ensure_player()
            player.gold = 100
            player.stamina = 50

            assert(economy.get_gold() == 100, "get_gold must return player gold")
            assert(economy.get_stamina() == 50, "get_stamina must return player stamina")
        end)
    end,

    economy_service_add_gold_and_stamina = function()
        local economy = game.services.get("rh:service.economy")
        assert(economy ~= nil)

        mutation_window.execute_in_window(function()
            local player = ensure_player()
            player.gold = 10
            player.stamina = 10

            local res_gold = economy.add_gold(15)
            assert(res_gold.ok == true, "add_gold must succeed")
            assert(res_gold.value.gold == 25, "gold must increase by 15 to 25")
            assert(res_gold.value.amount == 15)
            assert(player.gold == 25)

            local res_stamina = economy.add_stamina(20)
            assert(res_stamina.ok == true, "add_stamina must succeed")
            assert(res_stamina.value.stamina == 30, "stamina must increase by 20 to 30")
            assert(res_stamina.value.amount == 20)
            assert(player.stamina == 30)
        end)
    end,

    economy_service_spend_gold_and_stamina = function()
        local economy = game.services.get("rh:service.economy")
        assert(economy ~= nil)

        mutation_window.execute_in_window(function()
            local player = ensure_player()
            player.gold = 50
            player.stamina = 40

            local res_gold = economy.spend_gold(20)
            assert(res_gold.ok == true, "spend_gold must succeed")
            assert(res_gold.value.gold == 30, "gold must decrease by 20 to 30")
            assert(res_gold.value.amount == 20)
            assert(player.gold == 30)

            local res_stamina = economy.spend_stamina(15)
            assert(res_stamina.ok == true, "spend_stamina must succeed")
            assert(res_stamina.value.stamina == 25, "stamina must decrease by 15 to 25")
            assert(res_stamina.value.amount == 15)
            assert(player.stamina == 25)
        end)
    end,

    economy_service_insufficient_gold_and_stamina_refused = function()
        local economy = game.services.get("rh:service.economy")
        assert(economy ~= nil)

        mutation_window.execute_in_window(function()
            local player = ensure_player()
            player.gold = 10
            player.stamina = 5

            local res_gold = economy.spend_gold(50)
            assert(res_gold.ok == false, "spending more gold than balance must fail")
            assert(res_gold.error.code == "rh:error.economy.insufficient_gold")
            assert(res_gold.error.params.current_gold == 10)
            assert(res_gold.error.params.required_gold == 50)
            assert(player.gold == 10, "gold must not change on refusal")

            local res_stamina = economy.spend_stamina(20)
            assert(res_stamina.ok == false, "spending more stamina than balance must fail")
            assert(res_stamina.error.code == "rh:error.economy.insufficient_stamina")
            assert(res_stamina.error.params.current_stamina == 5)
            assert(res_stamina.error.params.required_stamina == 20)
            assert(player.stamina == 5, "stamina must not change on refusal")
        end)
    end,

    economy_service_invalid_amounts_refused = function()
        local economy = game.services.get("rh:service.economy")
        assert(economy ~= nil)

        mutation_window.execute_in_window(function()
            local player = ensure_player()
            player.gold = 100
            player.stamina = 50

            local invalid_values = { -1, -100, 1.5, 0.1, "10", {}, true, false }
            for _, val in ipairs(invalid_values) do
                local res_add_g = economy.add_gold(val)
                assert(res_add_g.ok == false, "add_gold with invalid amount must fail: " .. tostring(val))
                assert(res_add_g.error.code == "rh:error.economy.invalid_amount")

                local res_spend_g = economy.spend_gold(val)
                assert(res_spend_g.ok == false, "spend_gold with invalid amount must fail: " .. tostring(val))
                assert(res_spend_g.error.code == "rh:error.economy.invalid_amount")

                local res_add_s = economy.add_stamina(val)
                assert(res_add_s.ok == false, "add_stamina with invalid amount must fail: " .. tostring(val))
                assert(res_add_s.error.code == "rh:error.economy.invalid_amount")

                local res_spend_s = economy.spend_stamina(val)
                assert(res_spend_s.ok == false, "spend_stamina with invalid amount must fail: " .. tostring(val))
                assert(res_spend_s.error.code == "rh:error.economy.invalid_amount")
            end

            assert(player.gold == 100, "gold must remain 100")
            assert(player.stamina == 50, "stamina must remain 50")
        end)
    end,

    economy_service_player_not_found = function()
        local economy = game.services.get("rh:service.economy")
        assert(economy ~= nil)

        mutation_window.execute_in_window(function()
            local saved_player_id = game.state.meta.player_actor_id
            game.state.meta.player_actor_id = nil

            local res = economy.add_gold(10)
            assert(res.ok == false)
            assert(res.error.code == "rh:error.economy.player_not_found")

            local res_spend = economy.spend_gold(10)
            assert(res_spend.ok == false)
            assert(res_spend.error.code == "rh:error.economy.player_not_found")

            local res_add_s = economy.add_stamina(10)
            assert(res_add_s.ok == false)
            assert(res_add_s.error.code == "rh:error.economy.player_not_found")

            local res_spend_s = economy.spend_stamina(10)
            assert(res_spend_s.ok == false)
            assert(res_spend_s.error.code == "rh:error.economy.player_not_found")

            game.state.meta.player_actor_id = saved_player_id
        end)
    end,
}
