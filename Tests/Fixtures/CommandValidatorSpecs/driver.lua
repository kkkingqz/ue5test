-- TAS-13: test-only fixture consumed by Tests/Lua/commands/*.lua specs.
-- Registers test-scoped validators (each restricted to exactly one test
-- command ID, so they never interfere with each other or with unrelated
-- commands) and a single handler that mutates game.state.counter for any
-- recognized test command. Not under Scripts/ — never loaded by the real
-- module loader, never staged into the packaged game.
local dispatcher_factory = require("core:module.runtime.command_dispatcher")

local M = {
    id = "core:module.test.command_validator_specs_driver",
}

local function make_scoped_validator(target_command_id, behavior)
    return {
        validate = function(ctx)
            if ctx.command_id ~= target_command_id then
                return true
            end
            if behavior == "refuse" then
                return false, { code = "core:error.test.command_specs_refused", params = { reason = "test" } }
            elseif behavior == "refuse_no_params" then
                return false, { code = "core:error.test.gew03_no_params" }
            elseif behavior == "refuse_bad_code" then
                return false, { code = "not-canonical", params = {} }
            elseif behavior == "mutate_attempt" then
                -- Only reachable if the window guard incorrectly let this
                -- through; the attempted mutation makes that a fault
                -- instead of a silently-wrong pass.
                ctx.state.mutation_attempt = true
                return true
            elseif behavior == "mutate_if_invoked" then
                -- Only reachable if the chain incorrectly continued past a
                -- prior refusal.
                ctx.state.b_was_called = true
                return true
            elseif behavior == "read_check" then
                assert(type(ctx.state) == "table", "validator must receive state")
                assert(ctx.repository ~= nil, "validator must receive repository")
                assert(type(ctx.payload) == "table", "validator must receive payload")
                assert(ctx.payload.marker == "gew02", "validator must receive the actual command payload")
                return true
            end
            return true
        end,
    }
end

function M.register(_ctx)
    -- GEW-02 fixtures (Tests/Lua/commands/validator_invocation.lua)
    game.commands.validators.register(
        "core:validator.test.gew02_read_check",
        make_scoped_validator("core:command.test.gew02_read_check", "read_check"))
    game.commands.validators.register(
        "core:validator.test.gew02_refuse",
        make_scoped_validator("core:command.test.gew02_refuse", "refuse"))
    game.commands.validators.register(
        "core:validator.test.gew02_mutate_attempt",
        make_scoped_validator("core:command.test.gew02_mutate_attempt", "mutate_attempt"))

    -- GEW-03 fixtures (Tests/Lua/commands/refusal_semantics.lua)
    game.commands.validators.register(
        "core:validator.test.gew03_refuse",
        make_scoped_validator("core:command.test.gew03_refuse", "refuse"))
    game.commands.validators.register(
        "core:validator.test.gew03_missing_params",
        make_scoped_validator("core:command.test.gew03_missing_params", "refuse_no_params"))
    game.commands.validators.register(
        "core:validator.test.gew03_bad_code",
        make_scoped_validator("core:command.test.gew03_bad_code", "refuse_bad_code"))
    game.commands.validators.register(
        "core:validator.test.gew03_chain_a",
        make_scoped_validator("core:command.test.gew03_chain", "refuse"),
        { priority = 0 })
    game.commands.validators.register(
        "core:validator.test.gew03_chain_b",
        make_scoped_validator("core:command.test.gew03_chain", "mutate_if_invoked"),
        { priority = 1 })

    local handler = {
        handle_command = function(request)
            if request.command_id == "core:command.test.gew02_read_check"
                or request.command_id == "core:command.test.gew02_allow"
                or request.command_id == "core:command.test.gew02_refuse"
                or request.command_id == "core:command.test.gew02_mutate_attempt"
                or request.command_id == "core:command.test.gew03_refuse"
                or request.command_id == "core:command.test.gew03_chain"
                or request.command_id == "core:command.test.gew03_missing_params"
                or request.command_id == "core:command.test.gew03_bad_code"
            then
                game.state.counter = (game.state.counter or 0) + 1
                return { ok = true, value = { counter = game.state.counter } }
            end
            return nil
        end,
    }
    local dispatcher = dispatcher_factory.new({ handler })
    game.runtime.dispatch_command = dispatcher.dispatch
    game.runtime.get_canonical_state_hash = function()
        return tostring(game.state.counter or 0)
            .. ":" .. tostring(game.state.mutation_attempt or false)
            .. ":" .. tostring(game.state.b_was_called or false)
    end
end

return M
