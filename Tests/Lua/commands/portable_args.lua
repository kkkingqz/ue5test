-- DLA-04: Portable Value Validation Specification (ADR-0027, CommandsAndEvents.md)
-- Verifies that non-portable values (functions, metatables/wrappers, state tables, cycles,
-- sparse arrays, mixed keys, non-finite numbers) are rejected across:
--   1. Event envelope creation (event_envelope.create / event_bus)
--   2. Synchronous command dispatch (command_dispatcher.dispatch)
--   3. Deferred command queue (command_dispatcher.enqueue / game.commands.enqueue)

local portable_value = require("core:module.runtime.portable_value")
local event_envelope = require("core:module.runtime.event_envelope")
local command_dispatcher = require("core:module.runtime.command_dispatcher")

local function build_invalid_values()
    local cyclic_tbl = { name = "cycle" }
    cyclic_tbl.self = cyclic_tbl

    local metatable_tbl = setmetatable({ key = 1 }, { __index = {} })

    return {
        function_val = function() end,
        nested_function = { sub = { fn = function() end } },
        metatable_obj = metatable_tbl,
        state_table = game and game.state or metatable_tbl,
        cyclic = cyclic_tbl,
        sparse_array = { [1] = "a", [3] = "c" },
        mixed_keys = { [1] = "array_val", map_key = "map_val" },
        nan_number = 0 / 0,
        inf_number = 1 / 0,
        empty_string_key = { [""] = "val" },
        negative_int_key = { [-1] = "val" },
    }
end

return {
    portable_value_validator_accepts_valid_payloads = function()
        local valid_payloads = {
            nil,
            true,
            false,
            "hello string",
            123,
            45.67,
            { 1, 2, 3, "four" },
            { name = "aria", hp = 100, flags = { active = true } },
            { nested = { list = { "a", "b", { deep = true } } } },
        }

        for _, val in ipairs(valid_payloads) do
            local ok, err = pcall(function()
                portable_value.validate(val, "test")
            end)
            assert(ok, "Valid portable value must be accepted, got error: " .. tostring(err))
        end
    end,

    portable_value_validator_rejects_invalid_values = function()
        local invalid_values = build_invalid_values()

        for name, bad_val in pairs(invalid_values) do
            local ok, _ = pcall(function()
                portable_value.validate(bad_val, "test_field", "InvalidPortableValue")
            end)
            assert(not ok, "portable_value.validate must reject " .. name)
        end
    end,

    event_envelope_rejects_non_portable_payloads = function()
        local invalid_values = build_invalid_values()

        for name, bad_val in pairs(invalid_values) do
            local ok, _ = pcall(function()
                event_envelope.create({
                    event_id = "core:event.location.leave",
                    payload = bad_val,
                })
            end)
            assert(not ok, "event_envelope.create must reject non-portable payload: " .. name)
        end
    end,

    command_dispatcher_rejects_non_portable_args_on_sync_dispatch = function()
        local dispatcher = command_dispatcher.new()
        local invalid_values = build_invalid_values()

        for name, bad_val in pairs(invalid_values) do
            local ok, _ = pcall(function()
                dispatcher.dispatch({
                    command_id = "core:command.location.travel",
                    args = bad_val,
                    sequence = 100,
                })
            end)
            assert(not ok, "command_dispatcher.dispatch must reject non-portable args: " .. name)
        end
    end,

    command_queue_rejects_non_portable_args_on_enqueue = function()
        local invalid_values = build_invalid_values()

        for name, bad_val in pairs(invalid_values) do
            local ok, _ = pcall(function()
                command_dispatcher.enqueue({
                    command_id = "core:command.location.travel",
                    args = bad_val,
                })
            end)
            assert(not ok, "command_dispatcher.enqueue must reject non-portable args: " .. name)
        end
    end,
}
