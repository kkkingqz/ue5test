-- TSL-11: Semantic Actions and Action Registry Specification
-- Verifies game.actions registry, late-binding, freeze lifecycle, duplicate rejection,
-- and authoring presentation action helper resolution.

local action_registry = require("core:module.runtime.action_registry")
local authoring_presentation = require("core:module.authoring.presentation")

return {
    actions_registry_mounted_and_frozen = function()
        assert(game and game.actions, "game.actions must be mounted on game facade")
        assert(type(game.actions.get) == "function", "game.actions.get must be a function")
        assert(type(game.actions.require) == "function", "game.actions.require must be a function")
        assert(type(game.actions.bind) == "function", "game.actions.bind must be a function")
        assert(game.actions.is_frozen() == true, "game.actions must be frozen during active session")

        local ok, err = pcall(function()
            game.actions.bind("textsystem:action.test.late", "rh:command.travel")
        end)
        assert(not ok, "Binding an action after registry freeze must fail")
        assert(string.find(tostring(err), "ActionRegistryFrozen") ~= nil,
            "Error must contain ActionRegistryFrozen, got: " .. tostring(err))
    end,

    action_binding_lookup_and_resolution = function()
        local reg = action_registry.create_registry()
        assert(reg.is_frozen() == false, "new registry must start unfrozen")

        -- 1. Bind action with string command
        local b1 = reg.bind("textsystem:action.location.travel", "rh:command.travel")
        assert(b1.command_id == "rh:command.travel")
        assert(reg.exists("textsystem:action.location.travel") == true)
        assert(reg.get("textsystem:action.location.travel").command_id == "rh:command.travel")
        assert(reg.require("textsystem:action.location.travel").command_id == "rh:command.travel")

        -- 2. Resolve action with additional arguments
        local resolved = reg.resolve("textsystem:action.location.travel", { target_location_id = "rh:location.city.market" })
        assert(resolved.command_id == "rh:command.travel")
        assert(resolved.args.target_location_id == "rh:location.city.market")

        -- 3. Bind action with table containing default args
        reg.bind("rh:action.buy_sword", {
            command_id = "rh:command.buy",
            args = { item = "rh:item.weapon.iron_sword" },
        })
        local resolved_sword = reg.resolve("rh:action.buy_sword")
        assert(resolved_sword.command_id == "rh:command.buy")
        assert(resolved_sword.args.item == "rh:item.weapon.iron_sword")

        -- 4. List actions
        local list = reg.list()
        assert(#list == 2)
        assert(list[1] == "rh:action.buy_sword")
        assert(list[2] == "textsystem:action.location.travel")
    end,

    duplicate_binding_rejected = function()
        local reg = action_registry.create_registry()
        reg.bind("textsystem:action.location.travel", "rh:command.travel")

        local ok, err = pcall(function()
            reg.bind("textsystem:action.location.travel", "rh:command.other")
        end)
        assert(not ok, "Duplicate action binding must fail")
        assert(string.find(tostring(err), "DuplicateActionBinding") ~= nil,
            "Error must contain DuplicateActionBinding, got: " .. tostring(err))
    end,

    invalid_action_id_and_target_rejected = function()
        local reg = action_registry.create_registry()

        -- Invalid action kind (command instead of action)
        local ok1, err1 = pcall(function()
            reg.bind("textsystem:command.location.travel", "rh:command.travel")
        end)
        assert(not ok1, "Action ID with non-action kind must be rejected")
        assert(string.find(tostring(err1), "InvalidActionId") ~= nil,
            "Error must contain InvalidActionId, got: " .. tostring(err1))

        -- Invalid target kind (location instead of command)
        local ok2, err2 = pcall(function()
            reg.bind("textsystem:action.location.travel", "rh:location.city.market")
        end)
        assert(not ok2, "Target command with non-command kind must be rejected")
        assert(string.find(tostring(err2), "InvalidActionTargetCommand") ~= nil,
            "Error must contain InvalidActionTargetCommand, got: " .. tostring(err2))
    end,

    unbound_action_require_raises_typed_error = function()
        local reg = action_registry.create_registry()
        assert(reg.get("textsystem:action.unbound") == nil)

        local ok, err = pcall(function()
            reg.require("textsystem:action.unbound")
        end)
        assert(not ok, "require on unbound action must fail")
        assert(string.find(tostring(err), "ActionNotBound") ~= nil,
            "Error must contain ActionNotBound, got: " .. tostring(err))
    end,

    presentation_action_helper_resolves_semantic_actions = function()
        local reg = action_registry.create_registry()
        reg.bind("textsystem:action.location.travel", "rh:command.travel")
        reg.bind("rh:action.buy_sword", {
            command_id = "rh:command.buy",
            args = { item = "rh:item.weapon.iron_sword" },
        })

        local prev_actions = game.actions
        game.actions = reg

        local action_helper = authoring_presentation.create_action_helper("rh")

        -- Resolve semantic action with extra arg
        local act1 = action_helper("textsystem:action.location.travel", { target_location_id = "rh:location.city.market" })
        assert(type(act1) == "table", "action must return a table")
        assert(act1.command_id == "rh:command.travel", "command_id must resolve to rh:command.travel")
        assert(act1.args.target_location_id == "rh:location.city.market", "args must contain target_location_id")

        -- Resolve semantic action with default args
        local act2 = action_helper("rh:action.buy_sword")
        assert(act2.command_id == "rh:command.buy")
        assert(act2.args.item == "rh:item.weapon.iron_sword")

        -- Unbound semantic action throws ActionNotBound
        local ok, err = pcall(function()
            action_helper("rh:action.unbound_action")
        end)
        assert(not ok, "unbound semantic action must fail")
        assert(string.find(tostring(err), "ActionNotBound") ~= nil,
            "Error must contain ActionNotBound, got: " .. tostring(err))

        game.actions = prev_actions
    end,
}
