-- Migrations (SAV-18/19/20, plan SaveAndLoad, ADR-0021).
-- Section versions and the migrate_state lifecycle hook. A migration is
-- identified by (section_id, from_version, to_version) and operates on the
-- temporary tree core:module.runtime.load decoded — never on the original
-- container bytes, never on the live game.state.

local M = {
    id = "core:module.runtime.migrate",
}

-- SAV-18: the version this build understands each canonical section at.
-- Bump exactly one entry when that section's saved shape changes, and add
-- a migrate_state hook (on any module) that recognizes the old version.
M.CURRENT_SECTION_VERSIONS = {
    meta = 1,
    player = 1,
    actors = 1,
    item_instances = 1,
    world = 1,
    quests = 1,
    mods = 1,
}

-- SAV-18/20: given the section_versions map recorded in a save envelope,
-- returns (pending, nil) — a deterministically ordered list of
-- { section_id, from_version, to_version, handled = false } for every
-- section older than this build's version — or (nil, typed_error) if any
-- section's saved version is newer than this build understands (a
-- downgrade) or malformed.
function M.plan_migrations(saved_section_versions)
    saved_section_versions = saved_section_versions or {}
    if type(saved_section_versions) ~= "table" then
        return nil, "MigrationDowngradeUnsupported"
    end

    local section_ids = {}
    for section_id, _ in pairs(M.CURRENT_SECTION_VERSIONS) do
        section_ids[#section_ids + 1] = section_id
    end
    table.sort(section_ids)

    local pending = {}
    for _, section_id in ipairs(section_ids) do
        local current = M.CURRENT_SECTION_VERSIONS[section_id]
        local saved = saved_section_versions[section_id]
        if saved == nil then
            saved = current
        end
        if type(saved) ~= "number" or math.type(saved) ~= "integer" or saved > current then
            return nil, "MigrationDowngradeUnsupported"
        end
        if saved < current then
            pending[#pending + 1] = {
                section_id = section_id,
                from_version = saved,
                to_version = current,
                handled = false,
            }
        end
    end
    return pending, nil
end

-- SAV-20: called once, after every module's migrate_state hook has had a
-- chance to run. A pending migration nobody marked handled is rejected
-- explicitly, not silently left at a stale version.
function M.verify_complete()
    local pending = game and game.runtime and game.runtime.pending_section_migrations
    if pending == nil then
        return nil
    end
    for _, entry in ipairs(pending) do
        if not entry.handled then
            return "MigrationMissing:" .. entry.section_id .. ":" .. tostring(entry.from_version) .. "->" .. tostring(entry.to_version)
        end
    end
    return nil
end

return M
