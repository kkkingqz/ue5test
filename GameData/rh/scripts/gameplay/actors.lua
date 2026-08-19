-- Actor domain methods for rh package (ADR-0031, ADR-0032)
-- Declarative field contracts and clean domain methods without raw state or allocator access.

instances.register_kind("item")

Actor.gold = field.non_negative_integer()
Actor.stamina = field.non_negative_integer()

local function validate_amount(amount, err_type)
    if type(amount) ~= "number" or math.type(amount) ~= "integer" or amount < 0 then
        error(err_type .. ": amount must be a non-negative integer, got " .. tostring(amount), 3)
    end
end

function Actor:get_gold()
    return self.gold or 0
end

function Actor:require_gold(amount, opt_key)
    validate_amount(amount, "InvalidGoldAmount")
    local current = self:get_gold()
    if current < amount then
        fail(opt_key or "economy.insufficient_gold", {
            current_gold = current,
            required_gold = amount,
        })
    end
end

function Actor:spend_gold(amount)
    validate_amount(amount, "InvalidGoldAmount")
    local current = self:get_gold()
    if current < amount then
        error("PreconditionNotChecked: cannot spend " .. tostring(amount) .. " gold (current: " .. tostring(current) .. "). Check player:require_gold(" .. tostring(amount) .. ") before spending.", 2)
    end
    self.gold = current - amount
end

function Actor:add_gold(amount)
    validate_amount(amount, "InvalidGoldAmount")
    self.gold = self:get_gold() + amount
end

function Actor:get_stamina()
    return self.stamina or 0
end

function Actor:require_stamina(amount, opt_key)
    validate_amount(amount, "InvalidStaminaAmount")
    local current = self:get_stamina()
    if current < amount then
        fail(opt_key or "economy.insufficient_stamina", {
            current_stamina = current,
            required_stamina = amount,
        })
    end
end

function Actor:spend_stamina(amount)
    validate_amount(amount, "InvalidStaminaAmount")
    local current = self:get_stamina()
    if current < amount then
        error("PreconditionNotChecked: cannot spend " .. tostring(amount) .. " stamina (current: " .. tostring(current) .. "). Check player:require_stamina(" .. tostring(amount) .. ") before spending.", 2)
    end
    self.stamina = current - amount
end

function Actor:add_stamina(amount)
    validate_amount(amount, "InvalidStaminaAmount")
    self.stamina = self:get_stamina() + amount
end

function Actor:add_item(item_def_or_id)
    return instances.create("item", {
        definition = item_def_or_id,
        owner = self,
    })
end
