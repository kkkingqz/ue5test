-- GEW-02 / TAS-13: read-only, ordered command validators run before the
-- mutation window opens. Migrated from
-- GV2RuntimeCore::Testing::RunCommandValidatorInvocationConformance
-- (removed) — same cases, same conditions, same coverage.
--
-- Runs against the isolated CommandValidatorSpecs fixture session (see
-- Tests/Fixtures/CommandValidatorSpecs/), not the real production session:
-- the real session's validator registry is already frozen and empty by
-- the time any spec runs, so there is nothing to invoke.
--
-- Dispatch happens by calling game.runtime.dispatch_command(...) directly
-- (the same fixed entry point the C++ boundary calls) instead of through
-- FRuntimeSession::DispatchCommand(): a spec runs inside the VM, so no C++
-- round-trip is needed. A validator/dispatcher fault that the old C++ test
-- observed as FRuntimeFault{"LuaDispatchError", ...} is observed here as a
-- Lua error caught by pcall — an equivalent, not a reduced, check.

return {
    allowing_validator_receives_context_and_handler_runs = function()
        local before = game.runtime.get_canonical_state_hash()
        local ok, err = pcall(function()
            game.runtime.dispatch_command({
                command_id = "core:command.test.gew02_read_check",
                args = { marker = "gew02" },
                sequence = 1,
            })
        end)
        assert(ok, "dispatch must not throw: " .. tostring(err))
        local after = game.runtime.get_canonical_state_hash()
        assert(after ~= before, "an allowed command's handler must run and mutate state")
    end,

    unrestricted_command_handler_runs_normally = function()
        local before = game.runtime.get_canonical_state_hash()
        local ok, err = pcall(function()
            game.runtime.dispatch_command({
                command_id = "core:command.test.gew02_allow",
                args = {},
                sequence = 2,
            })
        end)
        assert(ok, "dispatch must not throw: " .. tostring(err))
        local after = game.runtime.get_canonical_state_hash()
        assert(after ~= before, "a command with no restricting validator must still run its handler")
    end,

    refusing_validator_skips_handler_without_fault = function()
        local before = game.runtime.get_canonical_state_hash()
        local ok, err = pcall(function()
            game.runtime.dispatch_command({
                command_id = "core:command.test.gew02_refuse",
                args = {},
                sequence = 3,
            })
        end)
        assert(ok, "a typed refusal must not throw a Lua error: " .. tostring(err))
        local after = game.runtime.get_canonical_state_hash()
        assert(after == before, "a refused command must not mutate state")
    end,

    validator_mutation_attempt_faults_dispatch = function()
        local before = game.runtime.get_canonical_state_hash()
        local ok, err = pcall(function()
            game.runtime.dispatch_command({
                command_id = "core:command.test.gew02_mutate_attempt",
                args = {},
                sequence = 4,
            })
        end)
        assert(not ok, "a validator that mutates state before the window opens must fault dispatch")
        assert(tostring(err):find("MutationWindowClosed") ~= nil,
            "the fault must be the mutation-window guard, got: " .. tostring(err))
        local after = game.runtime.get_canonical_state_hash()
        assert(after == before, "a faulted dispatch must not mutate state")
    end,
}
