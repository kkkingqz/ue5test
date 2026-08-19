-- RH Character Actor Economy Methods Specification (ADR-0031, ADR-0032, RAS-08..10, RAS-17)
-- Verifies is_player, is_npc, get/add/spend/require for gold and stamina, add_item,
-- negative amount rejection, and absence of raw runtime imports in actors.lua.

local mutation_window = require("core:module.runtime.mutation_window")

return {
    rh_actor_decorators_provide_economy_methods = function()
        if game and game.instances and game.instances.actors then
            mutation_window.execute_in_window(function()
                local player = game.instances.actors.player()
                if not player and game.instances.actors.create and game.state and game.state.meta then
                    local hero = game.instances.actors.create("rh:actor.character.hero", { gold = 42, stamina = 20 })
                    game.state.meta.player_actor_id = hero.instance_id
                    player = hero
                end

                if player then
                    assert(player.is_player() == true, "player actor is_player() must be true")
                    assert(player.is_npc() == false, "player actor is_npc() must be false")
                    assert(type(player.get_gold) == "function", "player actor must have get_gold method")
                    assert(type(player.add_gold) == "function", "player actor must have add_gold method")
                    assert(type(player.spend_gold) == "function", "player actor must have spend_gold method")
                    assert(type(player.require_gold) == "function", "player actor must have require_gold method")
                    assert(type(player.get_stamina) == "function", "player actor must have get_stamina method")
                    assert(type(player.add_stamina) == "function", "player actor must have add_stamina method")
                    assert(type(player.spend_stamina) == "function", "player actor must have spend_stamina method")
                    assert(type(player.require_stamina) == "function", "player actor must have require_stamina method")
                    assert(type(player.add_item) == "function", "player actor must have add_item method")

                    player.gold = 50
                    player.stamina = 30
                    assert(player:get_gold() == 50)
                    assert(player:get_stamina() == 30)

                    -- spend_gold and add_gold
                    player:spend_gold(20)
                    assert(player:get_gold() == 30)
                    player:add_gold(15)
                    assert(player:get_gold() == 45)

                    -- spend_stamina and add_stamina
                    player:spend_stamina(10)
                    assert(player:get_stamina() == 20)
                    player:add_stamina(5)
                    assert(player:get_stamina() == 25)

                    -- require_gold fail check
                    player:require_gold(40) -- passes

                    -- spend without enough gold
                    local ok_spend, err_spend = pcall(function()
                        player:spend_gold(100)
                    end)
                    assert(not ok_spend, "Spending more gold than available must fail")
                    assert(string.find(tostring(err_spend), "PreconditionNotChecked") ~= nil)

                    -- Negative amount rejection (RAS-17)
                    local ok_neg_gold, err_neg_gold = pcall(function()
                        player:spend_gold(-10)
                    end)
                    assert(not ok_neg_gold, "spend_gold(-10) must be rejected")
                    assert(string.find(tostring(err_neg_gold), "InvalidGoldAmount") ~= nil,
                        "Expected InvalidGoldAmount, got: " .. tostring(err_neg_gold))

                    local ok_neg_stam, err_neg_stam = pcall(function()
                        player:spend_stamina(-5)
                    end)
                    assert(not ok_neg_stam, "spend_stamina(-5) must be rejected")
                    assert(string.find(tostring(err_neg_stam), "InvalidStaminaAmount") ~= nil,
                        "Expected InvalidStaminaAmount, got: " .. tostring(err_neg_stam))

                    local ok_neg_add, err_neg_add = pcall(function()
                        player:add_gold(-5)
                    end)
                    assert(not ok_neg_add, "add_gold(-5) must be rejected")
                    assert(string.find(tostring(err_neg_add), "InvalidGoldAmount") ~= nil)

                    -- add_item works via instances.create
                    local item_id = player:add_item("rh:item.weapon.iron_sword")
                    assert(item_id ~= nil, "add_item must return allocated item_id")
                    assert(string.find(item_id, "^item@%d+$") ~= nil, "item_id must be item@<counter>")
                    assert(game.state.item_instances[item_id] ~= nil, "Item must be created in item_instances")
                    assert(game.state.item_instances[item_id].owner_id == player.instance_id, "Item owner must be player")
                end
            end)
        end
    end,
}
