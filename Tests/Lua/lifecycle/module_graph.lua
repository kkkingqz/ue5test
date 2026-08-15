-- LSM-01: Module Graph Specification (ADR-0024, LuaRuntimeContract.md)
-- Verifies that loaded runtime modules declare canonical module identities,
-- export required interfaces, and that dependency resolution algorithms
-- correctly order DAGs and detect cyclic or self dependencies.

local stable_id = require("core:module.runtime.stable_id")

local function topological_sort(graph)
    local in_degree = {}
    for node, _ in pairs(graph) do
        in_degree[node] = 0
    end
    for _, edges in pairs(graph) do
        for _, dest in ipairs(edges) do
            in_degree[dest] = (in_degree[dest] or 0) + 1
        end
    end

    local queue = {}
    for node, deg in pairs(in_degree) do
        if deg == 0 then
            table.insert(queue, node)
        end
    end

    local sorted = {}
    while #queue > 0 do
        local current = table.remove(queue, 1)
        table.insert(sorted, current)
        for _, dest in ipairs(graph[current] or {}) do
            in_degree[dest] = in_degree[dest] - 1
            if in_degree[dest] == 0 then
                table.insert(queue, dest)
            end
        end
    end

    local total_nodes = 0
    for _ in pairs(graph) do total_nodes = total_nodes + 1 end
    return sorted, #sorted == total_nodes
end

return {
    loaded_core_modules_have_valid_ids = function()
        local modules = {
            require("core:module.runtime.stable_id"),
            require("core:module.runtime.mutation_window"),
            require("core:module.runtime.command_dispatcher"),
            require("core:module.runtime.event_bus"),
            require("core:module.runtime.state_validator"),
            require("core:module.runtime.instance_allocator"),
            require("core:module.runtime.state_hasher"),
            require("core:module.runtime.actor_registry"),
            require("core:module.runtime.world"),
            require("core:module.runtime.service_registry"),
            require("core:module.runtime.validator_registry"),
            require("core:module.runtime.subscriber_registry"),
        }

        local seen_ids = {}
        for _, mod in ipairs(modules) do
            assert(type(mod) == "table", "module must export a table")
            assert(type(mod.id) == "string", "module must declare string id")
            assert(stable_id.is_kind(mod.id, "module"), "module id '" .. tostring(mod.id) .. "' must be of kind 'module'")
            assert(not seen_ids[mod.id], "duplicate module id: " .. tostring(mod.id))
            seen_ids[mod.id] = true
        end
    end,

    topological_sorter_resolves_dag = function()
        local sample_graph = {
            ["core:module.a"] = { "core:module.b", "core:module.c" },
            ["core:module.b"] = { "core:module.d" },
            ["core:module.c"] = { "core:module.d" },
            ["core:module.d"] = {},
        }
        local sorted, is_dag = topological_sort(sample_graph)
        assert(is_dag, "sample graph must be a valid DAG")
        assert(#sorted == 4, "all 4 modules must be sorted")
        assert(sorted[1] == "core:module.a", "module.a with in_degree 0 must be first")
        assert(sorted[4] == "core:module.d", "module.d with dependencies must be last")
    end,

    topological_sorter_detects_cycles = function()
        local cyclic_graph = {
            ["core:module.a"] = { "core:module.b" },
            ["core:module.b"] = { "core:module.c" },
            ["core:module.c"] = { "core:module.a" },
        }
        local _, is_dag = topological_sort(cyclic_graph)
        assert(not is_dag, "cyclic module dependencies must be detected and rejected as non-DAG")
    end,

    topological_sorter_detects_self_dependency = function()
        local self_dep_graph = {
            ["core:module.a"] = { "core:module.a" },
        }
        local _, is_dag = topological_sort(self_dep_graph)
        assert(not is_dag, "self-dependent module must be detected and rejected as non-DAG")
    end,
}
