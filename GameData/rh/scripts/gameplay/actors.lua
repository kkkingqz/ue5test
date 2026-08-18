local properties = require("core:module.authoring.properties")
local instance_allocator = require("core:module.runtime.instance_allocator")
local state_validator = require("core:module.runtime.state_validator")

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
        add_item = function(a, b)
            local item_def_or_id = (b ~= nil) and b or a
            local def_id = item_def_or_id
            if type(item_def_or_id) == "table" and (item_def_or_id.id or item_def_or_id.definition_id) then
                def_id = item_def_or_id.id or item_def_or_id.definition_id
            end
            local item_id = instance_allocator.allocate(game.state, "item")
            game.state.item_instances[item_id] = {
                instance_id = item_id,
                definition_id = def_id,
                owner_id = base.instance_id,
            }
            return item_id
        end,
        add_gold = function(a, b)
            local amount = (b ~= nil) and b or a
            if type(amount) ~= "number" or math.type(amount) ~= "integer" or amount < 0 then
                return {
                    ok = false,
                    error = {
                        code = "rh:error.economy.invalid_amount",
                        params = { amount = amount },
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
        spend_gold = function(a, b)
            local amount = (b ~= nil) and b or a
            if type(amount) ~= "number" or math.type(amount) ~= "integer" or amount < 0 then
                return {
                    ok = false,
                    error = {
                        code = "rh:error.economy.invalid_amount",
                        params = { amount = amount },
                    },
                }
            end
            local current = base.gold or 0
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
            base.gold = current - amount
            return {
                ok = true,
                value = {
                    actor_id = base.instance_id,
                    gold = base.gold,
                    amount = amount,
                },
            }
        end,
        get_stamina = function()
            return base.stamina or 0
        end,
        add_stamina = function(a, b)
            local amount = (b ~= nil) and b or a
            if type(amount) ~= "number" or math.type(amount) ~= "integer" or amount < 0 then
                return {
                    ok = false,
                    error = {
                        code = "rh:error.economy.invalid_amount",
                        params = { amount = amount },
                    },
                }
            end
            base.stamina = (base.stamina or 0) + amount
            return {
                ok = true,
                value = {
                    actor_id = base.instance_id,
                    stamina = base.stamina,
                    amount = amount,
                },
            }
        end,
        spend_stamina = function(a, b)
            local amount = (b ~= nil) and b or a
            if type(amount) ~= "number" or math.type(amount) ~= "integer" or amount < 0 then
                return {
                    ok = false,
                    error = {
                        code = "rh:error.economy.invalid_amount",
                        params = { amount = amount },
                    },
                }
            end
            local current = base.stamina or 0
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
            base.stamina = current - amount
            return {
                ok = true,
                value = {
                    actor_id = base.instance_id,
                    stamina = base.stamina,
                    amount = amount,
                },
            }
        end,
        travel = function(a, b)
            local target_location_id = (b ~= nil) and b or a
            local loc_service = game and game.services and game.services.get and game.services.get("core:service.location")
            if loc_service then
                return loc_service.travel(target_location_id)
            end
            base.current_location = target_location_id
            return {
                ok = true,
                value = {
                    to_location_id = target_location_id,
                },
            }
        end,
    }, {
        __index = base,
        __newindex = base,
    })
end

local function location_definition_decorator(def_base)
    return setmetatable({
        is_connected = function(a, b)
            local self_obj = (b ~= nil) and a or def_base
            local target_id = (b ~= nil) and b or a
            if not target_id then return false end
            local canonical_target = target_id
            if type(target_id) == "table" and target_id.id then
                canonical_target = target_id.id
            end
            local connected_ids = self_obj.connected_location_ids or {}
            for _, id in ipairs(connected_ids) do
                if id == canonical_target then
                    return true
                end
            end
            return false
        end,
    }, {
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
