-- GEW-06: Event envelope specification
-- Verifies event envelope structure, schema version, canonical Stable ID kind 'event',
-- immutable payload, detached deep copying, portable value constraints, and rejection
-- of runtime objects, state tables, cyclic references, and non-finite numbers.

local event_envelope = require("core:module.runtime.event_envelope")

return {
    valid_envelope_created_with_all_fields = function()
        local env = event_envelope.create({
            event_id = "core:event.location.leave",
            schema_version = 1,
            correlation_id = "corr_123",
            causation_id = "cmd_456",
            source = {
                kind = "command",
                command_id = "core:command.location.travel",
                sequence = 1,
            },
            sequence = 5,
            timestamp = 100.5,
            payload = {
                actor_id = "core:actor.player",
                from_location_id = "core:location.city.market",
                target_location_id = "core:location.city.tavern",
                count = 3,
                items = { "core:item.apple", "core:item.sword" },
                flags = { active = true },
            },
        })

        assert(event_envelope.is_envelope(env), "is_envelope must return true for valid envelope")
        assert(env.event_id == "core:event.location.leave", "event_id must match")
        assert(env.schema_version == 1, "schema_version must match")
        assert(env.correlation_id == "corr_123", "correlation_id must match")
        assert(env.causation_id == "cmd_456", "causation_id must match")
        assert(env.sequence == 5, "sequence must match")
        assert(env.timestamp == 100.5, "timestamp must match")
        assert(env.source.kind == "command", "source.kind must match")
        assert(env.source.command_id == "core:command.location.travel", "source.command_id must match")
        assert(env.source.sequence == 1, "source.sequence must match")
        assert(env.payload.actor_id == "core:actor.player", "payload.actor_id must match")
        assert(env.payload.from_location_id == "core:location.city.market", "payload.from_location_id must match")
        assert(env.payload.target_location_id == "core:location.city.tavern", "payload.target_location_id must match")
        assert(env.payload.count == 3, "payload.count must match")
        assert(#env.payload.items == 2, "payload.items length must match")
        assert(env.payload.items[1] == "core:item.apple", "payload.items[1] must match")
        assert(env.payload.items[2] == "core:item.sword", "payload.items[2] must match")
        assert(env.payload.flags.active == true, "payload.flags.active must match")
    end,

    default_schema_version_and_payload = function()
        local env = event_envelope.create({
            event_id = "core:event.location.enter",
        })

        assert(event_envelope.is_envelope(env), "is_envelope must return true")
        assert(env.event_id == "core:event.location.enter", "event_id must match")
        assert(env.schema_version == 1, "default schema_version must be 1")
        assert(type(env.payload) == "table", "payload must be a table")
        assert(next(env.payload) == nil, "default payload must be empty table")
        assert(env.correlation_id == nil, "default correlation_id must be nil")
        assert(env.causation_id == nil, "default causation_id must be nil")
        assert(env.source == nil, "default source must be nil")
    end,

    invalid_event_id_rejected = function()
        local invalid_ids = {
            nil,
            123,
            true,
            {},
            "",
            "invalid_id_grammar",
            "core:command.location.travel", -- kind 'command', not 'event'
            "core:location.city.market",    -- kind 'location', not 'event'
            "core:actor.player",           -- kind 'actor', not 'event'
            "core:item.sword",             -- kind 'item', not 'event'
        }

        for _, bad_id in ipairs(invalid_ids) do
            local ok, _ = pcall(function()
                event_envelope.create({ event_id = bad_id })
            end)
            assert(not ok, "event_id '" .. tostring(bad_id) .. "' must be rejected")
        end
    end,

    invalid_schema_version_rejected = function()
        local invalid_versions = {
            0,
            -1,
            1.5,
            "1",
            true,
            {},
        }

        for _, bad_ver in ipairs(invalid_versions) do
            local ok, _ = pcall(function()
                event_envelope.create({
                    event_id = "core:event.test",
                    schema_version = bad_ver,
                })
            end)
            assert(not ok, "schema_version '" .. tostring(bad_ver) .. "' must be rejected")
        end
    end,

    envelope_is_immutable = function()
        local env = event_envelope.create({
            event_id = "core:event.location.enter",
        })

        local ok_write_id = pcall(function()
            env.event_id = "core:event.other"
        end)
        assert(not ok_write_id, "writing to env.event_id must fail")

        local ok_write_ver = pcall(function()
            env.schema_version = 2
        end)
        assert(not ok_write_ver, "writing to env.schema_version must fail")

        local ok_write_new = pcall(function()
            env.new_field = 123
        end)
        assert(not ok_write_new, "adding new field to envelope must fail")
    end,

    payload_is_immutable = function()
        local env = event_envelope.create({
            event_id = "core:event.location.enter",
            payload = {
                location_id = "core:location.city.market",
                nested = {
                    count = 5,
                },
            },
        })

        local ok_write_top = pcall(function()
            env.payload.location_id = "core:location.city.tavern"
        end)
        assert(not ok_write_top, "writing to env.payload field must fail")

        local ok_write_nested = pcall(function()
            env.payload.nested.count = 10
        end)
        assert(not ok_write_nested, "writing to nested env.payload field must fail")

        local ok_write_new = pcall(function()
            env.payload.new_key = "test"
        end)
        assert(not ok_write_new, "adding new key to env.payload must fail")
    end,

    payload_is_detached_deep_copy = function()
        local source_payload = {
            count = 10,
            nested = {
                value = "initial",
            },
        }

        local env = event_envelope.create({
            event_id = "core:event.location.enter",
            payload = source_payload,
        })

        -- Mutate the original source table
        source_payload.count = 20
        source_payload.nested.value = "mutated"

        assert(env.payload.count == 10, "envelope payload must not reflect mutations to source table")
        assert(env.payload.nested.value == "initial", "envelope nested payload must not reflect mutations to source table")
    end,

    runtime_objects_and_state_tables_rejected_in_payload = function()
        -- Functions
        local ok_fn = pcall(function()
            event_envelope.create({
                event_id = "core:event.test",
                payload = { cb = function() end },
            })
        end)
        assert(not ok_fn, "functions must be rejected in payload")

        -- Domain objects / wrappers (e.g. WorldWrapper)
        if game and game.instances and game.instances.world then
            local w = game.instances.world()
            if w ~= nil then
                local ok_wrapper = pcall(function()
                    event_envelope.create({
                        event_id = "core:event.test",
                        payload = { world = w },
                    })
                end)
                assert(not ok_wrapper, "runtime domain object / wrapper must be rejected in payload")
            end
        end

        -- Tables with arbitrary metatables
        local custom_obj = setmetatable({}, { __index = {} })
        local ok_obj = pcall(function()
            event_envelope.create({
                event_id = "core:event.test",
                payload = { obj = custom_obj },
            })
        end)
        assert(not ok_obj, "tables with metatables must be rejected in payload")
    end,

    cycles_and_non_finite_numbers_rejected = function()
        -- Cyclic reference
        local cyclic = { a = 1 }
        cyclic.self = cyclic
        local ok_cycle = pcall(function()
            event_envelope.create({
                event_id = "core:event.test",
                payload = cyclic,
            })
        end)
        assert(not ok_cycle, "cyclic table reference in payload must be rejected")

        -- NaN
        local nan = 0 / 0
        local ok_nan = pcall(function()
            event_envelope.create({
                event_id = "core:event.test",
                payload = { val = nan },
            })
        end)
        assert(not ok_nan, "NaN in payload must be rejected")

        -- Infinity
        local inf = math.huge
        local ok_inf = pcall(function()
            event_envelope.create({
                event_id = "core:event.test",
                payload = { val = inf },
            })
        end)
        assert(not ok_inf, "Infinity in payload must be rejected")
    end,

    mixed_array_and_map_keys_rejected = function()
        local mixed = { [1] = "foo", bar = "baz" }
        local ok_mixed = pcall(function()
            event_envelope.create({
                event_id = "core:event.test",
                payload = mixed,
            })
        end)
        assert(not ok_mixed, "mixed array and map keys in payload must be rejected")
    end,

    source_provenance_is_immutable_and_isolated = function()
        local src = {
            kind = "command",
            command_id = "core:command.test",
            sequence = 1,
        }

        local env = event_envelope.create({
            event_id = "core:event.test",
            source = src,
        })

        -- Mutate source input table
        src.sequence = 2
        assert(env.source.sequence == 1, "envelope source must not reflect mutations to input table")

        -- Attempt to mutate envelope source
        local ok_write_src = pcall(function()
            env.source.sequence = 3
        end)
        assert(not ok_write_src, "writing to env.source must fail")
    end,
}
