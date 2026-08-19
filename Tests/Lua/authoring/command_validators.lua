-- CVA-04..08: Command Validator Authoring Specification (ADR-0033)
-- Verifies:
--   1. Execution scope tracking (none | command | validator | event) and restoration on success, refusal, and exception.
--   2. fail() in validator scope returning typed refusal with declaring package namespace.
--   3. validate() parameter validation and error types (InvalidAuthoringValidatorCommand/Name/Function).
--   4. Validator Stable ID construction and record introspection.
--   5. Duplicate validator rejection within package (AuthoringValidatorDuplicate).
--   6. Target command resolution at freeze and order independence across modules (AuthoringValidatorTargetMissing).
--   7. Declaration after freeze rejection (AuthoringValidatorDeclarationAfterFreeze).
--   8. Ordered execution and first-refusal-wins skipping handler without fault.
--   9. Identical argument decoding for handler and validator across positional, empty, and map forms.
--  10. Presence of validate() in authoring environment _ENV.

local authoring_context = require("core:module.authoring.context")
local handler_registry = require("core:module.runtime.handler_registry")
local mutation_window = require("core:module.runtime.mutation_window")

return {
    execution_scope_tracking_and_restoration = function()
        authoring_context.with_isolated_context(function()
            assert(authoring_context.get_current_scope().kind == "none", "Initial scope must be 'none'")

            local M_rh = authoring_context.gameplay("rh")
            local M_curse = authoring_context.gameplay("curse")

            local observed_command_scope = nil
            local observed_validator_scope = nil
            local observed_event_scope = nil

            M_rh.commands.test_scope = function()
                observed_command_scope = authoring_context.get_current_scope()
                M_rh.emit("test_event", {})
                return { ok = true }
            end

            M_curse.validate("rh:command.test_scope", "check_scope", function()
                observed_validator_scope = authoring_context.get_current_scope()
            end)

            M_rh.on("test_event", function()
                observed_event_scope = authoring_context.get_current_scope()
            end)

            M_rh.register()
            M_curse.register()
            authoring_context.freeze()

            -- 1. Scope during command dispatch & validator execution (Success path)
            local res = M_rh.commands.test_scope:run()
            assert(res ~= nil and res.ok == true, "Command should succeed")

            assert(observed_command_scope ~= nil, "Command scope must be captured")
            assert(observed_command_scope.kind == "command", "Scope kind must be 'command'")
            assert(observed_command_scope.package_id == "rh", "Scope package must be 'rh'")
            assert(observed_command_scope.command_id == "rh:command.test_scope", "Scope command_id must match")
            assert(observed_command_scope.initial_write_revision ~= nil, "Scope initial_write_revision must be present")

            assert(observed_validator_scope ~= nil, "Validator scope must be captured")
            assert(observed_validator_scope.kind == "validator", "Scope kind must be 'validator'")
            assert(observed_validator_scope.package_id == "curse", "Scope package must be declaring package 'curse'")
            assert(observed_validator_scope.command_id == "rh:command.test_scope", "Scope command_id must be target command")
            assert(observed_validator_scope.validator_id == "curse:validator.rh.test_scope.check_scope", "Scope validator_id must match")

            assert(observed_event_scope ~= nil, "Event scope must be captured")
            assert(observed_event_scope.kind == "event", "Scope kind must be 'event'")
            assert(observed_event_scope.package_id == "rh", "Scope package must be 'rh'")
            assert(observed_event_scope.event_id == "rh:event.test_event", "Scope event_id must match")

            assert(authoring_context.get_current_scope().kind == "none", "Scope must restore to 'none' on success")

            -- 2. Scope restoration on typed refusal (Refusal path)
            local M_refuser = authoring_context.gameplay("refuser")
            M_refuser.commands.refuse_cmd = function()
                return { ok = true }
            end
            M_refuser.validate(M_refuser.commands.refuse_cmd, "do_refuse", function()
                M_refuser.fail("refused", { reason = "test" })
            end)
            M_refuser.register()
            authoring_context.freeze()

            local fail_res = M_refuser.commands.refuse_cmd:run()
            assert(fail_res ~= nil and fail_res.ok == false, "Command should be refused")
            assert(authoring_context.get_current_scope().kind == "none", "Scope must restore to 'none' on typed refusal")

            -- 3. Scope restoration on exception (Exception path)
            local M_buggy = authoring_context.gameplay("buggy")
            M_buggy.commands.buggy_cmd = function()
                return { ok = true }
            end
            M_buggy.validate(M_buggy.commands.buggy_cmd, "do_throw", function()
                error("ValidatorUnexpectedError", 0)
            end)
            M_buggy.register()
            authoring_context.freeze()

            local exc_ok, _ = pcall(function()
                M_buggy.commands.buggy_cmd:run()
            end)
            assert(not exc_ok, "Command dispatch with crashing validator must throw")
            assert(authoring_context.get_current_scope().kind == "none", "Scope must restore to 'none' on exception")
        end)
    end,

    fail_in_validator_scope_produces_typed_refusal = function()
        authoring_context.with_isolated_context(function()
            local M_rh = authoring_context.gameplay("rh")
            local M_curse = authoring_context.gameplay("curse_mod")

            local handler_called = false
            M_rh.commands.travel = function(target)
                handler_called = true
                return { ok = true, target = target }
            end

            M_curse.validate("rh:command.travel", "curse_lock", function(target)
                M_curse.fail("travel.rooted", { target = target, duration = 3 })
            end)

            M_rh.register()
            M_curse.register()
            authoring_context.freeze()

            local res = M_rh.commands.travel:run("city.market")
            assert(type(res) == "table", "Result must be a table")
            assert(res.ok == false, "Command ok must be false on validator fail()")
            assert(type(res.error) == "table", "res.error must be a table")
            assert(res.error.code == "curse_mod:error.travel.rooted", "Error code must use declaring package namespace: " .. tostring(res.error.code))
            assert(res.error.params.target == "city.market", "Error params target must match")
            assert(res.error.params.duration == 3, "Error params duration must match")
            assert(handler_called == false, "Command handler must not be called when validator refuses")
        end)
    end,

    fail_outside_command_and_validator_rejected = function()
        authoring_context.with_isolated_context(function()
            -- Calling fail outside active command / validator must throw AuthoringFailOutsideCommand
            local ok_none, err_none = pcall(function()
                authoring_context.fail("unauthorized_error", {})
            end)
            assert(not ok_none, "fail() in scope 'none' must throw")
            assert(string.find(tostring(err_none), "AuthoringFailOutsideCommand"), "Error must mention AuthoringFailOutsideCommand, got: " .. tostring(err_none))

            -- Calling fail inside event subscriber must also throw AuthoringFailOutsideCommand
            local M_rh = authoring_context.gameplay("rh")
            local sub_error = nil
            M_rh.commands.trigger_cmd = function()
                M_rh.emit("test_fail_event", {})
                return { ok = true }
            end
            M_rh.on("test_fail_event", function()
                local ok_ev, err_ev = pcall(function()
                    M_rh.fail("event_fail", {})
                end)
                if not ok_ev then
                    sub_error = err_ev
                end
            end)
            M_rh.register()
            authoring_context.freeze()

            M_rh.commands.trigger_cmd:run()
            assert(sub_error ~= nil, "fail() inside event subscriber must throw")
            assert(string.find(tostring(sub_error), "AuthoringFailOutsideCommand"), "Error must mention AuthoringFailOutsideCommand, got: " .. tostring(sub_error))
        end)
    end,

    lua_error_in_validator_bubbles_as_runtime_fault = function()
        authoring_context.with_isolated_context(function()
            local M_rh = authoring_context.gameplay("rh")
            local handler_called = false

            M_rh.commands.safe_cmd = function()
                handler_called = true
                return { ok = true }
            end

            M_rh.validate(M_rh.commands.safe_cmd, "crasher", function()
                local t = nil
                local _ = t.nonexistent_field -- Lua nil indexing error
            end)

            M_rh.register()
            authoring_context.freeze()

            local ok, err = pcall(function()
                M_rh.commands.safe_cmd:run()
            end)

            assert(not ok, "Arbitrary Lua error in validator must bubble up and fail dispatch")
            assert(string.find(tostring(err), "attempt to index") ~= nil or string.find(tostring(err), "LuaDispatchError") ~= nil or string.find(tostring(err), "nonexistent_field") ~= nil,
                "Error must be Lua error, got: " .. tostring(err))
            assert(handler_called == false, "Handler must not be called after validator fault")
        end)
    end,

    validate_api_validation_errors = function()
        authoring_context.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")

            -- 1. Invalid command_ref
            local bad_cmds = { nil, 123, "", "not_a_command", "rh:item.invalid_kind", {} }
            for _, bad_cmd in ipairs(bad_cmds) do
                local ok, err = pcall(function()
                    M.validate(bad_cmd, "check", function() end)
                end)
                assert(not ok, "Invalid command_ref must throw: " .. tostring(bad_cmd))
                assert(string.find(tostring(err), "InvalidAuthoringValidatorCommand"), "Error must mention InvalidAuthoringValidatorCommand, got: " .. tostring(err))
            end

            -- 2. Invalid validator_name
            local bad_names = { nil, 123, "", "Upper_Case", "has.dot", "has:colon", "has-dash", "has space" }
            for _, bad_name in ipairs(bad_names) do
                local ok, err = pcall(function()
                    M.validate("rh:command.travel", bad_name, function() end)
                end)
                assert(not ok, "Invalid validator_name must throw: " .. tostring(bad_name))
                assert(string.find(tostring(err), "InvalidAuthoringValidatorName"), "Error must mention InvalidAuthoringValidatorName, got: " .. tostring(err))
            end

            -- 3. Invalid validator_fn
            local bad_fns = { nil, 123, "not_a_function", {}, true }
            for _, bad_fn in ipairs(bad_fns) do
                local ok, err = pcall(function()
                    M.validate("rh:command.travel", "valid_name", bad_fn)
                end)
                assert(not ok, "Invalid validator_fn must throw: " .. tostring(bad_fn))
                assert(string.find(tostring(err), "InvalidAuthoringValidatorFunction"), "Error must mention InvalidAuthoringValidatorFunction, got: " .. tostring(err))
            end

            -- 4. Duplicate validator in same package
            M.validate("rh:command.travel", "dup_check", function() end)
            local dup_ok, dup_err = pcall(function()
                M.validate("rh:command.travel", "dup_check", function() end)
            end)
            assert(not dup_ok, "Duplicate validator must throw")
            assert(string.find(tostring(dup_err), "AuthoringValidatorDuplicate"), "Error must mention AuthoringValidatorDuplicate, got: " .. tostring(dup_err))

            -- 5. Declaration after freeze
            M.register()
            local late_ok, late_err = pcall(function()
                M.validate("rh:command.travel", "late_val", function() end)
            end)
            assert(not late_ok, "Validator declaration after freeze must throw")
            assert(string.find(tostring(late_err), "AuthoringValidatorDeclarationAfterFreeze"), "Error must mention AuthoringValidatorDeclarationAfterFreeze, got: " .. tostring(late_err))
        end)
    end,

    validator_stable_id_construction_and_introspection = function()
        authoring_context.with_isolated_context(function()
            local M_rh = authoring_context.gameplay("rh")
            local M_mod = authoring_context.gameplay("curse_mod")

            M_rh.commands.travel = function() end
            M_rh.commands["location.deep.move"] = function() end

            -- Local validator
            local decl1 = M_rh.validate(M_rh.commands.travel, "local_check", function() end)
            assert(decl1.validator_id == "rh:validator.rh.travel.local_check", "Local validator Stable ID must match")

            -- Cross-package validator
            local decl2 = M_mod.validate("rh:command.travel", "cross_check", function() end)
            assert(decl2.validator_id == "curse_mod:validator.rh.travel.cross_check", "Cross-package validator Stable ID must match")

            -- Deep command path validator
            local decl3 = M_mod.validate("rh:command.location.deep.move", "deep_check", function() end)
            assert(decl3.validator_id == "curse_mod:validator.rh.location.deep.move.deep_check", "Deep command path validator Stable ID must match")

            M_rh.register()
            M_mod.register()
            authoring_context.freeze()

            -- Introspection on runtime registry
            assert(game and game.commands and game.commands.validators, "Validators registry must exist")
            local val_entry = game.commands.validators.get("curse_mod:validator.rh.travel.cross_check")
            assert(val_entry ~= nil, "Validator record must be accessible in registry")
            assert(val_entry.target_command_id == "rh:command.travel", "Introspection target_command_id must match")
            assert(val_entry.declaring_package == "curse_mod", "Introspection declaring_package must match")
            assert(val_entry.validator_name == "cross_check", "Introspection validator_name must match")
            assert(val_entry.validator_id == "curse_mod:validator.rh.travel.cross_check", "Introspection validator_id must match")
        end)
    end,

    target_resolution_at_freeze_and_order_independence = function()
        authoring_context.with_isolated_context(function()
            -- 1. Order independence: Module 1 declares validator before Module 2 declares target command
            local M_mod1 = authoring_context.gameplay("curse_mod")
            local M_mod2 = authoring_context.gameplay("rh")

            M_mod1.validate("rh:command.cross_module_cmd", "policy", function() end)
            M_mod2.commands.cross_module_cmd = function()
                return { ok = true, marker = "from_mod2" }
            end

            -- Module 1 registers FIRST
            M_mod1.register()
            -- Module 2 registers SECOND
            M_mod2.register()

            -- Freeze validates targets
            local freeze_ok, freeze_err = pcall(function()
                authoring_context.freeze()
            end)
            assert(freeze_ok, "Freeze must succeed when target command is registered by a later module: " .. tostring(freeze_err))

            local res = M_mod2.commands.cross_module_cmd:run()
            assert(res ~= nil and res.ok == true, "Command should run successfully")

            -- 2. Missing target command detection at freeze
            local M_bad = authoring_context.gameplay("bad_pkg")
            M_bad.validate("rh:command.completely_missing", "policy", function() end)
            M_bad.register()

            local missing_ok, missing_err = pcall(function()
                authoring_context.freeze()
            end)
            assert(not missing_ok, "Freeze must fail when target command does not exist in any module")
            assert(string.find(tostring(missing_err), "AuthoringValidatorTargetMissing"), "Error must mention AuthoringValidatorTargetMissing, got: " .. tostring(missing_err))
            assert(string.find(tostring(missing_err), "rh:command.completely_missing"), "Error must mention target command ID")
            assert(string.find(tostring(missing_err), "bad_pkg"), "Error must mention declaring package")
        end)
    end,

    multiple_validators_and_deterministic_order_first_refusal_wins = function()
        authoring_context.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")

            local log = {}
            M.commands.action_chain = function()
                table.insert(log, "handler")
                return { ok = true }
            end

            M.validate(M.commands.action_chain, "step_1", function()
                table.insert(log, "v1")
            end)

            M.validate(M.commands.action_chain, "step_2", function()
                table.insert(log, "v2")
                M.fail("stopped_at_step_2", {})
            end)

            M.validate(M.commands.action_chain, "step_3", function()
                table.insert(log, "v3")
            end)

            M.register()
            authoring_context.freeze()

            local res = M.commands.action_chain:run()
            assert(res ~= nil and res.ok == false, "Command must be refused by v2")
            assert(res.error.code == "rh:error.stopped_at_step_2", "Error code must match v2 refusal")

            -- Deterministic first-refusal-wins check:
            -- v1 ran, v2 ran and failed -> v3 was NOT called, handler was NOT called
            assert(#log == 2, "Exactly 2 steps must have run, got: " .. tostring(#log))
            assert(log[1] == "v1", "First step must be v1")
            assert(log[2] == "v2", "Second step must be v2")
        end)
    end,

    identical_argument_decoding_for_handler_and_validator = function()
        authoring_context.with_isolated_context(function()
            local M = authoring_context.gameplay("rh")

            local val_args = nil
            local handler_args = nil

            M.commands.multi_args = function(a, b, c)
                handler_args = { a, b, c }
                return { ok = true }
            end

            M.validate(M.commands.multi_args, "check_args", function(a, b, c)
                val_args = { a, b, c }
            end)

            M.register()
            authoring_context.freeze()

            -- 1. Positional arguments
            val_args = nil
            handler_args = nil
            M.commands.multi_args:run("hello", 42, { key = "val" })

            assert(val_args ~= nil and handler_args ~= nil, "Both validator and handler must receive args")
            assert(val_args[1] == "hello" and handler_args[1] == "hello", "Arg 1 must match")
            assert(val_args[2] == 42 and handler_args[2] == 42, "Arg 2 must match")
            assert(type(val_args[3]) == "table" and type(handler_args[3]) == "table", "Arg 3 must be table")
            assert(val_args[3].key == "val" and handler_args[3].key == "val", "Arg 3 contents must match")

            -- 2. Empty arguments
            local M_empty = authoring_context.gameplay("rh_empty")
            local empty_val_count = nil
            local empty_handler_count = nil

            M_empty.commands.no_args = function(...)
                empty_handler_count = select("#", ...)
                return { ok = true }
            end
            M_empty.validate(M_empty.commands.no_args, "check_empty", function(...)
                empty_val_count = select("#", ...)
            end)
            M_empty.register()
            authoring_context.freeze()

            M_empty.commands.no_args:run()
            assert(empty_val_count == 0, "Validator must receive 0 args for empty call, got " .. tostring(empty_val_count))
            assert(empty_handler_count == 0, "Handler must receive 0 args for empty call, got " .. tostring(empty_handler_count))
        end)
    end,

    validate_function_in_authoring_environment = function()
        authoring_context.with_isolated_context(function()
            local mod, env = authoring_context.create_authoring_environment("rh")
            assert(type(env.validate) == "function", "validate must be accessible in authoring _ENV")
            assert(env.validate == mod.validate, "env.validate must refer to mod.validate")

            -- Test declaring via environment
            env.commands.test_env_cmd = function() return { ok = true } end
            env.validate(env.commands.test_env_cmd, "env_val", function() end)

            mod.register()
            authoring_context.freeze()

            local res = env.commands.test_env_cmd:run()
            assert(res ~= nil and res.ok == true, "Command declared via _ENV must run successfully")
        end)
    end,
}
