-- Authoring gameplay script for rh package (ADR-0028, SAS-17..18)
-- Defines concise gameplay rules for travel, resting, working, and buying items.

local tavern = location("city.tavern")
local market = location("city.market")

local function handle_travel(target)
    player.current_location:require_connected(target)
    player:require_stamina(5, "travel.insufficient_stamina")

    player:spend_stamina(5)
    player:move_to(target)
end
commands["rh:command.travel"] = handle_travel
commands.travel = handle_travel

local function handle_wait_day()
    player:require_location(tavern)
    player:add_stamina(10)
end
commands["time.wait_day"] = handle_wait_day

local function handle_work()
    player:require_location(tavern)
    player:require_stamina(6, "work.insufficient_stamina")

    player:spend_stamina(2)
    player:add_gold(10)
end
commands["work.do_work"] = handle_work

commands.buy = function(item)
    player:require_location(market)
    player:require_gold(item.price, "shop.insufficient_gold")

    player:spend_gold(item.price)
    player:add_item(item)
end

-- Semantic action bindings (TSL-11, TSL-12, TSL-13)
actions["textsystem:action.location.travel"] = "rh:command.travel"
actions["rh:action.buy_sword"] = { command = "buy", args = { item = "rh:item.weapon.iron_sword" } }
actions["rh:action.buy_armor"] = { command = "buy", args = { item = "rh:item.armor.leather_armor" } }
actions["rh:action.wait_day"] = "time.wait_day"
actions["rh:action.do_work"] = "work.do_work"

