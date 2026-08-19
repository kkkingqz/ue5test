-- Actor domain methods for rh package (ADR-0031)

local instance_allocator = require("core:module.runtime.instance_allocator")

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

for res_name, config in pairs(RESOURCES) do
    local field = config.field or res_name
    local err_type = config.invalid_amount_error
    local default_fail_key = config.default_fail_key
    local cur_param = config.current_param
    local req_param = config.required_param

    Actor["get_" .. res_name] = function(self)
        local s = self or (game and game.instances and game.instances.actors and game.instances.actors.player and game.instances.actors.player())
        return (s and s[field]) or 0
    end

    Actor["require_" .. res_name] = function(self, amount, opt_key)
        validate_amount(amount, err_type)
        local current = self[field] or 0
        if current < amount then
            fail(opt_key or default_fail_key, {
                [cur_param] = current,
                [req_param] = amount,
            })
        end
    end

    Actor["spend_" .. res_name] = function(self, amount)
        validate_amount(amount, err_type)
        local current = self[field] or 0
        if current < amount then
            error("PreconditionNotChecked: cannot spend " .. tostring(amount) .. " " .. res_name ..
                  " (current: " .. tostring(current) .. "). Check player:require_" .. res_name ..
                  "(" .. tostring(amount) .. ") before spending.", 2)
        end
        self[field] = current - amount
    end

    Actor["add_" .. res_name] = function(self, amount)
        validate_amount(amount, err_type)
        self[field] = (self[field] or 0) + amount
    end
end

function Actor:add_item(item_def_or_id)
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
        owner_id = self.instance_id,
    }
    return item_id
end
