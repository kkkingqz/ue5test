local authoring_context = require("core:module.authoring.context")
local properties = require("core:module.authoring.properties")
local state_validator = require("core:module.runtime.state_validator")

local M = {
    id = "textsystem:module.gameplay.actors",
}

local function actor_decorator(base)
    local wrapper = {}

    function wrapper:is_player()
        return base.discriminator == "player"
    end

    function wrapper:is_npc()
        return base.discriminator == "npc"
    end

    function wrapper:require_location(target, opt_key)
        local target_id = target
        if type(target) == "table" then
            target_id = target.id or target.definition_id
        end
        if type(target_id) ~= "string" or target_id == "" then
            error("InvalidTargetLocation: expected location definition handle or ID, got " .. tostring(target), 2)
        end
        local cur_loc = base.current_location
        local cur_id = cur_loc
        if type(cur_loc) == "table" then
            cur_id = cur_loc.id or cur_loc.definition_id
        end
        if cur_id ~= target_id then
            authoring_context.fail(opt_key or "location.wrong_location", {
                required_location = target_id,
                current_location = cur_id or "",
            })
        end
    end

    function wrapper:move_to(target)
        local target_id = target
        if type(target) == "table" then
            target_id = target.id or target.definition_id
        end
        if type(target_id) ~= "string" or target_id == "" then
            error("InvalidTargetLocation: expected location definition handle or ID, got " .. tostring(target), 2)
        end
        local cur_loc = base.current_location
        local from_id = cur_loc
        if type(cur_loc) == "table" then
            from_id = cur_loc.id or cur_loc.definition_id
        end

        if game and game.events and game.events.enqueue then
            game.events.enqueue({
                event_id = "textsystem:event.location.leave",
                payload = {
                    from_location = from_id,
                    to_location = target_id,
                },
            })
        end

        base.current_location = target_id
        if base.current_location_id ~= nil then
            base.current_location_id = target_id
        end

        if game and game.events and game.events.enqueue then
            game.events.enqueue({
                event_id = "textsystem:event.location.enter",
                payload = {
                    from_location = from_id,
                    to_location = target_id,
                },
            })
        end

        return {
            ok = true,
            value = {
                from_location = from_id,
                to_location = target_id,
            },
        }
    end

    function wrapper:travel(target)
        return self:move_to(target)
    end

    return setmetatable(wrapper, {
        __index = base,
        __newindex = base,
    })
end

local function location_definition_decorator(def_base)
    local wrapper = {}

    function wrapper:is_connected(target)
        local target_id = target
        if type(target) == "table" then
            target_id = target.id or target.definition_id
        end
        if not target_id then return false end
        local connected_ids = def_base.connected_location_ids or (def_base.data and def_base.data.connected_location_ids) or {}
        for _, id in ipairs(connected_ids) do
            if id == target_id then
                return true
            end
        end
        return false
    end

    function wrapper:require_connected(target, opt_key)
        local target_id = target
        if type(target) == "table" then
            target_id = target.id or target.definition_id
        end
        if not self:is_connected(target) then
            authoring_context.fail(opt_key or "travel.not_connected", {
                from_location = def_base.id or def_base.definition_id,
                to_location = target_id or "",
            })
        end
    end

    return setmetatable(wrapper, {
        __index = def_base,
    })
end

function M.register(_ctx)
    if state_validator and state_validator.register_reference_field then
        state_validator.register_reference_field("current_location", "location")
        state_validator.register_reference_field("current_location_id", "location")
    end

    if game and game.instances and game.instances.actors and game.instances.actors.register_type then
        game.instances.actors.register_type("player", actor_decorator)
        game.instances.actors.register_type("npc", actor_decorator)
    end

    if properties and properties.register_definition_type then
        properties.register_definition_type("location", location_definition_decorator)
    end
end

return M
