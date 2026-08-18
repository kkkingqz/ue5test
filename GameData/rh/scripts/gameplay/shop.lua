-- Shop Command Handlers for rh package (DLA-21)
-- Handles buying sword and armor at the market, deducting gold and allocating item instances.

local authoring = require("core:module.authoring.context")
local location_screen = require("rh:module.presentation.location_screen")

local M = authoring.gameplay("rh")
M.id = "rh:module.gameplay.shop"

local ITEMS = {
    ["rh:command.shop.buy_sword"] = {
        item_name = "weapon.iron_sword",
        price = 10,
    },
    ["rh:command.shop.buy_armor"] = {
        item_name = "armor.leather_armor",
        price = 25,
    },
}

local function handle_buy(cmd_id)
    local info = ITEMS[cmd_id]
    if not info then
        return M.fail("command.unknown", { command_id = cmd_id })
    end

    if M.player.current_location_id ~= "rh:location.city.market" then
        return M.fail("location.wrong_location", {
            required_location_id = "rh:location.city.market",
            current_location_id = M.player.current_location_id,
        })
    end

    local item_def = M.def.item(info.item_name)
    local price = info.price or item_def.price or 0

    if M.player.gold < price then
        return M.fail("shop.insufficient_gold", {
            current_gold = M.player.gold,
            required_gold = price,
        })
    end

    M.player:spend_gold(price)
    local instance_id = M.player:add_item(item_def)

    if location_screen and location_screen.build_and_publish_screen then
        location_screen.build_and_publish_screen()
    end

    return {
        ok = true,
        value = {
            instance_id = instance_id,
            definition_id = item_def.id,
            price = price,
            owner_id = M.player.instance_id,
        },
    }
end

M.commands["shop.buy_sword"] = function()
    return handle_buy("rh:command.shop.buy_sword")
end

M.commands["shop.buy_armor"] = function()
    return handle_buy("rh:command.shop.buy_armor")
end

return M
