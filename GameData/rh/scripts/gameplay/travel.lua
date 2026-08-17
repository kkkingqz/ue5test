-- Travel Command Handler Override & Location Validator for rh package (TGS-06, TGS-07)
-- Overrides core:command.location.travel to deduct stamina on travel in a single mutation window.
-- Enforces stamina and map connectivity preconditions in read-only validator.

local stable_id = require("core:module.runtime.stable_id")
local state_validator = require("core:module.runtime.state_validator")

local M = {
    id = "rh:module.gameplay.travel",
}

local TRAVEL_STAMINA_COST = 5

local function travel_handler(request)
    local payload = request.args or {}
    local target_location_id = payload.target_location_id or payload.location_id

    local economy = game and game.services and game.services.get and game.services.get("rh:service.economy")
    if not economy then
        return {
            ok = false,
            error = {
                code = "core:error.service.not_found",
                params = { service_id = "rh:service.economy" },
            },
        }
    end

    local spend_res = economy.spend_stamina(TRAVEL_STAMINA_COST)
    if not spend_res.ok then
        return spend_res
    end

    local location_service = game.services.get("core:service.location")
    if not location_service then
        return {
            ok = false,
            error = {
                code = "core:error.service.not_found",
                params = { service_id = "core:service.location" },
            },
        }
    end

    return location_service.travel(target_location_id)
end

local travel_validator = {
    validate = function(ctx)
        if ctx.command_id ~= "core:command.location.travel" then
            return true
        end

        local payload = ctx.payload or {}
        local target_location_id = payload.target_location_id or payload.location_id

        if type(target_location_id) ~= "string" or not stable_id.is_kind(target_location_id, "location") then
            return true -- Handled by core validator
        end

        -- Check stamina requirement
        local player = game and game.instances and game.instances.actors and game.instances.actors.player and game.instances.actors.player()
        local current_stamina = player and (player.stamina or 0) or 0
        if current_stamina < TRAVEL_STAMINA_COST then
            return false, {
                code = "rh:error.travel.insufficient_stamina",
                params = {
                    required_stamina = TRAVEL_STAMINA_COST,
                    current_stamina = current_stamina,
                },
            }
        end

        -- Check connectivity
        local world = game and game.instances and game.instances.world and game.instances.world()
        local current_location_id = world and world.current_location_id

        if current_location_id and game and game.repository and game.repository.get then
            local current_def = game.repository.get(current_location_id)
            local data = current_def and (current_def.data or current_def)
            local connected_ids = data and data.connected_location_ids
            local is_connected = false

            if type(connected_ids) == "table" then
                for _, id in ipairs(connected_ids) do
                    if id == target_location_id then
                        is_connected = true
                        break
                    end
                end
            end

            if not is_connected then
                return false, {
                    code = "rh:error.travel.not_connected",
                    params = {
                        from_location_id = current_location_id,
                        to_location_id = target_location_id,
                    },
                }
            end
        end

        return true
    end,
}

function M.register_handlers(_ctx)
    if state_validator and state_validator.register_reference_field then
        state_validator.register_reference_field("current_location_id", "location")
    end

    if game and game.commands and game.commands.validators and game.commands.validators.register then
        game.commands.validators.register("rh:validator.location.travel", travel_validator, { priority = 10 })
    end

    if game and game.commands and game.commands.handlers and game.commands.handlers.register then
        game.commands.handlers.register("core:command.location.travel", travel_handler, { override = true })
    end
end

M.handler = travel_handler
M.validator = travel_validator

return M
