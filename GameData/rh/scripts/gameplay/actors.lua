-- Gameplay Actor Extensions for rh package (ADR-0028, TSL-18, TSL-19)
-- Provides domain methods for character actors: resource management (gold, stamina)
-- and item instance allocation.

local authoring_context = require("core:module.authoring.context")
local instance_allocator = require("core:module.runtime.instance_allocator")

local M = {
    id = "rh:module.gameplay.actors",
}

-- Resource descriptor table: defines validation rules, error codes, and parameter names.
-- Adding a new resource requires only adding an entry here (TSL-19).
local RESOURCES = {
    gold = {
        field = "gold",
        invalid_amount_error = "InvalidGoldAmount",
        default_fail_key = "economy.insufficient_gold",
        current_param = "current_gold",
        required_param = "required_gold",
    },
    stamina = {
        field = "stamina",
        invalid_amount_error = "InvalidStaminaAmount",
        default_fail_key = "economy.insufficient_stamina",
        current_param = "current_stamina",
        required_param = "required_stamina",
    },
}

local function validate_amount(amount, err_type)
    if type(amount) ~= "number" or math.type(amount) ~= "integer" or amount < 0 then
        error(err_type .. ": amount must be a non-negative integer, got " .. tostring(amount), 3)
    end
end

local function attach_resource_methods(wrapper, base, res_name, config)
    local field = config.field or res_name
    local err_type = config.invalid_amount_error
    local default_fail_key = config.default_fail_key
    local cur_param = config.current_param
    local req_param = config.required_param

    wrapper["get_" .. res_name] = function(_self)
        return base[field] or 0
    end

    wrapper["require_" .. res_name] = function(_self, amount, opt_key)
        validate_amount(amount, err_type)
        local current = base[field] or 0
        if current < amount then
            authoring_context.fail(opt_key or default_fail_key, {
                [cur_param] = current,
                [req_param] = amount,
            })
        end
    end

    wrapper["spend_" .. res_name] = function(_self, amount)
        validate_amount(amount, err_type)
        local current = base[field] or 0
        if current < amount then
            error("PreconditionNotChecked: cannot spend " .. tostring(amount) .. " " .. res_name ..
                  " (current: " .. tostring(current) .. "). Check player:require_" .. res_name ..
                  "(" .. tostring(amount) .. ") before spending.", 2)
        end
        base[field] = current - amount
    end

    wrapper["add_" .. res_name] = function(_self, amount)
        validate_amount(amount, err_type)
        base[field] = (base[field] or 0) + amount
    end
end

local function actor_decorator(base)
    local wrapper = {}

    function wrapper:is_player()
        return base.discriminator == "player"
    end

    function wrapper:is_npc()
        return base.discriminator == "npc"
    end

    -- Generate resource management methods from configuration table
    for res_name, config in pairs(RESOURCES) do
        attach_resource_methods(wrapper, base, res_name, config)
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

function M.register(_ctx)
    if game and game.instances and game.instances.actors and game.instances.actors.register_type then
        game.instances.actors.register_type("player", actor_decorator)
        game.instances.actors.register_type("npc", actor_decorator)
    end
end

return M
