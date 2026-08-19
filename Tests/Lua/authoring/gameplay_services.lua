-- GSA-01..05: Gameplay Services Authoring Specification (ADR-0034)
-- Verifies:
--   1. Kind 'service' validation in service_registry (GSA-02)
--   2. Atomic declaration via services.<name> in authoring _ENV (GSA-03)
--   3. Rejection of invalid names, non-table impl, non-function fields, distributed declaration (GSA-03)
--   4. Immutability, duplicate detection, freeze check, unknown key, and cross-package resolution (GSA-04)
--   5. Execution scope inheritance (command scope with mutation, validator scope with side-effect guards) (GSA-05)
--   6. fail() and emit() attribution to declaring package (GSA-05)

local authoring_context = require("core:module.authoring.context")
local service_registry = require("core:module.runtime.service_registry")

return {
    -- ------------------------------------------------------------------------
    -- GSA-02: service_registry validation
    -- ------------------------------------------------------------------------

    service_registry_rejects_invalid_kind = function()
        authoring_context.with_isolated_context(function()
            local reg = service_registry.create_registry()
            local ok, err = pcall(function()
                reg.register("core:validator.invalid", { op = function() end })
            end)
            assert(not ok, "registering non-service kind must fail")
            assert(string.find(tostring(err), "InvalidServiceId"), "expected InvalidServiceId, got: " .. tostring(err))

            local ok2, err2 = pcall(function()
                reg.register("invalid_plain_id", { op = function() end })
            end)
            assert(not ok2, "registering plain string id must fail")
            assert(string.find(tostring(err2), "InvalidServiceId"), "expected InvalidServiceId, got: " .. tostring(err2))
        end)
    end,

    service_registry_accepts_canonical_service_id = function()
        authoring_context.with_isolated_context(function()
            local reg = service_registry.create_registry()
            local impl = { op = function() return 42 end }
            local registered = reg.register("rh:service.trade", impl)
            assert(registered ~= nil, "service must be registered")
            assert(reg.exists("rh:service.trade") == true, "exists must return true")
            assert(reg.get("rh:service.trade").op() == 42, "get must return registered service")
        end)
    end,

    service_registry_rejects_duplicate_and_frozen = function()
        authoring_context.with_isolated_context(function()
            local reg = service_registry.create_registry()
            reg.register("rh:service.trade", { op = function() end })

            local ok_dup, err_dup = pcall(function()
                reg.register("rh:service.trade", { op = function() end })
            end)
            assert(not ok_dup, "duplicate registration must fail")
            assert(string.find(tostring(err_dup), "ServiceDuplicateRegistration"), "expected ServiceDuplicateRegistration, got: " .. tostring(err_dup))

            reg.freeze()
            local ok_frz, err_frz = pcall(function()
                reg.register("rh:service.craft", { op = function() end })
            end)
            assert(not ok_frz, "registration after freeze must fail")
            assert(string.find(tostring(err_frz), "ServiceRegistryFrozen"), "expected ServiceRegistryFrozen, got: " .. tostring(err_frz))
        end)
    end,

    -- ------------------------------------------------------------------------
    -- GSA-03: Authoring _ENV declaration and validation
    -- ------------------------------------------------------------------------

    authoring_declares_and_calls_service = function()
        authoring_context.with_isolated_context(function()
            local _, env = authoring_context.create_authoring_environment("rh")
            env.services.trade = {
                buy = function(buyer, seller, amount)
                    return { buyer = buyer, seller = seller, amount = amount }
                end,
            }

            assert(game.services.exists("rh:service.trade"), "rh:service.trade must be registered in game.services")
            assert(env.services.trade ~= nil, "services.trade must be accessible in env")
            local res = env.services.trade.buy("hero", "merchant", 50)
            assert(res.amount == 50, "service method must return expected result")
        end)
    end,

    authoring_rejects_invalid_service_name = function()
        authoring_context.with_isolated_context(function()
            local _, env = authoring_context.create_authoring_environment("rh")
            local ok, err = pcall(function()
                env.services["TradeService"] = { buy = function() end }
            end)
            assert(not ok, "invalid service name (uppercase) must fail")
            assert(string.find(tostring(err), "InvalidServiceName"), "expected InvalidServiceName, got: " .. tostring(err))

            local ok2, err2 = pcall(function()
                env.services["123"] = { buy = function() end }
            end)
            assert(not ok2, "invalid service name (starting with digit) must fail")
            assert(string.find(tostring(err2), "InvalidServiceName"), "expected InvalidServiceName, got: " .. tostring(err2))
        end)
    end,

    authoring_rejects_non_table_implementation = function()
        authoring_context.with_isolated_context(function()
            local _, env = authoring_context.create_authoring_environment("rh")
            local ok, err = pcall(function()
                env.services.trade = "not a table"
            end)
            assert(not ok, "non-table service impl must fail")
            assert(string.find(tostring(err), "InvalidServiceImplementation"), "expected InvalidServiceImplementation, got: " .. tostring(err))
        end)
    end,

    authoring_rejects_non_function_field = function()
        authoring_context.with_isolated_context(function()
            local _, env = authoring_context.create_authoring_environment("rh")
            local ok, err = pcall(function()
                env.services.trade = {
                    tax_rate = 0.05,
                    buy = function() end,
                }
            end)
            assert(not ok, "service table with non-function field must fail")
            assert(string.find(tostring(err), "ServiceFieldNotFunction"), "expected ServiceFieldNotFunction, got: " .. tostring(err))
        end)
    end,

    authoring_rejects_distributed_declaration = function()
        authoring_context.with_isolated_context(function()
            local _, env = authoring_context.create_authoring_environment("rh")
            local ok, err = pcall(function()
                env.services.trade.buy = function() end
            end)
            assert(not ok, "distributed declaration on undeclared service must fail")
            assert(string.find(tostring(err), "UnknownServiceKey"), "expected UnknownServiceKey, got: " .. tostring(err))
        end)
    end,

    -- ------------------------------------------------------------------------
    -- GSA-04: Immutability, duplicates, freeze, and cross-package resolution
    -- ------------------------------------------------------------------------

    authoring_service_immutable_after_registration = function()
        authoring_context.with_isolated_context(function()
            local _, env = authoring_context.create_authoring_environment("rh")
            env.services.trade = {
                buy = function() return true end,
            }

            local ok, err = pcall(function()
                env.services.trade.buy = function() return false end
            end)
            assert(not ok, "mutating registered service field must fail")
            assert(string.find(tostring(err), "ServiceImmutableAfterRegistration"), "expected ServiceImmutableAfterRegistration, got: " .. tostring(err))

            local ok2, err2 = pcall(function()
                env.services.trade.sell = function() return true end
            end)
            assert(not ok2, "adding new field to registered service must fail")
            assert(string.find(tostring(err2), "ServiceImmutableAfterRegistration"), "expected ServiceImmutableAfterRegistration, got: " .. tostring(err2))
        end)
    end,

    authoring_service_duplicate_declaration_rejected = function()
        authoring_context.with_isolated_context(function()
            local _, env = authoring_context.create_authoring_environment("rh")
            env.services.trade = { buy = function() end }

            local ok, err = pcall(function()
                env.services.trade = { buy = function() end }
            end)
            assert(not ok, "duplicate service declaration in same package must fail")
            assert(string.find(tostring(err), "ServiceDuplicateDeclaration"), "expected ServiceDuplicateDeclaration, got: " .. tostring(err))
        end)
    end,

    authoring_service_declaration_after_freeze_rejected = function()
        authoring_context.with_isolated_context(function()
            local mod, env = authoring_context.create_authoring_environment("rh")
            mod.freeze()

            local ok, err = pcall(function()
                env.services.trade = { buy = function() end }
            end)
            assert(not ok, "declaring service after freeze must fail")
            assert(string.find(tostring(err), "ServiceDeclarationAfterFreeze"), "expected ServiceDeclarationAfterFreeze, got: " .. tostring(err))
        end)
    end,

    authoring_unknown_service_key_rejected = function()
        authoring_context.with_isolated_context(function()
            local _, env = authoring_context.create_authoring_environment("rh")
            local ok, err = pcall(function()
                return env.services.nonexistent
            end)
            assert(not ok, "accessing undeclared service key must fail")
            assert(string.find(tostring(err), "UnknownServiceKey"), "expected UnknownServiceKey, got: " .. tostring(err))
        end)
    end,

    authoring_cross_package_reference_resolved_on_freeze = function()
        authoring_context.with_isolated_context(function()
            -- Module A in package "core_game" references "economy:service.calc"
            local _, env_a = authoring_context.create_authoring_environment("core_game")
            local calc_proxy = env_a.services["economy:service.calc"]
            assert(calc_proxy ~= nil, "cross-package reference proxy must be returned")

            -- Module B in package "economy" declares "economy:service.calc"
            local mod_b, env_b = authoring_context.create_authoring_environment("economy")
            env_b.services.calc = {
                add = function(a, b) return a + b end,
            }
            mod_b.register()

            -- Freeze authoring context: all referenced services exist -> success
            authoring_context.freeze()
            assert(calc_proxy.add(10, 20) == 30, "resolved cross-package service call must succeed")
        end)
    end,

    authoring_cross_package_reference_missing_target_throws_on_freeze = function()
        authoring_context.with_isolated_context(function()
            local _, env_a = authoring_context.create_authoring_environment("core_game")
            local _ = env_a.services["missing_pkg:service.missing"]

            local ok, err = pcall(function()
                authoring_context.freeze()
            end)
            assert(not ok, "freeze must fail when referenced cross-package service is missing")
            assert(string.find(tostring(err), "ServiceTargetMissing"), "expected ServiceTargetMissing, got: " .. tostring(err))
            assert(string.find(tostring(err), "core_game"), "error must mention requesting package, got: " .. tostring(err))
            assert(string.find(tostring(err), "missing_pkg:service.missing"), "error must mention target service id, got: " .. tostring(err))
        end)
    end,

    -- ------------------------------------------------------------------------
    -- GSA-05: Scope inheritance and attribution
    -- ------------------------------------------------------------------------

    service_called_in_command_inherits_scope_and_mutates_state = function()
        authoring_context.with_isolated_context(function()
            local mod, env = authoring_context.create_authoring_environment("rh")
            env.services.economy = {
                transfer_gold = function(actor, amount)
                    actor.gold = (actor.gold or 0) + amount
                    return actor.gold
                end,
            }

            env.commands.give_gold = function(amount)
                return env.services.economy.transfer_gold(env.player, amount)
            end

            mod.register()
            authoring_context.freeze()

            local prev_gold = env.player.gold or 0
            local res = env.commands.give_gold:run(25)
            assert(res ~= nil and res.ok == true, "command invoking service must succeed")
            assert(env.player.gold == prev_gold + 25, "player gold must increase by 25")
        end)
    end,

    service_called_in_validator_inherits_scope_and_guards_side_effects = function()
        authoring_context.with_isolated_context(function()
            local mod, env = authoring_context.create_authoring_environment("rh")
            env.services.unsafe = {
                try_emit = function()
                    env.emit("something_happened", {})
                end,
            }

            env.commands.target_cmd = function() return { ok = true } end

            env.validate(env.commands.target_cmd, "check_emit", function()
                env.services.unsafe.try_emit()
            end)

            mod.register()
            authoring_context.freeze()

            local ok, err = pcall(function()
                env.commands.target_cmd:run()
            end)
            assert(not ok, "validator calling service that attempts emit must throw error")
            assert(string.find(tostring(err), "AuthoringValidatorSideEffectDisallowed"),
                "expected AuthoringValidatorSideEffectDisallowed, got: " .. tostring(err))
            assert(string.find(tostring(err), "rh:validator.rh.target_cmd.check_emit"),
                "expected validator id in error, got: " .. tostring(err))
            assert(string.find(tostring(err), "emit"),
                "expected operation name in error, got: " .. tostring(err))
        end)
    end,

    service_fail_and_emit_attributed_to_declaring_package = function()
        authoring_context.with_isolated_context(function()
            local captured_event = nil
            -- Package "trade_system" declares service with fail() and emit()
            local mod_trade, env_trade = authoring_context.create_authoring_environment("trade_system")
            env_trade.services.market = {
                purchase = function(buyer_gold, price)
                    if buyer_gold < price then
                        env_trade.fail("insufficient_funds", { required = price, actual = buyer_gold })
                    end
                    env_trade.emit("purchase_completed", { price = price })
                    return true
                end,
            }
            env_trade.on("purchase_completed", function(payload, ev_env)
                captured_event = ev_env
            end)
            mod_trade.register()

            -- Package "rh_game" calls trade_system service from its own command
            local mod_rh, env_rh = authoring_context.create_authoring_environment("rh_game")
            env_rh.commands.buy_item = function(gold, price)
                return env_rh.services["trade_system:service.market"].purchase(gold, price)
            end
            mod_rh.register()

            authoring_context.freeze()

            -- 1. Test fail() attribution to declaring package "trade_system"
            local fail_res = env_rh.commands.buy_item:run(5, 20)
            assert(fail_res ~= nil and fail_res.ok == false, "command must fail when service calls fail()")
            assert(fail_res.error ~= nil, "error envelope must be present")
            assert(fail_res.error.code == "trade_system:error.insufficient_funds",
                "fail() error code must be attributed to declaring package trade_system, got: " .. tostring(fail_res.error.code))
            assert(fail_res.error.params.required == 20, "error params must match")

            -- 2. Test emit() attribution to declaring package "trade_system"
            local ok_res = env_rh.commands.buy_item:run(50, 20)
            assert(ok_res ~= nil and ok_res.ok == true, "command must succeed when funds are sufficient")
            assert(captured_event ~= nil, "event subscriber must receive event")
            assert(captured_event.event_id == "trade_system:event.purchase_completed",
                "event_id must be attributed to declaring package trade_system, got: " .. tostring(captured_event.event_id))
        end)
    end,
}
