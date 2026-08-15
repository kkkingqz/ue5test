-- GEW-03 / TAS-13: refusal envelope { code, params } is normalized and
-- validated (code must be a Stable ID of kind `error`); the first
-- refusing validator stops the chain. Migrated from
-- GV2RuntimeCore::Testing::RunCommandRefusalSemanticsConformance
-- (removed) — same cases, same conditions, same coverage.
--
-- Runs against the isolated CommandValidatorSpecs fixture session (see
-- Tests/Fixtures/CommandValidatorSpecs/) for the same reason as
-- validator_invocation.lua: the real production validator registry is
-- already frozen and empty by the time any spec runs.

return {
    well_formed_refusal_skips_handler_without_fault = function()
        local before = game.runtime.get_canonical_state_hash()
        local ok, err = pcall(function()
            game.runtime.dispatch_command({
                command_id = "core:command.test.gew03_refuse",
                args = {},
                sequence = 1,
            })
        end)
        assert(ok, "a typed refusal must not throw a Lua error: " .. tostring(err))
        local after = game.runtime.get_canonical_state_hash()
        assert(after == before, "a refused command must not mutate state")
    end,

    first_refusal_stops_the_validator_chain = function()
        local before = game.runtime.get_canonical_state_hash()
        local ok, err = pcall(function()
            game.runtime.dispatch_command({
                command_id = "core:command.test.gew03_chain",
                args = {},
                sequence = 2,
            })
        end)
        assert(ok, "dispatch must not throw when the chain stops at the first refusal: " .. tostring(err))
        local after = game.runtime.get_canonical_state_hash()
        assert(after == before,
            "the second (lower-priority) validator's self-incriminating mutation attempt must never run")
    end,

    malformed_refusal_code_faults_dispatch = function()
        local before = game.runtime.get_canonical_state_hash()
        local ok, err = pcall(function()
            game.runtime.dispatch_command({
                command_id = "core:command.test.gew03_bad_code",
                args = {},
                sequence = 3,
            })
        end)
        assert(not ok, "a malformed refusal code must fault dispatch, not become a typed refusal")
        assert(tostring(err):find("InvalidValidatorRefusal") ~= nil,
            "the fault must name the malformed refusal, got: " .. tostring(err))
        local after = game.runtime.get_canonical_state_hash()
        assert(after == before, "a faulted dispatch must not mutate state")
    end,

    omitted_refusal_params_default_to_empty_table = function()
        local ok, err = pcall(function()
            game.runtime.dispatch_command({
                command_id = "core:command.test.gew03_missing_params",
                args = {},
                sequence = 4,
            })
        end)
        assert(ok, "dispatch must not throw for a well-formed refusal missing params: " .. tostring(err))
        local result = game.runtime.last_command_result
        assert(type(result) == "table" and result.ok == false, "missing_params command must be refused")
        assert(type(result.error) == "table", "refusal must have an error table")
        assert(result.error.code == "core:error.test.gew03_no_params", "refusal code must be preserved")
        assert(type(result.error.params) == "table" and next(result.error.params) == nil,
            "omitted refusal.params must default to an empty table")
    end,
}
