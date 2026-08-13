local stable_id = require("core:module.runtime.stable_id")

local M = {}

function M.reference(resource_id)
    assert(stable_id.is_kind(resource_id, "resource"), "resource_id must be canonical")
    return { resource_id = resource_id }
end

function M.prepare_request(resource_ids)
    assert(type(resource_ids) == "table", "resource_ids must be a table")

    local unique_ids = {}
    local seen = {}
    for _, resource_id in ipairs(resource_ids) do
        assert(stable_id.is_kind(resource_id, "resource"), "resource_id must be canonical")
        if not seen[resource_id] then
            seen[resource_id] = true
            unique_ids[#unique_ids + 1] = resource_id
        end
    end
    return { resource_ids = unique_ids }
end

return M
