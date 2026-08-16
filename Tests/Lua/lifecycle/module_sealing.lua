-- PKG-11: Module Sealing Specification (ADR-0025, LuaRuntimeContract.md)
-- Verifies that module export tables are immutable after initialization:
-- writing new fields or overwriting existing fields triggers LuaModuleExportFrozen,
-- while reading, calling methods, pairs iteration, and prototype delegation work.

local stable_id = require("core:module.runtime.stable_id")
local mutation_window = require("core:module.runtime.mutation_window")

return {
    cannot_add_new_field_to_export_table = function()
        local ok, err = pcall(function()
            stable_id.new_field = "illegal"
        end)
        assert(not ok, "Adding new field to module export table must fail")
        assert(string.find(tostring(err), "LuaModuleExportFrozen") ~= nil,
            "Error must contain LuaModuleExportFrozen, got: " .. tostring(err))
    end,

    cannot_overwrite_existing_field_in_export_table = function()
        local ok, err = pcall(function()
            stable_id.is_kind = function() return false end
        end)
        assert(not ok, "Overwriting existing function in module export table must fail")
        assert(string.find(tostring(err), "LuaModuleExportFrozen") ~= nil,
            "Error must contain LuaModuleExportFrozen, got: " .. tostring(err))
    end,

    cannot_modify_export_table_metatable = function()
        local mt = getmetatable(stable_id)
        assert(mt == false, "getmetatable must return false for frozen export table")

        local ok, err = pcall(function()
            setmetatable(stable_id, {})
        end)
        assert(not ok, "setmetatable on frozen export table must fail")
    end,

    can_read_and_call_module_functions = function()
        assert(stable_id.id == "core:module.runtime.stable_id", "module id must match")
        assert(stable_id.is_kind("core:actor.player", "actor") == true, "is_kind must return true for valid id")
        assert(stable_id.is_kind("core:actor.player", "location") == false, "is_kind must return false for mismatched kind")
    end,

    pairs_iteration_works_on_export_table = function()
        local keys = {}
        for k, v in pairs(stable_id) do
            keys[k] = true
        end
        assert(keys["id"] == true, "pairs iteration must expose id")
        assert(keys["is_kind"] == true, "pairs iteration must expose is_kind")
    end,

    export_table_works_as_prototype_index = function()
        local derived = setmetatable({ custom_field = 42 }, { __index = stable_id })
        assert(derived.custom_field == 42, "Derived table has its own fields")
        assert(type(derived.is_kind) == "function", "Derived table resolves base functions via __index")
        assert(derived.is_kind("core:screen.main", "screen") == true, "Calling base function through derived works")

        -- Modifying derived table succeeds and does not touch base
        derived.new_custom_field = "derived_val"
        assert(derived.new_custom_field == "derived_val", "Can write to derived table")
    end,
}
