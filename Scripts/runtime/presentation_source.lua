-- Presentation Source Registry (SAS-14, SAS-15, SAS-16)
-- Registers the package presentation source function during the "register" phase.
-- Invoked automatically by runtime after successful command commits outside the mutation window.

local screens_module = require("core:module.presentation.screen_requests")

local M = {
    id = "core:module.runtime.presentation_source",
}

local registered_source = nil
local is_frozen = false

function M.register_source(fn)
    if is_frozen then
        error("PresentationSourceRegistryFrozen: cannot register presentation source after register phase / freeze", 2)
    end
    if type(fn) ~= "function" then
        error("InvalidPresentationSource: presentation source must be a function, got " .. type(fn), 2)
    end
    if registered_source ~= nil then
        error("PresentationSourceDuplicateRegistration: presentation source is already registered", 2)
    end
    registered_source = fn
end

function M.get_source()
    return registered_source
end

function M.has_source()
    return registered_source ~= nil
end

function M.resolve()
    if registered_source ~= nil then
        local res = registered_source()
        if type(res) == "table" and res.screen_id and res.fields then
            screens_module.publish(res)
        end
        return res
    end
    return nil
end

function M.freeze()
    is_frozen = true
end

function M.is_frozen()
    return is_frozen
end

function M.clear_for_test()
    registered_source = nil
    is_frozen = false
end

function M.register(_ctx)
    if not game then
        game = {}
    end
    if not game.presentation then
        game.presentation = {}
    end
    game.presentation.register_source = M.register_source
    game.presentation.get_source = M.get_source
    game.presentation.has_source = M.has_source
    game.presentation.resolve = M.resolve
    game.presentation.freeze = M.freeze
    game.presentation.is_frozen = M.is_frozen
    game.presentation.clear_for_test = M.clear_for_test
end

function M.start(_ctx)
    M.resolve()
end

return M
