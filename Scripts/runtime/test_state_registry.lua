-- GAP-003/EGC-04: registry of module-level transient state that must be isolated
-- between Lua spec cases. Infrastructure modules register their own transient
-- upvalues with a snapshot/restore/reset triple; test_isolation.lua and
-- authoring/context.lua sweep the registry instead of hand-enumerating variables.
--
-- The static enumerator that catches a registered-but-forgotten module-level
-- local lives in Tools/Content/validate_test_state_registry.py, not here: the
-- Lua sandbox this module runs in (Source/GV2RuntimeCore/.../OpenEnvironment)
-- opens only base/table/string/math/utf8 -- no `debug`, no `io`, no `load` --
-- so a module cannot inspect its own or another module's upvalues at runtime.
-- The Python checker reads the same source files statically instead.

local M = {
    id = "core:module.runtime.test_state_registry",
}

local participants = {}

function M.register_participant(name, funcs)
    if type(name) ~= "string" or name == "" then
        error("TestStateRegistryInvalidName: participant name must be a non-empty string", 2)
    end
    if participants[name] ~= nil then
        error("TestStateRegistryDuplicateParticipant: '" .. name .. "' is already registered", 2)
    end
    if type(funcs) ~= "table"
        or type(funcs.snapshot) ~= "function"
        or type(funcs.restore) ~= "function"
        or type(funcs.reset) ~= "function"
    then
        error("TestStateRegistryInvalidParticipant: '" .. name .. "' must provide snapshot/restore/reset functions", 2)
    end
    participants[name] = funcs
end

-- Test-only: withdraw a participant to simulate a missing registration (mutation probes).
function M.unregister_for_test(name)
    participants[name] = nil
end

function M.snapshot_all()
    local snap = {}
    for name, funcs in pairs(participants) do
        snap[name] = funcs.snapshot()
    end
    return snap
end

function M.restore_all(snap)
    if not snap then return end
    for name, funcs in pairs(participants) do
        funcs.restore(snap[name])
    end
end

function M.reset_all()
    for _, funcs in pairs(participants) do
        funcs.reset()
    end
end

function M.registered_names()
    local names = {}
    for name in pairs(participants) do
        table.insert(names, name)
    end
    table.sort(names)
    return names
end

return M
