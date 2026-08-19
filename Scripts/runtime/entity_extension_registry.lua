-- Canonical Entity Extension Registry (ADR-0031 / EAE-02..04)
-- Manages registration, conflict validation, and lifecycle freezing of entity extension methods.

local M = {
    id = "core:module.runtime.entity_extension_registry",
}

function M.create_registry()
    local methods_by_kind = {}
    local declarations_list = {}
    local effective_method_tables = {}
    local is_frozen = false

    local registry = {}

    function registry.register(source_module, package_id, entity_kind, method_name, fn)
        if is_frozen then
            error("EntityExtensionRegistryFrozen: cannot register extension method after register phase / freeze", 2)
        end
        if type(source_module) ~= "string" or source_module == "" then
            error("InvalidSourceModule: expected non-empty string source module, got " .. tostring(source_module), 2)
        end
        if type(package_id) ~= "string" or package_id == "" then
            error("InvalidPackageId: expected non-empty string package id, got " .. tostring(package_id), 2)
        end
        if type(entity_kind) ~= "string" or entity_kind == "" then
            error("InvalidEntityKind: expected non-empty string entity kind, got " .. tostring(entity_kind), 2)
        end
        if type(method_name) ~= "string" or method_name == "" then
            error("InvalidMethodName: expected non-empty string method name, got " .. tostring(method_name), 2)
        end
        if type(fn) ~= "function" then
            error("InvalidMethodImplementation: expected function implementation for method '" .. tostring(method_name) .. "' on kind '" .. tostring(entity_kind) .. "', got " .. type(fn), 2)
        end

        local existing = methods_by_kind[entity_kind] and methods_by_kind[entity_kind][method_name]
        if existing ~= nil then
            if existing.source_module ~= source_module or existing.package_id ~= package_id then
                error("entity_extension.method_conflict: method '" .. method_name .. "' on kind '" .. entity_kind .. "' declared by '" .. source_module .. "' (package '" .. package_id .. "') conflicts with existing declaration by '" .. existing.source_module .. "' (package '" .. existing.package_id .. "')", 2)
            else
                existing.fn = fn
                return existing
            end
        end

        local record = {
            source_module = source_module,
            package_id = package_id,
            entity_kind = entity_kind,
            method_name = method_name,
            fn = fn,
        }

        methods_by_kind[entity_kind] = methods_by_kind[entity_kind] or {}
        methods_by_kind[entity_kind][method_name] = record

        declarations_list[entity_kind] = declarations_list[entity_kind] or {}
        table.insert(declarations_list[entity_kind], record)

        return record
    end

    function registry.freeze()
        if is_frozen then
            return
        end
        is_frozen = true

        for entity_kind, methods in pairs(methods_by_kind) do
            local effective = {}
            for method_name, record in pairs(methods) do
                effective[method_name] = record.fn
            end
            local frozen_effective = setmetatable({}, {
                __index = effective,
                __newindex = function(_, _k, _v)
                    error("EffectiveMethodTableFrozen: cannot modify effective method table for kind '" .. tostring(entity_kind) .. "' after freeze", 2)
                end,
                __pairs = function(_)
                    return pairs(effective)
                end,
            })
            effective_method_tables[entity_kind] = frozen_effective
        end
    end

    function registry.is_frozen()
        return is_frozen
    end

    function registry.get_effective_methods(entity_kind)
        if type(entity_kind) ~= "string" or entity_kind == "" then
            return {}
        end
        if effective_method_tables[entity_kind] ~= nil then
            return effective_method_tables[entity_kind]
        end
        if is_frozen then
            local empty_table = setmetatable({}, {
                __index = function() return nil end,
                __newindex = function(_, _k, _v)
                    error("EffectiveMethodTableFrozen: cannot modify effective method table for kind '" .. tostring(entity_kind) .. "' after freeze", 2)
                end,
                __pairs = function(_)
                    return pairs({})
                end,
            })
            effective_method_tables[entity_kind] = empty_table
            return empty_table
        end

        local effective = {}
        local methods = methods_by_kind[entity_kind]
        if methods then
            for method_name, record in pairs(methods) do
                effective[method_name] = record.fn
            end
        end
        return effective
    end

    function registry.get_method(entity_kind, method_name)
        if type(entity_kind) ~= "string" or type(method_name) ~= "string" then
            return nil
        end
        local methods = methods_by_kind[entity_kind]
        if methods and methods[method_name] then
            return methods[method_name].fn
        end
        return nil
    end

    function registry.get_method_info(entity_kind, method_name)
        if type(entity_kind) ~= "string" or type(method_name) ~= "string" then
            return nil
        end
        local methods = methods_by_kind[entity_kind]
        if methods and methods[method_name] then
            return methods[method_name]
        end
        return nil
    end

    function registry.describe(entity_kind)
        local result = {}
        if type(entity_kind) ~= "string" or entity_kind == "" then
            return result
        end
        local methods = methods_by_kind[entity_kind]
        if methods then
            for method_name, record in pairs(methods) do
                table.insert(result, {
                    method = method_name,
                    source = record.source_module,
                    package_id = record.package_id,
                })
            end
            table.sort(result, function(a, b)
                return a.method < b.method
            end)
        end
        return result
    end

    function registry.kinds()
        local result = {}
        for kind, _ in pairs(methods_by_kind) do
            table.insert(result, kind)
        end
        table.sort(result)
        return result
    end

    function registry.clear_for_test()
        methods_by_kind = {}
        declarations_list = {}
        effective_method_tables = {}
        is_frozen = false
    end

    function registry.with_isolated_extensions(fn)
        local prev_methods = methods_by_kind
        local prev_declarations = declarations_list
        local prev_effective = effective_method_tables
        local prev_frozen = is_frozen

        methods_by_kind = {}
        declarations_list = {}
        effective_method_tables = {}
        is_frozen = false

        for kind, methods in pairs(prev_methods) do
            methods_by_kind[kind] = {}
            for m_name, record in pairs(methods) do
                methods_by_kind[kind][m_name] = record
            end
        end
        for kind, decls in pairs(prev_declarations) do
            declarations_list[kind] = {}
            for _, decl in ipairs(decls) do
                table.insert(declarations_list[kind], decl)
            end
        end

        local ok, res_or_err = pcall(fn)

        methods_by_kind = prev_methods
        declarations_list = prev_declarations
        effective_method_tables = prev_effective
        is_frozen = prev_frozen

        if not ok then
            error(res_or_err, 0)
        end
        return res_or_err
    end

    local public_facade = {}
    local mt = {
        __index = function(_, k)
            if registry[k] ~= nil then
                return registry[k]
            end
            return methods_by_kind[k]
        end,
        __newindex = function(_, _k, _v)
            error("EntityExtensionRegistryDirectAssignmentDisallowed: use registry.register(...) or authoring prototypes to register extension methods", 2)
        end,
        __tostring = function(_)
            return "EntityExtensionRegistry"
        end,
    }
    setmetatable(public_facade, mt)
    return public_facade
end

local default_registry = nil

function M.register(_ctx)
    if not game then
        game = {}
    end
    default_registry = M.create_registry()
    game.entity_extensions = default_registry
end

return M
