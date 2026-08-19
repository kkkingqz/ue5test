local stable_id = require("core:module.runtime.stable_id")

local M = {
    id = "core:module.runtime.service_registry",
}

function M.create_registry()
    local services_by_id = {}
    local is_frozen = false

    local registry = {}

    function registry.register(id, service_impl)
        if is_frozen then
            error("ServiceRegistryFrozen: cannot register service '" .. tostring(id) .. "' after register phase / freeze", 2)
        end
        if type(id) ~= "string" or not stable_id.is_kind(id, "service") then
            error("InvalidServiceId: service id must be a canonical Stable ID of kind 'service', got '" .. tostring(id) .. "'", 2)
        end
        if services_by_id[id] ~= nil then
            error("ServiceDuplicateRegistration: service '" .. id .. "' is already registered", 2)
        end
        if type(service_impl) ~= "table" then
            error("InvalidServiceImpl: service implementation must be a table", 2)
        end

        services_by_id[id] = service_impl
        return service_impl
    end

    function registry.get(id)
        if type(id) ~= "string" or id == "" then
            return nil
        end
        return services_by_id[id]
    end

    function registry.require(id)
        local s = registry.get(id)
        if s == nil then
            error("ServiceNotFound: service '" .. tostring(id) .. "' not found in registry", 2)
        end
        return s
    end

    function registry.exists(id)
        if type(id) ~= "string" or id == "" then
            return false
        end
        return services_by_id[id] ~= nil
    end

    function registry.list()
        local result = {}
        for id, _ in pairs(services_by_id) do
            table.insert(result, id)
        end
        table.sort(result)
        return result
    end

    function registry.freeze()
        is_frozen = true
    end

    function registry.is_frozen()
        return is_frozen
    end

    local public_facade = {}
    local mt = {
        __index = function(_, k)
            if registry[k] ~= nil then
                return registry[k]
            end
            return services_by_id[k]
        end,
        __newindex = function(_, k, v)
            error("ServiceRegistryDirectAssignmentDisallowed: use game.services.register(id, service) to register a service", 2)
        end,
        __tostring = function(_)
            return "GameplayServiceRegistry"
        end,
    }
    setmetatable(public_facade, mt)
    return public_facade
end

function M.register(_ctx)
    if not game then
        game = {}
    end
    game.services = M.create_registry()
end

function M.with_isolated_services(fn)
    local old_services = game and game.services
    local fresh_registry = M.create_registry()
    if not game then
        game = {}
    end
    game.services = fresh_registry

    local ok, err = pcall(fn)
    game.services = old_services
    if not ok then
        error(err, 0)
    end
end

return M
