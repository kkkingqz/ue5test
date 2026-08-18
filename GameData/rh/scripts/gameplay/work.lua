-- Work Command Handlers & Validator for rh package (TGS-10)
-- Handles working in the tavern (+10 gold, -2 stamina), requiring stamina > 5.

local M = {
    id = "rh:module.gameplay.work",
}

local function do_work_handler(request)
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

    local spend_res = economy.spend_stamina(2)
    if not spend_res.ok then
        return spend_res
    end

    local add_res = economy.add_gold(10)
    if not add_res.ok then
        return add_res
    end

    return {
        ok = true,
        value = {
            gold_gained = 10,
            stamina_spent = 2,
            current_gold = player.gold,
            current_stamina = player.stamina,
        },
    }
end

local work_validator = {
    validate = function(ctx)
        if ctx.command_id ~= "rh:command.work.do_work" then
            return true
        end

        local player_id = ctx.state and ctx.state.meta and ctx.state.meta.player_actor_id
        local player_state = player_id and ctx.state.actors and ctx.state.actors[player_id]
        local current_loc = player_state and (player_state.current_location_id or player_state.current_location)
            or (ctx.state and ctx.state.world and ctx.state.world.current_location_id)

        if current_loc ~= "rh:location.city.tavern" then
            return false, {
                code = "rh:error.location.wrong_location",
                params = {
                    required_location_id = "rh:location.city.tavern",
                    current_location_id = current_loc,
                },
            }
        end

        local current_stamina = player_state and player_state.stamina or 0

        if current_stamina <= 5 then
            return false, {
                code = "rh:error.work.insufficient_stamina",
                params = {
                    current_stamina = current_stamina,
                    required_stamina = 6,
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
        game.commands.validators.register("rh:validator.work.do_work", work_validator, { priority = 10 })
    end

    if game and game.commands and game.commands.handlers and game.commands.handlers.register then
        game.commands.handlers.register("rh:command.work.do_work", with_republish(do_work_handler, republish))
    end
end

M.handler = do_work_handler
M.validator = work_validator

return M
