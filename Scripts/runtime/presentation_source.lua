-- Presentation Source Registry (SAS-14, SAS-15, SAS-16)
-- Registers the package presentation source function during the "register" phase.
-- Invoked automatically by runtime after successful command commits outside the mutation window.

local screens_module = require("core:module.presentation.screen_requests")

local M = {
    id = "core:module.runtime.presentation_source",
}

function M.create_registry()
    local registry = {}
    local registered_source = nil
    local is_frozen = false

    function registry.register_source(fn)
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

    function registry.get_source()
        return registered_source
    end

    function registry.has_source()
        return registered_source ~= nil
    end

    function registry.resolve()
        if registered_source ~= nil then
            local res = registered_source()
            -- Authoring show_screen() returns an already-published UiDocument. Its
            -- metatable exposes route fields for legacy reads, so test its native
            -- document shape first and never publish it a second time.
            local is_document = type(res) == "table"
                and res.ui_instance_id ~= nil
                and res.revision ~= nil
                and res.route ~= nil
            if not is_document and type(res) == "table" and res.screen_id and res.fields then
                screens_module.publish(res)
            end
            return res
        end
        return nil
    end

    function registry.freeze()
        is_frozen = true
    end

    function registry.is_frozen()
        return is_frozen
    end

    function registry.clear_for_test()
        registered_source = nil
        is_frozen = false
    end

    return registry
end

local default_registry = nil

local function get_reg()
    if game and game.presentation and game.presentation.register_source then
        return game.presentation
    end
    if not default_registry then
        default_registry = M.create_registry()
    end
    return default_registry
end

function M.register_source(fn)
    return get_reg().register_source(fn)
end

function M.get_source()
    return get_reg().get_source()
end

function M.has_source()
    return get_reg().has_source()
end

function M.resolve()
    return get_reg().resolve()
end

function M.freeze()
    return get_reg().freeze()
end

function M.is_frozen()
    return get_reg().is_frozen()
end

function M.clear_for_test()
    if default_registry then
        default_registry.clear_for_test()
    end
    if game and game.presentation and game.presentation.clear_for_test then
        game.presentation.clear_for_test()
    end
end

function M.register(_ctx)
    -- CFC-05: game.presentation is installed into the game facade by
    -- core:module.bootstrap.registry_lifecycle before module register hooks run.
end

function M.start(_ctx)
    M.resolve()
end

return M
