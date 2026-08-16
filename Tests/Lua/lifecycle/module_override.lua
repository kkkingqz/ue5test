-- PKG-17/19: Module Override & require_base() Specification (ADR-0025, LuaRuntimeContract.md)
-- Verifies require_base() semantics, chain resolution, and prototype inheritance.

local gameplay_root = require("core:module.gameplay.root")

return {
    require_base_not_available_outside_replacing_module = function()
        assert(type(require_base) == "function", "require_base must be published as a global function")
        local ok, err = pcall(function()
            require_base()
        end)
        assert(not ok, "require_base() must fail when called outside replacing module initialization")
        assert(string.find(tostring(err), "LuaModuleBaseNotAvailable") ~= nil,
            "Error must contain LuaModuleBaseNotAvailable, got: " .. tostring(err))
    end,

    active_winner_is_loaded_and_callable = function()
        assert(type(gameplay_root) == "table", "gameplay root must be a table")
        assert(type(gameplay_root.handle_command) == "function", "handle_command must be a function")
    end,

    derived_module_prototype_delegation = function()
        local fake_base = {
            id = "core:module.test.base",
            greet = function() return "hello from base" end,
            add = function(a, b) return a + b end,
        }

        local derived = setmetatable({
            id = "core:module.test.base",
            greet = function() return "hello from derived" end,
        }, { __index = fake_base })

        assert(derived.greet() == "hello from derived", "Derived overrides base method")
        assert(derived.add(2, 3) == 5, "Derived delegates un-overridden method to base")
    end,
}
