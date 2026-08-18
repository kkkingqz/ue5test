-- TGS-10: Location Actions Specification (Shop, Time & Work)
-- Verifies buy_sword, buy_armor, wait_day, do_work commands, item allocation,
-- state validation, and read-only validator refusal semantics.

local mutation_window = require("core:module.runtime.mutation_window")
local state_validator = require("core:module.runtime.state_validator")
local canonical_codec = require("core:module.runtime.canonical_codec")

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
            assert(seq == 901)

            local res = game.runtime.last_command_result
            assert(res ~= nil and res.ok == true, "buy sword must succeed")
            assert(player.gold == initial_gold - 10, "gold must decrease by 10")

            -- Verify item allocated in state
            local found_item = false
            for _, item_entry in pairs(game.state.item_instances or {}) do
                if item_entry.definition_id == "rh:item.weapon.iron_sword" and item_entry.owner_id == player.instance_id then
                    found_item = true
                    break
                end
            end
            assert(found_item, "item instance must exist in state.item_instances")

            -- State validation
            local raw_state = canonical_codec.deserialize(canonical_codec.serialize(game.state))
            state_validator.validate_state_tree(raw_state)
        end)
    end,

    shop_buy_armor_success = function()
        mutation_window.execute_in_window(function()
            local player = ensure_player(20, 50)
            local world = game.instances.world()
            player.current_location_id = "rh:location.city.market"

            local initial_gold = player.gold
            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.buy",
                args = { item = "rh:item.armor.leather_armor" },
                sequence = 902,
            })
            assert(seq == 902)

            local res = game.runtime.last_command_result
            assert(res ~= nil and res.ok == true, "buy armor must succeed")
            assert(player.gold == initial_gold - 25, "gold must decrease by 25")

            local found_armor = false
            for _, item_entry in pairs(game.state.item_instances or {}) do
                if item_entry.definition_id == "rh:item.armor.leather_armor" and item_entry.owner_id == player.instance_id then
                    found_armor = true
                    break
                end
            end
            assert(found_armor, "armor instance must exist in state.item_instances")
        end)
    end,

    shop_buy_insufficient_gold_refused = function()
        mutation_window.execute_in_window(function()
            local player = ensure_player(20, 5)
            local world = game.instances.world()
            player.current_location_id = "rh:location.city.market"

            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.buy",
                args = { item = "rh:item.weapon.iron_sword" },
                sequence = 903,
            })
            assert(seq == 903)

            local res = game.runtime.last_command_result
            assert(res ~= nil and res.ok == false, "must be refused on insufficient gold")
            assert(res.error.code == "rh:error.shop.insufficient_gold")
            assert(res.error.params.current_gold == 5)
            assert(res.error.params.required_gold == 10)
            assert(player.gold == 5, "gold must not change on refusal")
        end)
    end,

    shop_buy_wrong_location_refused = function()
        mutation_window.execute_in_window(function()
            local player = ensure_player(20, 50)
            local world = game.instances.world()
            player.current_location_id = "rh:location.city.tavern"

            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.buy",
                args = { item = "rh:item.weapon.iron_sword" },
                sequence = 904,
            })
            assert(seq == 904)

            local res = game.runtime.last_command_result
            assert(res ~= nil and res.ok == false, "must be refused outside market")
            assert(res.error.code == "rh:error.location.wrong_location")
            assert(res.error.params.required_location_id == "rh:location.city.market")
            assert(res.error.params.current_location_id == "rh:location.city.tavern")
            assert(player.gold == 50, "gold must not change on refusal")
        end)
    end,

    time_wait_day_success = function()
        mutation_window.execute_in_window(function()
            local player = ensure_player(10, 50)
            local world = game.instances.world()
            player.current_location_id = "rh:location.city.tavern"

            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.time.wait_day",
                args = {},
                sequence = 905,
            })
            assert(seq == 905)

            local res = game.runtime.last_command_result
            assert(res ~= nil and res.ok == true, "wait_day must succeed")
            assert(player.stamina == 20, "stamina must increase by 10 to 20")
        end)
    end,

    time_wait_day_wrong_location_refused = function()
        mutation_window.execute_in_window(function()
            local player = ensure_player(10, 50)
            local world = game.instances.world()
            player.current_location_id = "rh:location.city.market"

            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.time.wait_day",
                args = {},
                sequence = 906,
            })
            assert(seq == 906)

            local res = game.runtime.last_command_result
            assert(res ~= nil and res.ok == false, "must be refused outside tavern")
            assert(res.error.code == "rh:error.location.wrong_location")
            assert(player.stamina == 10, "stamina must not change")
        end)
    end,

    work_do_work_success = function()
        mutation_window.execute_in_window(function()
            local player = ensure_player(10, 20)
            local world = game.instances.world()
            player.current_location_id = "rh:location.city.tavern"

            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.work.do_work",
                args = {},
                sequence = 907,
            })
            assert(seq == 907)

            local res = game.runtime.last_command_result
            assert(res ~= nil and res.ok == true, "do_work must succeed")
            assert(player.gold == 30, "gold must increase by 10 to 30")
            assert(player.stamina == 8, "stamina must decrease by 2 to 8")
        end)
    end,

    work_insufficient_stamina_refused = function()
        mutation_window.execute_in_window(function()
            local player = ensure_player(5, 20) -- stamina must be > 5, so 5 is insufficient
            local world = game.instances.world()
            player.current_location_id = "rh:location.city.tavern"

            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.work.do_work",
                args = {},
                sequence = 908,
            })
            assert(seq == 908)

            local res = game.runtime.last_command_result
            assert(res ~= nil and res.ok == false, "do_work must be refused when stamina <= 5")
            assert(res.error.code == "rh:error.work.insufficient_stamina")
            assert(res.error.params.current_stamina == 5)
            assert(res.error.params.required_stamina == 6)
            assert(player.gold == 20, "gold must not change")
            assert(player.stamina == 5, "stamina must not change")
        end)
    end,

    work_wrong_location_refused = function()
        mutation_window.execute_in_window(function()
            local player = ensure_player(20, 20)
            local world = game.instances.world()
            player.current_location_id = "rh:location.city.gate"

            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.work.do_work",
                args = {},
                sequence = 909,
            })
            assert(seq == 909)

            local res = game.runtime.last_command_result
            assert(res ~= nil and res.ok == false, "must be refused outside tavern")
            assert(res.error.code == "rh:error.location.wrong_location")
            assert(player.gold == 20, "gold must not change")
            assert(player.stamina == 20, "stamina must not change")
        end)
    end,
}
