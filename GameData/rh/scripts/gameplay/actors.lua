local M = {
    id = "rh:module.gameplay.actors",
}

local function actor_decorator(base)
    return setmetatable({
        is_player = function()
            return base.discriminator == "player"
        end,
        is_npc = function()
            return base.discriminator == "npc"
        end,
        get_gold = function()
            return base.gold or 0
        end,
        add_gold = function(amount)
            if type(amount) ~= "number" or math.type(amount) ~= "integer" or amount < 0 then
                return {
                    ok = false,
                    error = {
                        code = "core:error.actor.invalid_reward_amount",
                        message = "Reward amount must be a non-negative integer",
                    },
                }
            end
            base.gold = (base.gold or 0) + amount
            return {
                ok = true,
                value = {
                    actor_id = base.instance_id,
                    gold = base.gold,
                    amount = amount,
                },
            }
        end,
    }, {
        __index = base,
        __newindex = base,
    })
end

function M.register(_ctx)
    if not game or not game.instances or not game.instances.actors or not game.instances.actors.register_type then
        return
    end

    game.instances.actors.register_type("player", actor_decorator)
    game.instances.actors.register_type("npc", actor_decorator)
end

return M
