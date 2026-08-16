local M = {
    id = "core:module.gameplay.root",
}

local function handle_actor_reward(request)
    local args = request.args or {}
    local actor_id = args.actor_id
    if not actor_id or actor_id == "" then
        if game and game.state and game.state.meta then
            actor_id = game.state.meta.player_actor_id
        end
    end

    if not actor_id or type(actor_id) ~= "string" then
        return {
            ok = false,
            error = {
                code = "core:error.actor.actor_not_specified",
                message = "Actor ID must be specified or player actor must exist in state.meta",
            },
        }
    end

    local reg = game and game.instances and game.instances.actors
    if not reg then
        return {
            ok = false,
            error = {
                code = "core:error.actor.registry_not_available",
                message = "Actor registry is not available",
            },
        }
    end

    local actor = reg.get(actor_id)
    if not actor then
        return {
            ok = false,
            error = {
                code = "core:error.actor.not_found",
                message = "Actor with id '" .. tostring(actor_id) .. "' not found",
            },
        }
    end

    local amount = args.gold or args.amount
    if amount == nil or type(amount) ~= "number" or math.type(amount) ~= "integer" or amount < 0 then
        return {
            ok = false,
            error = {
                code = "core:error.actor.invalid_reward_amount",
                message = "Reward gold amount must be a non-negative integer",
            },
        }
    end

    return actor.add_gold(amount)
end

local function handle_location_travel(request)
    local location_service = game and game.services and game.services.get and game.services.get("core:service.location")
    if not location_service then
        return {
            ok = false,
            error = {
                code = "core:error.service.not_found",
                params = { service_id = "core:service.location" },
            },
        }
    end

    local args = request.args or {}
    local target_location_id = args.target_location_id or args.location_id
    return location_service.travel(target_location_id)
end

function M.register(_ctx)
    if not game or not game.commands or not game.commands.handlers then
        return
    end

    game.commands.handlers.register("core:command.actor.reward", handle_actor_reward)
    game.commands.handlers.register("core:command.location.travel", handle_location_travel)
end

return M
