-- Shop Command Handlers & Validator for rh package (TGS-10)
-- Handles buying sword and armor at the market, deducting gold and allocating item instances.

local instance_allocator = require("core:module.runtime.instance_allocator")

local M = {
    id = "rh:module.gameplay.shop",
}

local ITEMS = {
    ["rh:command.shop.buy_sword"] = {
        definition_id = "rh:item.weapon.iron_sword",
        price = 10,
    },
    ["rh:command.shop.buy_armor"] = {
        definition_id = "rh:item.armor.leather_armor",
        price = 25,
    },
}

local function buy_item_handler(request)
    local item_info = ITEMS[request.command_id]
    if not item_info then
        return {
            ok = false,
            error = {
                code = "core:error.command.unknown",
                params = { command_id = request.command_id },
            },
        }
    end

    local player = game.instances.actors.player()
    if not player then
        return {
            ok = false,
            error = { code = "rh:error.economy.player_not_found" },
        }
    end

    local economy = game.services.get("rh:service.economy")
    if not economy then
        return {
            ok = false,
            error = {
                code = "core:error.service.not_found",
                params = { service_id = "rh:service.economy" },
            },
        }
    end

    local spend_res = economy.spend_gold(item_info.price)
    if not spend_res.ok then
        return spend_res
    end

    local item_instance_id = instance_allocator.allocate(game.state, "item")
    game.state.item_instances[item_instance_id] = {
        instance_id = item_instance_id,
        definition_id = item_info.definition_id,
        owner_id = player.instance_id,
    }

    return {
        ok = true,
        value = {
            instance_id = item_instance_id,
            definition_id = item_info.definition_id,
            price = item_info.price,
            owner_id = player.instance_id,
        },
    }
end

local shop_validator = {
    validate = function(ctx)
        local item_info = ITEMS[ctx.command_id]
        if not item_info then
            return true
        end

        local current_loc = ctx.state and ctx.state.world and ctx.state.world.current_location_id
        if current_loc ~= "rh:location.city.market" then
            return false, {
                code = "rh:error.location.wrong_location",
                params = {
                    required_location_id = "rh:location.city.market",
                    current_location_id = current_loc,
                },
            }
        end

        local player_id = ctx.state and ctx.state.meta and ctx.state.meta.player_actor_id
        local player_state = player_id and ctx.state.actors and ctx.state.actors[player_id]
        local current_gold = player_state and player_state.gold or 0

        if current_gold < item_info.price then
            return false, {
                code = "rh:error.shop.insufficient_gold",
                params = {
                    current_gold = current_gold,
                    required_gold = item_info.price,
                },
            }
        end

        return true
    end,
}

-- TGS-09: the screen is rebuilt after a successful action so the player sees the
-- result. The republish callback is injected by the composition root, keeping
-- presentation out of this module's imports.
local function with_republish(handler, republish)
    if type(republish) ~= "function" then
        return handler
    end
    return function(request)
        local result = handler(request)
        if type(result) ~= "table" or result.ok ~= false then
            republish()
        end
        return result
    end
end

function M.register_handlers(_ctx, republish)
    if game and game.commands and game.commands.validators and game.commands.validators.register then
        game.commands.validators.register("rh:validator.shop.buy_item", shop_validator, { priority = 10 })
    end

    if game and game.commands and game.commands.handlers and game.commands.handlers.register then
        game.commands.handlers.register("rh:command.shop.buy_sword", with_republish(buy_item_handler, republish))
        game.commands.handlers.register("rh:command.shop.buy_armor", with_republish(buy_item_handler, republish))
    end
end

M.ITEMS = ITEMS
M.handler = buy_item_handler
M.validator = shop_validator

return M
