-- LSM-05, DLA-02, DLA-03: Mutation Window Specification (ADR-0024, ADR-0027, CommandsAndEvents.md, CanonicalStateAndSave.md)
-- Verifies that state mutations outside the active mutation window are rejected,
-- that mutation is permitted within execute_in_window, that nested tables are protected,
-- that write_revision monotonically increments on every state mutation (direct, nested, table.insert/remove, wrapped decorator),
-- and that raw canonical state is strictly isolated (unwrap_state is not exported).

local mutation_window = require("core:module.runtime.mutation_window")

return {
    direct_mutation_outside_window_rejected = function()
        assert(game and game.state, "game.state must exist")

        local ok, err = pcall(function()
            game.state.world.unauthorized_flag = true
        end)
        assert(not ok, "Direct mutation to game.state.world outside mutation window must be rejected")
        assert(string.find(tostring(err), "MutationWindowClosed"), "Error must mention MutationWindowClosed, got: " .. tostring(err))
    end,

    direct_mutation_inside_window_succeeds = function()
        mutation_window.execute_in_window(function()
            game.state.world.authorized_flag = "spec_ok"
        end)
        assert(game.state.world.authorized_flag == "spec_ok", "State write inside window must succeed")

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.world.authorized_flag = nil
        end)
    end,

    nested_table_mutation_outside_window_rejected = function()
        mutation_window.execute_in_window(function()
            game.state.world.nested_test = { val = 1 }
        end)

        local ok, err = pcall(function()
            game.state.world.nested_test.val = 2
        end)
        assert(not ok, "Nested table mutation outside window must be rejected")
        assert(string.find(tostring(err), "MutationWindowClosed"), "Error must mention MutationWindowClosed, got: " .. tostring(err))

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.world.nested_test = nil
        end)
    end,

    nested_table_mutation_inside_window_succeeds = function()
        mutation_window.execute_in_window(function()
            game.state.world.nested_test2 = { counter = 10 }
        end)

        mutation_window.execute_in_window(function()
            game.state.world.nested_test2.counter = 20
        end)
        assert(game.state.world.nested_test2.counter == 20, "Nested write inside window must succeed")

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.world.nested_test2 = nil
        end)
    end,

    write_revision_increments_on_direct_mutation = function()
        local r0 = mutation_window.write_revision()
        assert(type(r0) == "number", "write_revision must return a number")

        mutation_window.execute_in_window(function()
            game.state.world.rev_test_direct = 42
        end)
        local r1 = mutation_window.write_revision()
        assert(r1 > r0, "write_revision must increment on direct field write: " .. r1 .. " > " .. r0)

        -- Multiple mutations in one window produce multiple increments
        mutation_window.execute_in_window(function()
            game.state.world.rev_test_direct = 43
            game.state.world.rev_test_direct = 44
        end)
        local r2 = mutation_window.write_revision()
        assert(r2 >= r1 + 2, "write_revision must increment per mutation: " .. r2 .. " >= " .. (r1 + 2))

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.world.rev_test_direct = nil
        end)
    end,

    write_revision_increments_on_nested_mutation = function()
        mutation_window.execute_in_window(function()
            game.state.world.rev_nested = { sub = { count = 0 } }
        end)
        local r0 = mutation_window.write_revision()

        mutation_window.execute_in_window(function()
            game.state.world.rev_nested.sub.count = 1
        end)
        local r1 = mutation_window.write_revision()
        assert(r1 > r0, "write_revision must increment on deeply nested mutation")

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.world.rev_nested = nil
        end)
    end,

    write_revision_increments_on_table_methods = function()
        mutation_window.execute_in_window(function()
            game.state.world.rev_list = { 10, 20 }
        end)
        local r0 = mutation_window.write_revision()

        -- table.insert on guarded proxy table
        mutation_window.execute_in_window(function()
            table.insert(game.state.world.rev_list, 30)
        end)
        local r1 = mutation_window.write_revision()
        assert(r1 > r0, "write_revision must increment on table.insert")
        assert(#game.state.world.rev_list == 3, "list length must be 3")
        assert(game.state.world.rev_list[3] == 30, "inserted value must match")

        -- table.remove on guarded proxy table
        mutation_window.execute_in_window(function()
            table.remove(game.state.world.rev_list, 1)
        end)
        local r2 = mutation_window.write_revision()
        assert(r2 > r1, "write_revision must increment on table.remove")
        assert(#game.state.world.rev_list == 2, "list length must be 2")
        assert(game.state.world.rev_list[1] == 20, "first element must now be 20")

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.world.rev_list = nil
        end)
    end,

    write_revision_increments_via_wrapped_decorator = function()
        local actor_reg_mod = require("core:module.runtime.actor_registry")
        local reg = actor_reg_mod.create_registry()
        reg.register_type("test_hero", function(base)
            return setmetatable({
                level_up = function(self)
                    self.level = (self.level or 1) + 1
                end,
            }, { __index = base })
        end)

        mutation_window.execute_in_window(function()
            game.state.actors["actor@1:99"] = {
                instance_id = "actor@1:99",
                definition_id = "core:actor.test",
                discriminator = "test_hero",
                level = 1,
            }
        end)

        local r0 = mutation_window.write_revision()
        mutation_window.execute_in_window(function()
            local wrapped = reg.wrap(game.state.actors["actor@1:99"])
            wrapped:level_up()
        end)
        local r1 = mutation_window.write_revision()
        assert(r1 > r0, "write_revision must increment on wrapped decorator mutation: " .. r1 .. " > " .. r0)
        assert(game.state.actors["actor@1:99"].level == 2, "level must be 2")

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.actors["actor@1:99"] = nil
        end)
    end,

    raw_state_isolated_and_unwrap_state_not_exported = function()
        -- DLA-03: unwrap_state is absent from public exports
        assert(mutation_window.unwrap_state == nil, "mutation_window.unwrap_state must NOT be exported")
        assert(type(mutation_window.guard_state) == "function", "guard_state must be exported for host")
        assert(type(mutation_window.write_revision) == "function", "write_revision must be exported")
    end,
}
