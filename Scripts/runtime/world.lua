-- World Domain Object (GEW-04)
-- Singleton disposable wrapper over state.world, mirroring the Actor model
-- (actor_registry.lua) but without instance_id/definition_id/discriminator
-- identity fields: there is exactly one world, not a registry of many.

local M = {
    id = "core:module.runtime.world",
}

local function wrap_world(world_state)
    if type(world_state) ~= "table" then
        return nil
    end

    local wrapper = {}

    local domain_methods = {
        get_state = function() return world_state end,
    }

    local mt = {
        __index = function(_, k)
            if domain_methods[k] ~= nil then
                return domain_methods[k]
            end
            if k == "current_location" then
                local player = game and game.instances and game.instances.actors and game.instances.actors.player and game.instances.actors.player()
                if player then
                    if player.current_location ~= nil then
                        return player.current_location
                    elseif player.current_location_id ~= nil then
                        return player.current_location_id
                    end
                end
                return nil
            end
            if k == "current_location_id" then
                local player = game and game.instances and game.instances.actors and game.instances.actors.player and game.instances.actors.player()
                if player then
                    if player.current_location_id ~= nil then
                        return player.current_location_id
                    elseif player.current_location ~= nil then
                        if type(player.current_location) == "table" and player.current_location.id then
                            return player.current_location.id
                        end
                        return player.current_location
                    end
                end
                return nil
            end
            return world_state[k]
        end,
        __newindex = function(_, k, v)
            if k == "current_location" or k == "current_location_id" then
                error("WorldLocationReadOnly: world." .. k .. " is a read-only accessor delegating to the player actor; mutate location on player instead", 2)
            end
            -- world_state is itself the mutation_window-guarded proxy for
            -- state.world (game.state access already wraps nested tables),
            -- so this delegates window enforcement rather than duplicating it.
            world_state[k] = v
        end,
        __tostring = function(_)
            return "WorldWrapper"
        end,
    }
    setmetatable(wrapper, mt)
    return wrapper
end

M.wrap = wrap_world

-- Disposable accessor: returns a fresh wrapper on every call, never the
-- same table twice, and caches nothing (GEW-04 "wrapper disposable").
local function get_world()
    if not game or not game.state or not game.state.world then
        return nil
    end
    return wrap_world(game.state.world)
end

M.get_world = get_world

function M.register(_ctx)
    if not game.instances then
        game.instances = {}
    end
    game.instances.world = get_world
end

return M
