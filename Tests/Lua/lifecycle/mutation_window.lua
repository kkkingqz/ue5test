-- LSM-05, DLA-02, DLA-03: Mutation Window Specification (ADR-0024, ADR-0027, CommandsAndEvents.md, CanonicalStateAndSave.md)
-- Verifies that state mutations outside the active mutation window are rejected,
-- that mutation is permitted within execute_in_window, that nested tables are protected,
-- that write_revision monotonically increments on every state mutation (direct, nested, table.insert/remove, domain method),
-- and that raw canonical state is strictly isolated (unwrap_state is not exported).

local mutation_window = require("core:module.runtime.mutation_window")

local function ensure_player()
    local player = game.instances.actors.player()
    if player then
        return player
    end
    local hero = game.instances.actors.create("rh:actor.character.hero", {
        stamina = 20,
        gold = 50,
    })
    game.state.meta.player_actor_id = hero.instance_id
    return hero
end

return {
    direct_mutation_outside_window_rejected = function()
        assert(game and game.state, "game.state must exist")

        local ok, err = pcall(function()
            game.state.world.unauthorized_flag = true
        end)
        assert(not ok, "Direct mutation to game.state.world outside mutation window must be rejected")
        assert(string.find(tostring(err), "MutationWindowClosed"), "Error must mention MutationWindowClosed, got: " .. tostring(err))
    end,

    mutation_allowed_inside_window = function()
        assert(game and game.state, "game.state must exist")

        local result = mutation_window.execute_in_window(function()
            game.state.world.test_flag = "authorized_value"
            return game.state.world.test_flag
        end)

        assert(result == "authorized_value", "Mutation inside window must succeed")
        assert(game.state.world.test_flag == "authorized_value", "Value must be readable outside window")

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.world.test_flag = nil
        end)
    end,

    nested_table_mutation_protected = function()
        assert(game and game.state, "game.state must exist")

        mutation_window.execute_in_window(function()
            game.state.world.nested_container = { sub_key = 10 }
        end)

        local ok, err = pcall(function()
            game.state.world.nested_container.sub_key = 20
        end)
        assert(not ok, "Mutating nested state tables outside window must be rejected")
        assert(string.find(tostring(err), "MutationWindowClosed"), "Error must mention MutationWindowClosed, got: " .. tostring(err))

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.world.nested_container = nil
        end)
    end,

    window_closed_after_exception = function()
        assert(not mutation_window.is_open(), "Window must be closed initially")

        local ok, _ = pcall(function()
            mutation_window.execute_in_window(function()
                assert(mutation_window.is_open(), "Window must be open inside callback")
                error("SimulatedHandlerFailure")
            end)
        end)

        assert(not ok, "Exception inside window must propagate")
        assert(not mutation_window.is_open(), "Mutation window must be closed after exception")
    end,

    write_revision_increments_on_direct_and_idempotent_mutation = function()
        assert(game and game.state, "game.state must exist")

        local r0 = mutation_window.write_revision()
        assert(type(r0) == "number", "write_revision must return a number")

        mutation_window.execute_in_window(function()
            game.state.world.rev_direct = "value_1"
        end)
        local r1 = mutation_window.write_revision()
        assert(r1 > r0, "write_revision must increment on direct write: " .. r1 .. " > " .. r0)

        -- Idempotent write (x = x) must also increment write_revision as a state write attempt
        mutation_window.execute_in_window(function()
            game.state.world.rev_direct = "value_1"
        end)
        local r2 = mutation_window.write_revision()
        assert(r2 > r1, "write_revision must increment on idempotent write: " .. r2 .. " > " .. r1)

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.world.rev_direct = nil
        end)
    end,

    write_revision_increments_on_nested_and_table_operations = function()
        assert(game and game.state, "game.state must exist")

        local r0 = mutation_window.write_revision()

        mutation_window.execute_in_window(function()
            game.state.world.rev_list = { 10, 20 }
        end)
        local r1 = mutation_window.write_revision()
        assert(r1 > r0, "write_revision must increment on nested table creation")

        -- table.insert on guarded proxy table
        mutation_window.execute_in_window(function()
            table.insert(game.state.world.rev_list, 30)
        end)
        local r2 = mutation_window.write_revision()
        assert(r2 > r1, "write_revision must increment on table.insert")
        assert(#game.state.world.rev_list == 3, "list length must be 3")
        assert(game.state.world.rev_list[3] == 30, "inserted value must match")

        -- table.remove on guarded proxy table
        mutation_window.execute_in_window(function()
            table.remove(game.state.world.rev_list, 1)
        end)
        local r3 = mutation_window.write_revision()
        assert(r3 > r2, "write_revision must increment on table.remove")
        assert(#game.state.world.rev_list == 2, "list length must be 2")
        assert(game.state.world.rev_list[1] == 20, "first element must now be 20")

        -- Cleanup
        mutation_window.execute_in_window(function()
            game.state.world.rev_list = nil
        end)
    end,

    write_revision_increments_via_domain_methods = function()
        assert(game and game.instances and game.instances.actors, "actors registry must exist")

        local r0 = mutation_window.write_revision()
        mutation_window.execute_in_window(function()
            local player = ensure_player()
            assert(player ~= nil, "player must exist")
            player:add_gold(10)
        end)
        local r1 = mutation_window.write_revision()
        assert(r1 > r0, "write_revision must increment on domain method mutation: " .. r1 .. " > " .. r0)
    end,

    raw_state_isolated_and_unwrap_state_not_exported = function()
        -- DLA-03: unwrap_state is absent from public exports
        assert(mutation_window.unwrap_state == nil, "mutation_window.unwrap_state must NOT be exported")
        assert(type(mutation_window.guard_state) == "function", "guard_state must be exported for host")
        assert(type(mutation_window.write_revision) == "function", "write_revision must be exported")

        local controller, admin = mutation_window.create_controller()
        assert(controller.unwrap_state == nil, "controller must not expose unwrap_state")
        assert(type(admin.unwrap) == "function", "admin handle holds internal unwrap function")
    end,
}
