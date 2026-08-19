local mutation_window = require("core:module.runtime.mutation_window")

local function ensure_game_state(initial_stamina, initial_gold)
    local player = game.instances.actors.player()
    local merchant = nil
    if not player then
        game.runtime.dispatch_command({
            command_id = "rh:command.start_game",
            args = {},
            sequence = 900,
        })
        player = game.instances.actors.player()
    end
    local npcs = game.instances.actors.find_by_discriminator("npc")
    if #npcs > 0 then
        merchant = npcs[1]
    else
        merchant = game.instances.actors.create("rh:actor.npc.merchant", {
            current_location = "rh:location.city.market",
            gold = 100,
            stamina = 50,
        })
    end
    if initial_stamina ~= nil then
        player.stamina = initial_stamina
    end
    if initial_gold ~= nil then
        player.gold = initial_gold
    end
    return player, merchant
end

return {
    game_start_initializes_player_and_merchant = function()
        mutation_window.execute_in_window(function()
            local player, merchant = ensure_game_state(20, 50)
            assert(player ~= nil, "player must exist after start_game")
            assert(merchant ~= nil, "merchant must exist after start_game")
            assert(merchant.current_location_id == "rh:location.city.market" or merchant.current_location == "rh:location.city.market",
                "merchant must be located at market")
            assert(merchant:get_gold() >= 100, "merchant must have initial gold")
        end)
    end,

    shop_buy_sword_success = function()
        mutation_window.execute_in_window(function()
            local player, merchant = ensure_game_state(20, 50)
            player.current_location_id = "rh:location.city.market"

            -- Ensure merchant has sword
            if not merchant:has_item("rh:item.weapon.iron_sword") then
                merchant:add_item("rh:item.weapon.iron_sword")
            end

            local initial_player_gold = player:get_gold()
            local initial_merchant_gold = merchant:get_gold()

            -- Find the merchant's sword instance id
            local merchant_sword_id = nil
            for id, inst in pairs(game.state.item_instances) do
                if inst.owner_id == merchant.instance_id and inst.definition_id == "rh:item.weapon.iron_sword" then
                    merchant_sword_id = id
                    break
                end
            end
            assert(merchant_sword_id ~= nil, "merchant must have a sword instance before trade")

            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.buy",
                args = { "rh:item.weapon.iron_sword" },
                sequence = 901,
            })
            assert(seq == 901, "dispatch sequence must match")

            local result = game.runtime.last_command_result
            assert(result ~= nil and result.ok == true, "buy command must succeed")
            assert(player:get_gold() == initial_player_gold - 10, "player gold must decrease by 10")
            assert(merchant:get_gold() == initial_merchant_gold + 10, "merchant gold must increase by 10")

            -- Verify it is the EXACT SAME item instance transferred to player
            local sword_state = game.state.item_instances[merchant_sword_id]
            assert(sword_state ~= nil, "sword instance must still exist in item_instances")
            assert(sword_state.owner_id == player.instance_id, "sword instance owner must now be player")
            assert(player:has_item("rh:item.weapon.iron_sword") == true, "player must now own sword")
            assert(merchant:has_item("rh:item.weapon.iron_sword") == false, "merchant must no longer own sword")
        end)
    end,

    shop_buy_armor_success = function()
        mutation_window.execute_in_window(function()
            local player, merchant = ensure_game_state(20, 50)
            player.current_location_id = "rh:location.city.market"

            -- Ensure merchant has armor
            if not merchant:has_item("rh:item.armor.leather_armor") then
                merchant:add_item("rh:item.armor.leather_armor")
            end

            local initial_player_gold = player:get_gold()
            local initial_merchant_gold = merchant:get_gold()

            local merchant_armor_id = nil
            for id, inst in pairs(game.state.item_instances) do
                if inst.owner_id == merchant.instance_id and inst.definition_id == "rh:item.armor.leather_armor" then
                    merchant_armor_id = id
                    break
                end
            end
            assert(merchant_armor_id ~= nil, "merchant must have armor instance before trade")

            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.buy",
                args = { "rh:item.armor.leather_armor" },
                sequence = 902,
            })
            assert(seq == 902, "dispatch sequence must match")

            local result = game.runtime.last_command_result
            assert(result ~= nil and result.ok == true, "buy armor command must succeed")
            assert(player:get_gold() == initial_player_gold - 25, "player gold must decrease by 25")
            assert(merchant:get_gold() == initial_merchant_gold + 25, "merchant gold must increase by 25")

            local armor_state = game.state.item_instances[merchant_armor_id]
            assert(armor_state ~= nil, "armor instance must still exist in item_instances")
            assert(armor_state.owner_id == player.instance_id, "armor instance owner must now be player")
        end)
    end,

    shop_buy_refused_when_item_not_available = function()
        mutation_window.execute_in_window(function()
            local player, merchant = ensure_game_state(20, 50)
            player.current_location_id = "rh:location.city.market"

            -- Ensure merchant has NO sword
            while merchant:has_item("rh:item.weapon.iron_sword") do
                merchant:take_item("rh:item.weapon.iron_sword")
            end

            local initial_player_gold = player:get_gold()
            local initial_merchant_gold = merchant:get_gold()
            local initial_rev = mutation_window.write_revision()

            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.buy",
                args = { "rh:item.weapon.iron_sword" },
                sequence = 903,
            })
            assert(seq == 903, "dispatch sequence must match")

            local result = game.runtime.last_command_result
            assert(result ~= nil and result.ok == false, "buy when merchant has no item must fail")
            assert(result.error ~= nil and result.error.code == "rh:error.trade.item_not_available",
                "error code must be rh:error.trade.item_not_available, got: " .. tostring(result.error and result.error.code))

            assert(player:get_gold() == initial_player_gold, "player gold must remain unchanged")
            assert(merchant:get_gold() == initial_merchant_gold, "merchant gold must remain unchanged")
            local after_rev = mutation_window.write_revision()
            assert(after_rev == initial_rev, "state revision must remain completely unchanged on refusal")
        end)
    end,

    shop_buy_refused_when_insufficient_gold = function()
        mutation_window.execute_in_window(function()
            local player, merchant = ensure_game_state(20, 5)
            player.current_location_id = "rh:location.city.market"

            -- Ensure merchant has sword
            if not merchant:has_item("rh:item.weapon.iron_sword") then
                merchant:add_item("rh:item.weapon.iron_sword")
            end

            local initial_player_gold = player:get_gold()
            local initial_merchant_gold = merchant:get_gold()
            local initial_rev = mutation_window.write_revision()

            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.buy",
                args = { "rh:item.weapon.iron_sword" },
                sequence = 904,
            })
            assert(seq == 904, "dispatch sequence must match")

            local result = game.runtime.last_command_result
            assert(result ~= nil and result.ok == false, "buy with 5 gold must fail")
            assert(result.error ~= nil and result.error.code == "rh:error.shop.insufficient_gold",
                "error code must be insufficient_gold, got: " .. tostring(result.error and result.error.code))

            assert(player:get_gold() == initial_player_gold, "gold must remain unchanged")
            assert(merchant:get_gold() == initial_merchant_gold, "merchant gold must remain unchanged")
            assert(merchant:has_item("rh:item.weapon.iron_sword") == true, "merchant must still have sword")
            local after_rev = mutation_window.write_revision()
            assert(after_rev == initial_rev, "state revision must remain completely unchanged on refusal")
        end)
    end,

    shop_buy_refused_when_not_in_market = function()
        mutation_window.execute_in_window(function()
            local player, merchant = ensure_game_state(20, 50)
            player.current_location_id = "rh:location.city.tavern"

            local initial_gold = player:get_gold()
            local initial_rev = mutation_window.write_revision()

            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.buy",
                args = { "rh:item.weapon.iron_sword" },
                sequence = 905,
            })
            assert(seq == 905, "dispatch sequence must match")

            local result = game.runtime.last_command_result
            assert(result ~= nil and result.ok == false, "buy outside market must fail")
            assert(player:get_gold() == initial_gold, "gold must remain unchanged")
            local after_rev = mutation_window.write_revision()
            assert(after_rev == initial_rev, "state must remain unchanged")
        end)
    end,

    time_wait_day_success = function()
        mutation_window.execute_in_window(function()
            local player = ensure_game_state(10, 50)
            player.current_location_id = "rh:location.city.tavern"

            local initial_stamina = player.stamina
            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.time.wait_day",
                args = {},
                sequence = 906,
            })
            assert(seq == 906, "dispatch sequence must match")

            local result = game.runtime.last_command_result
            assert(result ~= nil and result.ok == true, "wait_day command must succeed")
            assert(player.stamina == initial_stamina + 10, "stamina must increase by 10 to 20, got: " .. tostring(player.stamina))
        end)
    end,

    time_wait_day_refused_outside_tavern = function()
        mutation_window.execute_in_window(function()
            local player = ensure_game_state(10, 50)
            player.current_location_id = "rh:location.city.market"

            local initial_stamina = player.stamina
            local initial_rev = mutation_window.write_revision()

            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.time.wait_day",
                args = {},
                sequence = 907,
            })
            assert(seq == 907, "dispatch sequence must match")

            local result = game.runtime.last_command_result
            assert(result ~= nil and result.ok == false, "wait_day outside tavern must fail")
            assert(player.stamina == initial_stamina, "stamina must remain unchanged")
            local after_rev = mutation_window.write_revision()
            assert(after_rev == initial_rev, "state must remain unchanged")
        end)
    end,

    work_do_work_success = function()
        mutation_window.execute_in_window(function()
            local player = ensure_game_state(20, 50)
            player.current_location_id = "rh:location.city.tavern"

            local initial_stamina = player.stamina
            local initial_gold = player.gold

            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.work.do_work",
                args = {},
                sequence = 908,
            })
            assert(seq == 908, "dispatch sequence must match")

            local result = game.runtime.last_command_result
            assert(result ~= nil and result.ok == true, "do_work command must succeed")
            assert(player.stamina == initial_stamina - 2, "stamina must decrease by 2 to 18, got: " .. tostring(player.stamina))
            assert(player.gold == initial_gold + 10, "gold must increase by 10 to 60, got: " .. tostring(player.gold))
        end)
    end,

    work_do_work_refused_when_insufficient_stamina = function()
        mutation_window.execute_in_window(function()
            local player = ensure_game_state(5, 50)
            player.current_location_id = "rh:location.city.tavern"

            local initial_stamina = player.stamina
            local initial_gold = player.gold
            local initial_rev = mutation_window.write_revision()

            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.work.do_work",
                args = {},
                sequence = 909,
            })
            assert(seq == 909, "dispatch sequence must match")

            local result = game.runtime.last_command_result
            assert(result ~= nil and result.ok == false, "do_work with stamina 5 must fail")
            assert(result.error ~= nil and result.error.code == "rh:error.work.insufficient_stamina",
                "error code must be insufficient_stamina, got: " .. tostring(result.error and result.error.code))

            assert(player.stamina == initial_stamina, "stamina must remain unchanged")
            assert(player.gold == initial_gold, "gold must remain unchanged")
            local after_rev = mutation_window.write_revision()
            assert(after_rev == initial_rev, "state must remain unchanged on refusal")
        end)
    end,

    work_do_work_refused_outside_tavern = function()
        mutation_window.execute_in_window(function()
            local player = ensure_game_state(20, 50)
            player.current_location_id = "rh:location.city.gate"

            local initial_stamina = player.stamina
            local initial_gold = player.gold
            local initial_rev = mutation_window.write_revision()

            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.work.do_work",
                args = {},
                sequence = 910,
            })
            assert(seq == 910, "dispatch sequence must match")

            local result = game.runtime.last_command_result
            assert(result ~= nil and result.ok == false, "do_work outside tavern must fail")
            assert(player.stamina == initial_stamina, "stamina must remain unchanged")
            assert(player.gold == initial_gold, "gold must remain unchanged")
            local after_rev = mutation_window.write_revision()
            assert(after_rev == initial_rev, "state must remain unchanged")
        end)
    end,
}
