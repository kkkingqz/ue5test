-- Economy Gameplay Service for rh package (TGS-03)
-- Manages player gold and stamina transactions with strict validation and failure rollback.

local M = {
    id = "rh:module.services.economy",
}

local service = {}

local function is_valid_amount(amount)
    return type(amount) == "number" and math.type(amount) == "integer" and amount >= 0
end

local function get_player_actor()
    if not game or not game.instances or not game.instances.actors or not game.instances.actors.player then
        return nil
    end
    return game.instances.actors.player()
end

function service.get_gold()
    local player = get_player_actor()
    if not player then
        return 0
    end
    return player.gold or 0
end

function service.get_stamina()
    local player = get_player_actor()
    if not player then
        return 0
    end
    return player.stamina or 0
end

function service.add_gold(amount)
    if not is_valid_amount(amount) then
        return {
            ok = false,
            error = {
                code = "rh:error.economy.invalid_amount",
                params = { amount = amount },
            },
        }
    end

    local player = get_player_actor()
    if not player then
        return {
            ok = false,
            error = {
                code = "rh:error.economy.player_not_found",
                params = {},
            },
        }
    end

    local current = player.gold or 0
    player.gold = current + amount
    return {
        ok = true,
        value = {
            gold = player.gold,
            amount = amount,
        },
    }
end

function service.spend_gold(amount)
    if not is_valid_amount(amount) then
        return {
            ok = false,
            error = {
                code = "rh:error.economy.invalid_amount",
                params = { amount = amount },
            },
        }
    end

    local player = get_player_actor()
    if not player then
        return {
            ok = false,
            error = {
                code = "rh:error.economy.player_not_found",
                params = {},
            },
        }
    end

    local current = player.gold or 0
    if current < amount then
        return {
            ok = false,
            error = {
                code = "rh:error.economy.insufficient_gold",
                params = {
                    current_gold = current,
                    required_gold = amount,
                },
            },
        }
    end

    player.gold = current - amount
    return {
        ok = true,
        value = {
            gold = player.gold,
            amount = amount,
        },
    }
end

function service.add_stamina(amount)
    if not is_valid_amount(amount) then
        return {
            ok = false,
            error = {
                code = "rh:error.economy.invalid_amount",
                params = { amount = amount },
            },
        }
    end

    local player = get_player_actor()
    if not player then
        return {
            ok = false,
            error = {
                code = "rh:error.economy.player_not_found",
                params = {},
            },
        }
    end

    local current = player.stamina or 0
    player.stamina = current + amount
    return {
        ok = true,
        value = {
            stamina = player.stamina,
            amount = amount,
        },
    }
end

function service.spend_stamina(amount)
    if not is_valid_amount(amount) then
        return {
            ok = false,
            error = {
                code = "rh:error.economy.invalid_amount",
                params = { amount = amount },
            },
        }
    end

    local player = get_player_actor()
    if not player then
        return {
            ok = false,
            error = {
                code = "rh:error.economy.player_not_found",
                params = {},
            },
        }
    end

    local current = player.stamina or 0
    if current < amount then
        return {
            ok = false,
            error = {
                code = "rh:error.economy.insufficient_stamina",
                params = {
                    current_stamina = current,
                    required_stamina = amount,
                },
            },
        }
    end

    player.stamina = current - amount
    return {
        ok = true,
        value = {
            stamina = player.stamina,
            amount = amount,
        },
    }
end

function M.register(_ctx)
    if game and game.services and game.services.register then
        game.services.register("rh:service.economy", service)
    end
end

M.service = service

return M
