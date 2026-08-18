-- TGS-10 / TSL-17: Location Actions Specification (Shop, Time & Work, FullGame tier)
-- Verifies buy_sword, buy_armor, wait_day, do_work commands, item allocation,
-- state validation, and read-only validator refusal semantics in rh package.

local mutation_window = require("core:module.runtime.mutation_window")

local function ensure_player(initial_stamina, initial_gold)
    local player = game.instances.actors.player()
    if not player then
        local hero = game.instances.actors.create("rh:actor.character.hero", {
            stamina = initial_stamina or 20,
            gold = initial_gold or 50,
        })
        game.state.meta.player_actor_id = hero.instance_id
        return hero
    end
    if initial_stamina ~= nil then
        player.stamina = initial_stamina
    end
    if initial_gold ~= nil then
        player.gold = initial_gold
    end
    return player
end

return {
    shop_buy_sword_success = function()
        mutation_window.execute_in_window(function()
            local player = ensure_player(20, 50)
            local world = game.instances.world()
            player.current_location_id = "rh:location.city.market"

            local initial_gold = player.gold
            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.buy",
                args = { item = "rh:item.weapon.iron_sword" },
                sequence = 901,
            })
            assert(seq == 901, "dispatch sequence must match")

            local result = game.runtime.last_command_result
            assert(result ~= nil and result.ok == true, "buy command must succeed")
            assert(player.gold == initial_gold - 10, "gold must decrease by 10 to 40, got: " .. tostring(player.gold))

            local found_item_id = nil
            for id, inst in pairs(game.state.item_instances) do
                if inst.owner_id == player.instance_id and inst.definition_id == "rh:item.weapon.iron_sword" then
                    found_item_id = id
                    break
                end
            end
            assert(found_item_id ~= nil, "item instance must exist with player owner_id")
        end)
    end,

    shop_buy_armor_success = function()
        mutation_window.execute_in_window(function()
            local player = ensure_player(20, 50)
            player.current_location_id = "rh:location.city.market"

            local initial_gold = player.gold
            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.buy",
                args = { item = "rh:item.armor.leather_armor" },
                sequence = 902,
            })
            assert(seq == 902, "dispatch sequence must match")

            local result = game.runtime.last_command_result
            assert(result ~= nil and result.ok == true, "buy armor command must succeed")
            assert(player.gold == initial_gold - 25, "gold must decrease by 25, got: " .. tostring(player.gold))

            local found_armor_id = nil
            for id, inst in pairs(game.state.item_instances) do
                if inst.owner_id == player.instance_id and inst.definition_id == "rh:item.armor.leather_armor" then
                    found_armor_id = id
                    break
                end
            end
            assert(found_armor_id ~= nil, "armor instance must exist with player owner_id")
        end)
    end,

    shop_buy_refused_when_insufficient_gold = function()
        mutation_window.execute_in_window(function()
            local player = ensure_player(20, 5)
            player.current_location_id = "rh:location.city.market"

            local initial_gold = player.gold
            local initial_rev = mutation_window.write_revision()

            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.buy",
                args = { item = "rh:item.weapon.iron_sword" },
                sequence = 903,
            })
            assert(seq == 903, "dispatch sequence must match")

            local result = game.runtime.last_command_result
            assert(result ~= nil and result.ok == false, "buy with 5 gold must fail")
            assert(result.error ~= nil and result.error.code == "rh:error.shop.insufficient_gold",
                "error code must be insufficient_gold, got: " .. tostring(result.error and result.error.code))

            assert(player.gold == initial_gold, "gold must remain unchanged")
            local after_rev = mutation_window.write_revision()
            assert(after_rev == initial_rev, "state revision must remain completely unchanged on refusal")
        end)
    end,

    shop_buy_refused_when_not_in_market = function()
        mutation_window.execute_in_window(function()
            local player = ensure_player(20, 50)
            player.current_location_id = "rh:location.city.tavern"

            local initial_gold = player.gold
            local initial_rev = mutation_window.write_revision()

            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.buy",
                args = { item = "rh:item.weapon.iron_sword" },
                sequence = 904,
            })
            assert(seq == 904, "dispatch sequence must match")

            local result = game.runtime.last_command_result
            assert(result ~= nil and result.ok == false, "buy outside market must fail")
            assert(player.gold == initial_gold, "gold must remain unchanged")
            local after_rev = mutation_window.write_revision()
            assert(after_rev == initial_rev, "state must remain unchanged")
        end)
    end,

    time_wait_day_success = function()
        mutation_window.execute_in_window(function()
            local player = ensure_player(10, 50)
            player.current_location_id = "rh:location.city.tavern"

            local initial_stamina = player.stamina
            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.time.wait_day",
                args = {},
                sequence = 905,
            })
            assert(seq == 905, "dispatch sequence must match")

            local result = game.runtime.last_command_result
            assert(result ~= nil and result.ok == true, "wait_day command must succeed")
            assert(player.stamina == initial_stamina + 10, "stamina must increase by 10 to 20, got: " .. tostring(player.stamina))
        end)
    end,

    time_wait_day_refused_outside_tavern = function()
        mutation_window.execute_in_window(function()
            local player = ensure_player(10, 50)
            player.current_location_id = "rh:location.city.market"

            local initial_stamina = player.stamina
            local initial_rev = mutation_window.write_revision()

            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.time.wait_day",
                args = {},
                sequence = 906,
            })
            assert(seq == 906, "dispatch sequence must match")

            local result = game.runtime.last_command_result
            assert(result ~= nil and result.ok == false, "wait_day outside tavern must fail")
            assert(player.stamina == initial_stamina, "stamina must remain unchanged")
            local after_rev = mutation_window.write_revision()
            assert(after_rev == initial_rev, "state must remain unchanged")
        end)
    end,

    work_do_work_success = function()
        mutation_window.execute_in_window(function()
            local player = ensure_player(20, 50)
            player.current_location_id = "rh:location.city.tavern"

            local initial_stamina = player.stamina
            local initial_gold = player.gold

            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.work.do_work",
                args = {},
                sequence = 907,
            })
            assert(seq == 907, "dispatch sequence must match")

            local result = game.runtime.last_command_result
            assert(result ~= nil and result.ok == true, "do_work command must succeed")
            assert(player.stamina == initial_stamina - 2, "stamina must decrease by 2 to 18, got: " .. tostring(player.stamina))
            assert(player.gold == initial_gold + 10, "gold must increase by 10 to 60, got: " .. tostring(player.gold))
        end)
    end,

    work_do_work_refused_when_insufficient_stamina = function()
        mutation_window.execute_in_window(function()
            local player = ensure_player(5, 50)
            player.current_location_id = "rh:location.city.tavern"

            local initial_stamina = player.stamina
            local initial_gold = player.gold
            local initial_rev = mutation_window.write_revision()

            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.work.do_work",
                args = {},
                sequence = 908,
            })
            assert(seq == 908, "dispatch sequence must match")

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
            local player = ensure_player(20, 50)
            player.current_location_id = "rh:location.city.gate"

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
            assert(result ~= nil and result.ok == false, "do_work outside tavern must fail")
            assert(player.stamina == initial_stamina, "stamina must remain unchanged")
            assert(player.gold == initial_gold, "gold must remain unchanged")
            local after_rev = mutation_window.write_revision()
            assert(after_rev == initial_rev, "state must remain unchanged")
        end)
    end,
}
