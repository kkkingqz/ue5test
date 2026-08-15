-- Command Validator Registry (GEW-01)
-- Registers ordered, read-only command validators during the "register"
-- lifecycle phase and freezes them together with the other registries.
-- Only the registry and its stable order are implemented here; running
-- validators against a command (permission scope, refusal envelope) is
-- CommandsAndEvents.md "Command validators" scope for GEW-02/GEW-03.

local stable_id = require("core:module.runtime.stable_id")

local M = {
    id = "core:module.runtime.validator_registry",
}

-- Stable order: priority (ascending, lower runs first), then package load
-- order, then registration order (CommandsAndEvents.md "Command validators").
--
-- There is a single global "register" lifecycle pass over the fully
-- resolved module LoadOrder (core modules first, then mods in resolved
-- order; BootstrapAndSessionLifecycle.md "Module lifecycle"). Because every
-- module's register(ctx) hook runs exactly once, in that order, on one
-- registry instance, a single monotonic `sequence` counter assigned inside
-- registry.register() already encodes both "package load order" (which
-- module's register() call happens first) and "registration order" (call
-- order within one module) as one composite tie-break key. A separate
-- numeric package-load-order field is not tracked because no second
-- dimension exists yet: mod/package registration is out of this plan's
-- scope (GameplayEventsAndWorld/README.md "Не входят").
function M.create_registry()
    local entries = {}
    local entries_by_id = {}
    local is_frozen = false
    local next_sequence = 1

    local registry = {}

    function registry.register(id, validator_impl, options)
        if is_frozen then
            error("ValidatorRegistryFrozen: cannot register validator '" .. tostring(id) .. "' after register phase / freeze", 2)
        end
        if type(id) ~= "string" or not stable_id.is_kind(id, "validator") then
            error("InvalidValidatorId: validator id must be a canonical Stable ID of kind 'validator', got '" .. tostring(id) .. "'", 2)
        end
        if entries_by_id[id] ~= nil then
            error("ValidatorDuplicateRegistration: validator '" .. id .. "' is already registered", 2)
        end
        if type(validator_impl) ~= "table" then
            error("InvalidValidatorImpl: validator implementation must be a table", 2)
        end

        local priority = 0
        if options ~= nil then
            if type(options) ~= "table" then
                error("InvalidValidatorOptions: options must be a table", 2)
            end
            if options.priority ~= nil then
                if type(options.priority) ~= "number" or math.type(options.priority) ~= "integer" then
                    error("InvalidValidatorPriority: priority must be an integer", 2)
                end
                priority = options.priority
            end
        end

        local entry = {
            id = id,
            impl = validator_impl,
            priority = priority,
            sequence = next_sequence,
        }
        next_sequence = next_sequence + 1
        entries_by_id[id] = entry
        table.insert(entries, entry)
        return validator_impl
    end

    function registry.get(id)
        local entry = entries_by_id[id]
        return entry and entry.impl or nil
    end

    function registry.exists(id)
        return type(id) == "string" and entries_by_id[id] ~= nil
    end

    function registry.freeze()
        if is_frozen then
            return
        end
        table.sort(entries, function(a, b)
            if a.priority ~= b.priority then
                return a.priority < b.priority
            end
            return a.sequence < b.sequence
        end)
        is_frozen = true
    end

    function registry.is_frozen()
        return is_frozen
    end

    -- Ordered list of { id = ..., impl = ... } in final stable order.
    -- Meaningful only after freeze(); returns raw registration order before it.
    function registry.ordered()
        local result = {}
        for i, entry in ipairs(entries) do
            result[i] = { id = entry.id, impl = entry.impl }
        end
        return result
    end

    local public_facade = {}
    local mt = {
        __index = function(_, k)
            if registry[k] ~= nil then
                return registry[k]
            end
            local entry = entries_by_id[k]
            return entry and entry.impl or nil
        end,
        __newindex = function(_, k, v)
            error("ValidatorRegistryDirectAssignmentDisallowed: use game.commands.validators.register(id, validator, options) to register a validator", 2)
        end,
        __tostring = function(_)
            return "GameplayValidatorRegistry"
        end,
    }
    setmetatable(public_facade, mt)
    return public_facade
end

-- Self-contained conformance check, called by both hosts through the same
-- test command (core:command.test.validator_registry_conformance) rather
-- than duplicated host-local assertions (HeadlessParityAndReplay/README.md
-- "Правило проверяется одной реализацией").
--
-- Returns "" on success, or a stable "validator_registry.<case_id>" on the
-- first failing case.
function M.run_conformance()
    local registry = M.create_registry()

    -- Priority ascending; ties broken by registration order.
    registry.register("core:validator.test.high_priority", {}, { priority = 5 })
    registry.register("core:validator.test.tie_first", {}, { priority = 1 })
    registry.register("core:validator.test.tie_second", {}, { priority = 1 })
    registry.register("core:validator.test.default_priority", {})

    registry.freeze()

    local ordered = registry.ordered()
    local expected_order = {
        "core:validator.test.default_priority",
        "core:validator.test.tie_first",
        "core:validator.test.tie_second",
        "core:validator.test.high_priority",
    }
    if #ordered ~= #expected_order then
        return "validator_registry.order_length_mismatch"
    end
    for i, expected_id in ipairs(expected_order) do
        if ordered[i].id ~= expected_id then
            return "validator_registry.order_mismatch"
        end
    end

    local late_ok = pcall(function()
        registry.register("core:validator.test.late", {})
    end)
    if late_ok then
        return "validator_registry.late_registration_not_rejected"
    end

    local fresh = M.create_registry()
    local invalid_id_ok = pcall(function()
        fresh.register("core:item.not_a_validator", {})
    end)
    if invalid_id_ok then
        return "validator_registry.invalid_id_not_rejected"
    end

    local duplicate_ok = pcall(function()
        fresh.register("core:validator.test.dup", {})
        fresh.register("core:validator.test.dup", {})
    end)
    if duplicate_ok then
        return "validator_registry.duplicate_not_rejected"
    end

    return ""
end

function M.register(_ctx)
    if not game then
        game = {}
    end
    if not game.commands then
        game.commands = {}
    end
    game.commands.validators = M.create_registry()
end

return M
