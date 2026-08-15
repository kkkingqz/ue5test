-- LSM-05: Mutation Window Specification (ADR-0024, CommandsAndEvents.md, CanonicalStateAndSave.md)
-- Verifies that state mutations outside the active mutation window are rejected,
-- that mutation is permitted within execute_in_window, and that nested tables are protected.

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
}
