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

local function validate_actor_receiver(receiver, method_name)
    if not receiver or type(receiver) ~= "table" then
        error("MissingReceiver: Actor:" .. tostring(method_name) .. " requires an Actor receiver", 3)
    end
end

local function get_item_definition_id(item)
    if type(item) == "string" then
        return item
    elseif type(item) == "table" then
        return item.definition_id or item.id or (item.definition and (type(item.definition) == "table" and (item.definition.id or item.definition.definition_id) or item.definition))
    end
    return nil
end

function Actor:has_item(item)
    validate_actor_receiver(self, "has_item")
    local def_id = get_item_definition_id(item)
    if not def_id then return false end
    if game and game.state and game.state.item_instances then
        for _, inst in pairs(game.state.item_instances) do
            if inst.owner_id == self.instance_id and (inst.definition_id == def_id or inst.instance_id == def_id) then
                return true
            end
        end
    end
    return false
end

function Actor:require_item(item, opt_key)
    validate_actor_receiver(self, "require_item")
    local def_id = get_item_definition_id(item)
    if not self:has_item(item) then
        fail(opt_key or "trade.item_not_available", {
            item = def_id or tostring(item),
            owner = self.instance_id,
        })
    end
end

function Actor:take_item(item)
    validate_actor_receiver(self, "take_item")
    local def_id = get_item_definition_id(item)
    local found_inst = nil
    if game and game.state and game.state.item_instances then
        for _, inst in pairs(game.state.item_instances) do
            if inst.owner_id == self.instance_id and (inst.definition_id == def_id or inst.instance_id == def_id) then
                found_inst = inst
                break
            end
        end
    end
    if not found_inst then
        error("PreconditionNotChecked: cannot take item '" .. tostring(def_id) .. "' (actor '" .. tostring(self.instance_id) .. "' does not own it). Check actor:require_item before take_item.", 2)
    end
    found_inst.owner_id = nil
    return found_inst
end

function Actor:receive_item(instance)
    validate_actor_receiver(self, "receive_item")
    local inst_id = nil
    if type(instance) == "string" then
        inst_id = instance
    elseif type(instance) == "table" then
        inst_id = instance.instance_id or instance.id
    end
    if not inst_id or not (game and game.state and game.state.item_instances and game.state.item_instances[inst_id]) then
        error("InvalidItemInstance: cannot receive invalid item instance " .. tostring(instance), 2)
    end
    local item_state = game.state.item_instances[inst_id]
    item_state.owner_id = self.instance_id
    return inst_id
end

function Actor:add_item(item_def_or_id)
    return instances.create("item", {
        definition = item_def_or_id,
        owner = self,
    })
end
