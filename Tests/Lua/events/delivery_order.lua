-- GEW-08: Deterministic Event Delivery Specification
-- Verifies FIFO queue processing, breadth-first traversal for nested/child events,
-- and deterministic subscriber invocation order by priority ascending + registration order.

local event_bus = require("core:module.runtime.event_bus")
local command_dispatcher = require("core:module.runtime.command_dispatcher")
local handler_registry = require("core:module.runtime.handler_registry")

return {
    subscriber_priority_ordering = function()
        event_bus.with_isolated_subscribers(function()
            event_bus.clear_published_events()

            local trace = {}

            event_bus.subscribe("core:event.test.priority", function(_env)
                table.insert(trace, "sub_10")
            end, { priority = 10 })

            event_bus.subscribe("core:event.test.priority", function(_env)
                table.insert(trace, "sub_minus_5")
            end, { priority = -5 })

            event_bus.subscribe("core:event.test.priority", function(_env)
                table.insert(trace, "sub_0_first")
            end, { priority = 0 })

            event_bus.subscribe("core:event.test.priority", function(_env)
                table.insert(trace, "sub_5")
            end, { priority = 5 })

            event_bus.subscribe("core:event.test.priority", function(_env)
                table.insert(trace, "sub_0_second")
            end, { priority = 0 })

            local reg = handler_registry.create_registry()
            reg.register("core:command.test.gew08_priority", function(_request)
                game.events.enqueue({
                    event_id = "core:event.test.priority",
                    payload = {},
                })
                return { ok = true }
            end)

            local dispatcher = command_dispatcher.new(reg)
            dispatcher.dispatch({
                command_id = "core:command.test.gew08_priority",
                args = {},
                sequence = 201,
            })

            assert(#trace == 5, "all 5 subscribers must have executed, got " .. tostring(#trace))
            assert(trace[1] == "sub_minus_5", "priority -5 must run 1st")
            assert(trace[2] == "sub_0_first", "priority 0 (1st registered) must run 2nd")
            assert(trace[3] == "sub_0_second", "priority 0 (2nd registered) must run 3rd")
            assert(trace[4] == "sub_5", "priority 5 must run 4th")
            assert(trace[5] == "sub_10", "priority 10 must run 5th")
        end)
    end,

    fifo_event_queue_ordering = function()
        event_bus.with_isolated_subscribers(function()
            event_bus.clear_published_events()

            local trace = {}

            event_bus.subscribe("core:event.test.step1", function(env)
                table.insert(trace, "recv_" .. env.event_id)
            end)
            event_bus.subscribe("core:event.test.step2", function(env)
                table.insert(trace, "recv_" .. env.event_id)
            end)
            event_bus.subscribe("core:event.test.step3", function(env)
                table.insert(trace, "recv_" .. env.event_id)
            end)

            local reg = handler_registry.create_registry()
            reg.register("core:command.test.gew08_fifo", function(_request)
                game.events.enqueue({ event_id = "core:event.test.step1", payload = {} })
                game.events.enqueue({ event_id = "core:event.test.step2", payload = {} })
                game.events.enqueue({ event_id = "core:event.test.step3", payload = {} })
                return { ok = true }
            end)

            local dispatcher = command_dispatcher.new(reg)
            dispatcher.dispatch({
                command_id = "core:command.test.gew08_fifo",
                args = {},
                sequence = 202,
            })

            assert(#trace == 3, "all 3 events must be received, got " .. tostring(#trace))
            assert(trace[1] == "recv_core:event.test.step1", "step1 must be first")
            assert(trace[2] == "recv_core:event.test.step2", "step2 must be second")
            assert(trace[3] == "recv_core:event.test.step3", "step3 must be third")
        end)
    end,

    breadth_first_nested_event_delivery = function()
        event_bus.with_isolated_subscribers(function()
            event_bus.clear_published_events()

            local trace = {}

            -- Root event subscribers
            event_bus.subscribe("core:event.test.root", function(_env)
                table.insert(trace, "root_sub1")
                -- Sub1 emits ChildA
                game.events.enqueue({ event_id = "core:event.test.child_a", payload = {} })
            end, { priority = 1 })

            event_bus.subscribe("core:event.test.root", function(_env)
                table.insert(trace, "root_sub2")
                -- Sub2 emits ChildB
                game.events.enqueue({ event_id = "core:event.test.child_b", payload = {} })
            end, { priority = 2 })

            -- ChildA subscriber (emits GrandChildA)
            event_bus.subscribe("core:event.test.child_a", function(_env)
                table.insert(trace, "child_a_sub")
                game.events.enqueue({ event_id = "core:event.test.grandchild_a", payload = {} })
            end)

            -- ChildB subscriber (emits nothing)
            event_bus.subscribe("core:event.test.child_b", function(_env)
                table.insert(trace, "child_b_sub")
            end)

            -- GrandChildA subscriber (emits nothing)
            event_bus.subscribe("core:event.test.grandchild_a", function(_env)
                table.insert(trace, "grandchild_a_sub")
            end)

            local reg = handler_registry.create_registry()
            reg.register("core:command.test.gew08_breadth_first", function(_request)
                game.events.enqueue({ event_id = "core:event.test.root", payload = {} })
                return { ok = true }
            end)

            local dispatcher = command_dispatcher.new(reg)
            dispatcher.dispatch({
                command_id = "core:command.test.gew08_breadth_first",
                args = {},
                sequence = 203,
            })

            -- In Breadth-First order:
            -- 1. root_sub1 runs -> enqueues child_a
            -- 2. root_sub2 runs -> enqueues child_b
            -- 3. child_a_sub runs -> enqueues grandchild_a
            -- 4. child_b_sub runs (BEFORE grandchild_a!)
            -- 5. grandchild_a_sub runs
            assert(#trace == 5, "all 5 subscriber callbacks must have run, got " .. tostring(#trace))
            assert(trace[1] == "root_sub1", "1: root_sub1")
            assert(trace[2] == "root_sub2", "2: root_sub2")
            assert(trace[3] == "child_a_sub", "3: child_a_sub")
            assert(trace[4] == "child_b_sub", "4: child_b_sub (breadth-first before grandchild)")
            assert(trace[5] == "grandchild_a_sub", "5: grandchild_a_sub")
        end)
    end,
}
