-- Base gameplay root of the core package.
--
-- Holds only the default handler of a command whose mechanism belongs to the
-- engine. Game rules — cost, preconditions, rewards — belong to the gameplay
-- package, which replaces this module and calls base.register (ADR-0026).

local M = {
    id = "core:module.gameplay.root",
}

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

    if not (game.commands.handlers.exists and game.commands.handlers.exists("core:command.location.travel")) then
        game.commands.handlers.register("core:command.location.travel", handle_location_travel)
    end
end

return M
