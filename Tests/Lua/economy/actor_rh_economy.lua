-- RH Character Actor Economy Methods Specification
-- Verifies is_player, is_npc, get_gold, add_gold on rh character hero.

local mutation_window = require("core:module.runtime.mutation_window")

return {
    rh_actor_decorators_provide_economy_methods = function()
        if game and game.instances and game.instances.actors then
            mutation_window.execute_in_window(function()
                local player = game.instances.actors.player()
                if not player and game.instances.actors.create and game.state and game.state.meta then
                    local hero = game.instances.actors.create("rh:actor.character.hero", { gold = 42 })
                    game.state.meta.player_actor_id = hero.instance_id
                    player = hero
                end

                if player then
                    assert(player.is_player() == true, "player actor is_player() must be true")
                    assert(player.is_npc() == false, "player actor is_npc() must be false")
                    assert(type(player.get_gold) == "function", "player actor must have get_gold method")
                    assert(type(player.add_gold) == "function", "player actor must have add_gold method")
                    assert(player.get_gold() == (player.gold or 0), "get_gold() must return gold")
                end
            end)
        end
    end,
}
