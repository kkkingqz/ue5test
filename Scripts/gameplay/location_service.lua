-- Location Gameplay Service & Validator (GEW-13)
-- Implements the location travel workflow: validator -> handler -> service -> world.
-- Enforces travel preconditions and updates current location in world state.

local stable_id = require("core:module.runtime.stable_id")

local M = {
    id = "core:module.gameplay.location_service",
}

local service = {}

function service.travel(target_location_id)
    if type(target_location_id) ~= "string" or not stable_id.is_kind(target_location_id, "location") then
        return {
            ok = false,
            error = {
                code = "core:error.location.invalid_target",
                params = { target_location_id = target_location_id },
            },
        }
    end

    local world_instance = game and game.instances and game.instances.world and game.instances.world()
    if not world_instance then
        return {
            ok = false,
            error = {
                code = "core:error.world.not_available",
                params = {},
            },
        }
    end

    local from_location_id = world_instance.current_location_id

    -- GEW-14: Enqueue location.leave fact before updating state
    if game and game.events and game.events.enqueue then
        game.events.enqueue({
            event_id = "core:event.location.leave",
            payload = {
                from_location_id = from_location_id,
                to_location_id = target_location_id,
            },
        })
    end

    world_instance.current_location_id = target_location_id

    -- GEW-14: Enqueue location.enter fact after updating state
    if game and game.events and game.events.enqueue then
        game.events.enqueue({
            event_id = "core:event.location.enter",
            payload = {
                from_location_id = from_location_id,
                to_location_id = target_location_id,
            },
        })
    end

    return {
        ok = true,
        value = {
            from_location_id = from_location_id,
            to_location_id = target_location_id,
        },
    }
end

local validator = {
    validate = function(ctx)
        if ctx.command_id ~= "core:command.location.travel" then
            return true
        end

        local payload = ctx.payload or {}
        local target_location_id = payload.target_location_id or payload.location_id

        if type(target_location_id) ~= "string" or not stable_id.is_kind(target_location_id, "location") then
            return false, {
                code = "core:error.location.invalid_target",
                params = { target_location_id = target_location_id },
            }
        end

        if ctx.repository and ctx.repository.get then
            local def = ctx.repository.get(target_location_id)
            if not def then
                return false, {
                    code = "core:error.location.not_found",
                    params = { target_location_id = target_location_id },
                }
            end
        end

        local current_location_id = ctx.state and ctx.state.world and ctx.state.world.current_location_id
        if current_location_id == target_location_id then
            return false, {
                code = "core:error.location.already_at_target",
                params = { target_location_id = target_location_id },
            }
        end

        return true
    end,
}

function M.register(_ctx)
    if game and game.services and game.services.register then
        game.services.register("core:service.location", service)
    end
    if game and game.commands and game.commands.validators and game.commands.validators.register then
        game.commands.validators.register("core:validator.location.travel", validator, { priority = 0 })
    end
end

M.service = service
M.validator = validator

return M
