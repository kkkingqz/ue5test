-- Actor and Location domain methods for textsystem package (ADR-0031, EEH-06, EEH-07)

local state_validator = require("core:module.runtime.state_validator")
if state_validator and state_validator.register_reference_field then
    state_validator.register_reference_field("current_location", "location")
    state_validator.register_reference_field("current_location_id", "location")
end

local function validate_actor_receiver(receiver, method_name)
    if not receiver or type(receiver) ~= "table" then
        error("MissingReceiver: Actor:" .. tostring(method_name) .. " requires an Actor receiver", 3)
    end
end

local function validate_location_receiver(receiver, method_name)
    if not receiver or type(receiver) ~= "table" then
        error("MissingReceiver: Location:" .. tostring(method_name) .. " requires a Location receiver", 3)
    end
end

function Actor:is_player()
    validate_actor_receiver(self, "is_player")
    return self.discriminator == "player"
end

function Actor:is_npc()
    validate_actor_receiver(self, "is_npc")
    return self.discriminator == "npc"
end

function Actor:require_location(target, opt_key)
    validate_actor_receiver(self, "require_location")
    local target_id = target
    if type(target) == "table" then
        target_id = target.id or target.definition_id
    end
    if type(target_id) ~= "string" or target_id == "" then
        error("InvalidTargetLocation: expected location definition handle or ID, got " .. tostring(target), 2)
    end
    local cur_loc = self.current_location
    local cur_id = cur_loc
    if type(cur_loc) == "table" then
        cur_id = cur_loc.id or cur_loc.definition_id
    end
    if cur_id ~= target_id then
        fail(opt_key or "location.wrong_location", {
            required_location = target_id,
            current_location = cur_id or "",
        })
    end
end

function Actor:move_to(target)
    validate_actor_receiver(self, "move_to")
    local target_id = target
    if type(target) == "table" then
        target_id = target.id or target.definition_id
    end
    if type(target_id) ~= "string" or target_id == "" then
        error("InvalidTargetLocation: expected location definition handle or ID, got " .. tostring(target), 2)
    end
    local cur_loc = self.current_location
    local from_id = cur_loc
    if type(cur_loc) == "table" then
        from_id = cur_loc.id or cur_loc.definition_id
    end

    emit("textsystem:event.location.leave", {
        from_location = from_id,
        to_location = target_id,
    })

    self.current_location = target_id
    if self.current_location_id ~= nil then
        self.current_location_id = target_id
    end

    emit("textsystem:event.location.enter", {
        from_location = from_id,
        to_location = target_id,
    })

    return {
        ok = true,
        value = {
            from_location = from_id,
            to_location = target_id,
        },
    }
end

function Actor:travel(target)
    validate_actor_receiver(self, "travel")
    return self:move_to(target)
end

function Location:is_connected(target)
    validate_location_receiver(self, "is_connected")
    local target_id = target
    if type(target) == "table" then
        target_id = target.id or target.definition_id
    end
    if not target_id then return false end
    local connected_ids = self.connected_location_ids or (self.data and self.data.connected_location_ids) or {}
    for _, id in ipairs(connected_ids) do
        if id == target_id then
            return true
        end
    end
    return false
end

function Location:require_connected(target, opt_key)
    validate_location_receiver(self, "require_connected")
    local target_id = target
    if type(target) == "table" then
        target_id = target.id or target.definition_id
    end
    if not self:is_connected(target) then
        fail(opt_key or "travel.not_connected", {
            from_location = self.id or self.definition_id,
            to_location = target_id or "",
        })
    end
end
