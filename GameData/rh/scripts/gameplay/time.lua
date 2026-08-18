-- Time Command Handlers & Validator for rh package (TGS-10)
-- Handles resting / waiting 1 day in the tavern, restoring stamina.

local M = {
    id = "rh:module.gameplay.time",
}

local function wait_day_handler(request)
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

    local add_res = economy.add_stamina(10)
    if not add_res.ok then
        return add_res
    end

    return {
        ok = true,
        value = {
            stamina_gained = 10,
            current_stamina = player.stamina,
        },
    }
end

local time_validator = {
    validate = function(ctx)
        if ctx.command_id ~= "rh:command.time.wait_day" then
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
        game.commands.validators.register("rh:validator.time.wait_day", time_validator, { priority = 10 })
    end

    if game and game.commands and game.commands.handlers and game.commands.handlers.register then
        game.commands.handlers.register("rh:command.time.wait_day", with_republish(wait_day_handler, republish))
    end
end

M.handler = wait_day_handler
M.validator = time_validator

return M
