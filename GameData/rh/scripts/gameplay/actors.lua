local authoring_context = require("core:module.authoring.context")
local properties = require("core:module.authoring.properties")
local instance_allocator = require("core:module.runtime.instance_allocator")
local state_validator = require("core:module.runtime.state_validator")

local M = {
    id = "rh:module.gameplay.actors",
}

local function actor_decorator(base)
    local wrapper = {}

    function wrapper:is_player()
        return base.discriminator == "player"
    end

    function wrapper:is_npc()
        return base.discriminator == "npc"
    end

    function wrapper:get_gold()
        return base.gold or 0
    end

    function wrapper:require_gold(amount, opt_key)
        if type(amount) ~= "number" or math.type(amount) ~= "integer" or amount < 0 then
            error("InvalidGoldAmount: amount must be a non-negative integer, got " .. tostring(amount), 2)
        end
        local current = base.gold or 0
        if current < amount then
            authoring_context.fail(opt_key or "economy.insufficient_gold", {
                current_gold = current,
                required_gold = amount,
            })
        end
    end

    function wrapper:spend_gold(amount)
        if type(amount) ~= "number" or math.type(amount) ~= "integer" or amount < 0 then
            error("InvalidGoldAmount: amount must be a non-negative integer, got " .. tostring(amount), 2)
        end
        local current = base.gold or 0
        if current < amount then
            error("PreconditionNotChecked: cannot spend " .. tostring(amount) .. " gold (current: " .. tostring(current) .. "). Check player:require_gold(" .. tostring(amount) .. ") before spending.", 2)
        end
        base.gold = current - amount
    end

    function wrapper:add_gold(amount)
        if type(amount) ~= "number" or math.type(amount) ~= "integer" or amount < 0 then
            error("InvalidGoldAmount: amount must be a non-negative integer, got " .. tostring(amount), 2)
        end
        base.gold = (base.gold or 0) + amount
    end

    function wrapper:get_stamina()
        return base.stamina or 0
    end

    function wrapper:require_stamina(amount, opt_key)
        if type(amount) ~= "number" or math.type(amount) ~= "integer" or amount < 0 then
            error("InvalidStaminaAmount: amount must be a non-negative integer, got " .. tostring(amount), 2)
        end
        local current = base.stamina or 0
        if current < amount then
            authoring_context.fail(opt_key or "economy.insufficient_stamina", {
                current_stamina = current,
                required_stamina = amount,
            })
        end
    end

    function wrapper:spend_stamina(amount)
        if type(amount) ~= "number" or math.type(amount) ~= "integer" or amount < 0 then
            error("InvalidStaminaAmount: amount must be a non-negative integer, got " .. tostring(amount), 2)
        end
        local current = base.stamina or 0
        if current < amount then
            error("PreconditionNotChecked: cannot spend " .. tostring(amount) .. " stamina (current: " .. tostring(current) .. "). Check player:require_stamina(" .. tostring(amount) .. ") before spending.", 2)
        end
        base.stamina = current - amount
    end

    function wrapper:add_stamina(amount)
        if type(amount) ~= "number" or math.type(amount) ~= "integer" or amount < 0 then
            error("InvalidStaminaAmount: amount must be a non-negative integer, got " .. tostring(amount), 2)
        end
        base.stamina = (base.stamina or 0) + amount
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
                required_location_id = target_id,
                current_location_id = cur_id or "",
            })
        end
    end

    function wrapper:travel(target)
        local target_id = target
        if type(target) == "table" then
            target_id = target.id or target.definition_id
        end
        if type(target_id) ~= "string" or target_id == "" then
            error("InvalidTargetLocation: expected location definition handle or ID, got " .. tostring(target), 2)
        end
        local loc_service = game and game.services and game.services.get and game.services.get("core:service.location")
        if loc_service then
            return loc_service.travel(target_id)
        end
        base.current_location = target_id
    end

    function wrapper:add_item(item_def_or_id)
        local def_id = item_def_or_id
        if type(item_def_or_id) == "table" and (item_def_or_id.id or item_def_or_id.definition_id) then
            def_id = item_def_or_id.id or item_def_or_id.definition_id
        end
        if type(def_id) ~= "string" or def_id == "" then
            error("InvalidItemDefinition: expected item definition handle or ID, got " .. tostring(item_def_or_id), 2)
        end
        local item_id = instance_allocator.allocate(game.state, "item")
        game.state.item_instances[item_id] = {
            instance_id = item_id,
            definition_id = def_id,
            owner_id = base.instance_id,
        }
        return item_id
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
        local connected_ids = def_base.connected_location_ids or {}
        for _, id in ipairs(connected_ids) do
            if id == target_id then
                return true
            end
        end
        return false
    end

    function wrapper:require_connected(target)
        local target_id = target
        if type(target) == "table" then
            target_id = target.id or target.definition_id
        end
        if not self:is_connected(target) then
            authoring_context.fail("travel.not_connected", {
                from_location_id = def_base.id or def_base.definition_id,
                to_location_id = target_id or "",
            })
        end
    end

    return setmetatable(wrapper, {
        __index = def_base,
    })
end

function M.register(_ctx)
    if state_validator and state_validator.register_reference_field then
        state_validator.register_reference_field("current_location_id", "location")
        state_validator.register_reference_field("current_location", "location")
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
