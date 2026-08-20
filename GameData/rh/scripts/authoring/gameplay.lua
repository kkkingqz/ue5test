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

local function handle_start_game()
    local hero = instances.create("actor", {
        definition = def("character.hero"),
        current_location = tavern,
        gold = 50,
        stamina = 20,
        is_player = true,
    })

    local merchant_inst = instances.create("actor", {
        definition = def("npc.merchant"),
        current_location = market,
        gold = 100,
        stamina = 50,
    })
    merchant_inst:add_item("rh:item.weapon.iron_sword")
    merchant_inst:add_item("rh:item.armor.leather_armor")

    return {
        player = hero,
        merchant = merchant_inst,
    }
end
commands.start_game = handle_start_game
commands["game.start"] = handle_start_game

services.trade = {
    buy = function(buyer, seller, item)
        local price = nil
        if type(item) == "table" and item.price ~= nil then
            price = item.price
        elseif type(item) == "string" and game and game.repository and game.repository.get then
            local item_def = game.repository.get(item)
            if item_def and item_def.data and item_def.data.price then
                price = item_def.data.price
            end
        end
        if price == nil then
            error("InvalidItemPrice: cannot determine price for item " .. tostring(item), 2)
        end

        buyer:require_gold(price, "shop.insufficient_gold")
        seller:require_item(item, "trade.item_not_available")

        buyer:spend_gold(price)
        seller:add_gold(price)

        local instance = seller:take_item(item)
        buyer:receive_item(instance)

        emit("trade.completed", {
            buyer = buyer,
            seller = seller,
            item = item,
            price = price,
        })
        return true
    end,
}

commands.request_buy = function(item)
    player:require_location(market)
    local merchant = actor("npc.merchant")
    if type(item) == "table" and item.item ~= nil and item.price == nil and item.id == nil then
        item = item.item
    end

    local price = 10
    local title_text = text("shop.confirm.title")
    local desc_text = text("shop.confirm.sword")
    if item == "rh:item.armor.leather_armor" then
        price = 25
        desc_text = text("shop.confirm.armor")
    end

    player:require_gold(price, "shop.insufficient_gold")
    merchant:require_item(item, "trade.item_not_available")

    show_modal("confirm_purchase", {
        template = "core:screen.modal_confirm",
        title = title_text,
        content = desc_text,
        buttons = {
            button(text("action.confirm"), action("rh:command.buy", item), "confirm"),
            button(text("action.cancel"), action("rh:command.cancel_buy"), "cancel"),
        },
    })
end

local function handle_buy(item)
    player:require_location(market)
    local merchant = actor("npc.merchant")
    if type(item) == "table" and item.item ~= nil and item.price == nil and item.id == nil then
        item = item.item
    end
    services.trade.buy(player, merchant, item)
    close_modal("confirm_purchase")
end

commands.buy = handle_buy
commands.confirm_buy = handle_buy
commands.cancel_buy = function()
    close_modal("confirm_purchase")
end

-- Semantic action bindings (TSL-11, TSL-12, TSL-13)
actions["textsystem:action.location.travel"] = "rh:command.travel"
actions["rh:action.buy_sword"] = { command = "request_buy", args = { item = "rh:item.weapon.iron_sword" } }
actions["rh:action.buy_armor"] = { command = "request_buy", args = { item = "rh:item.armor.leather_armor" } }
actions["rh:action.confirm_buy"] = "rh:command.buy"
actions["rh:action.cancel_buy"] = "rh:command.cancel_buy"
actions["rh:action.wait_day"] = "time.wait_day"
actions["rh:action.do_work"] = "work.do_work"


