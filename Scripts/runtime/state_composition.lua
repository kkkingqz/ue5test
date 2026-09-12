local state_validator = require("core:module.runtime.state_validator")

local M = {
    id = "core:module.runtime.state_composition",
}

function M.merge_contribution(target_tree, contrib, module_id)
    local colon_pos = string.find(module_id, ":", 1, true)
    local mod_namespace = colon_pos and string.sub(module_id, 1, colon_pos - 1) or module_id

    for section_key, section_val in pairs(contrib) do
        if type(section_key) ~= "string" then
            return false, {
                code = "LuaModuleDefaultStateInvalid",
                message = "State contribution section keys must be strings: " .. tostring(module_id),
            }
        end

        if not state_validator.is_canonical_section(section_key) then
            return false, {
                code = "LuaModuleDefaultStateInvalid",
                message = "Unknown canonical state section '" .. tostring(section_key) .. "' in module contribution: " .. tostring(module_id),
            }
        end

        if type(section_val) ~= "table" then
            return false, {
                code = "LuaModuleDefaultStateInvalid",
                message = "State contribution section '" .. tostring(section_key) .. "' must be a table: " .. tostring(module_id),
            }
        end

        if getmetatable(section_val) ~= nil then
            return false, {
                code = "LuaStateValidationInvalid",
                message = "State contribution section '" .. tostring(section_key) .. "' cannot have a metatable: " .. tostring(module_id),
            }
        end

        local target_section = target_tree[section_key]
        if target_section == nil then
            target_section = {}
            target_tree[section_key] = target_section
        end

        for sub_key, sub_val in pairs(section_val) do
            -- Mod section isolation check
            if section_key == "mods" then
                if type(sub_key) ~= "string" then
                    return false, {
                        code = "LuaModuleDefaultStateInvalid",
                        message = "Mod state keys must be string mod IDs: " .. tostring(module_id),
                    }
                end
                if sub_key ~= mod_namespace and sub_key ~= module_id then
                    return false, {
                        code = "LuaModuleDefaultStateInvalid",
                        message = "Module '" .. tostring(module_id) .. "' attempted to contribute to forbidden mod section '" .. tostring(sub_key) .. "'",
                    }
                end
            end

            -- Special nested tables in meta (instance_counters, prng, time)
            local handled_nested = false
            if section_key == "meta" and type(sub_key) == "string" and type(sub_val) == "table" then
                if sub_key == "instance_counters" or sub_key == "prng" or sub_key == "time" then
                    local target_sub_table = target_section[sub_key]
                    if type(target_sub_table) == "table" then
                        for nested_k, nested_v in pairs(sub_val) do
                            target_sub_table[nested_k] = nested_v
                        end
                        handled_nested = true
                    end
                end
            end

            if not handled_nested then
                -- Collision / override check (exempting default meta primitives)
                local skip_collision = false
                if section_key == "meta" and type(sub_key) == "string" then
                    if sub_key == "schema_version" or sub_key == "save_version" or sub_key == "save_id" or sub_key == "seed_hex" then
                        skip_collision = true
                    end
                end

                if target_section[sub_key] ~= nil and not skip_collision then
                    local colliding_key = tostring(sub_key)
                    return false, {
                        code = "LuaModuleDefaultStateInvalid",
                        message = "Duplicate state contribution key '" .. colliding_key .. "' in section '" .. tostring(section_key) .. "' from module: " .. tostring(module_id),
                    }
                end

                target_section[sub_key] = sub_val
            end
        end
    end

    return true, nil
end

function M.compose_default_state(ctx, modules)
    ctx = ctx or {}
    local target_tree = state_validator.create_empty_canonical_state()

    if modules == nil then
        return target_tree, nil
    end

    for _, entry in ipairs(modules) do
        local module_id = entry.module_id or "unknown"
        local mod = entry.module
        local hook = entry.create_default_state

        if hook == nil and mod ~= nil then
            if type(mod) ~= "table" then
                return nil, {
                    code = "LuaModuleLifecycleError",
                    message = "Module export table is missing: " .. tostring(module_id),
                }
            end
            hook = mod.create_default_state
        end

        if hook ~= nil then
            if type(hook) ~= "function" then
                return nil, {
                    code = "LuaModuleLifecycleInvalid",
                    message = "Module hook 'create_default_state' must be a function: " .. tostring(module_id),
                }
            end

            local ok, contrib_or_err = pcall(hook, ctx)
            if not ok then
                return nil, {
                    code = "LuaModuleLifecycleError",
                    message = "Module create_default_state failed: " .. tostring(contrib_or_err),
                }
            end

            local contrib = contrib_or_err
            if contrib ~= nil then
                if type(contrib) ~= "table" then
                    return nil, {
                        code = "LuaModuleDefaultStateInvalid",
                        message = "Module create_default_state must return a table or nil: " .. tostring(module_id),
                    }
                end

                local merge_ok, merge_err = M.merge_contribution(target_tree, contrib, module_id)
                if not merge_ok then
                    return nil, merge_err
                end
            end
        end
    end

    return target_tree, nil
end

return M
