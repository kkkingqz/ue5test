-- SAV-18/19/20: core:module.runtime.migrate — section version planning and
-- completeness verification. Runs on the real production session; every
-- case restores game.runtime.pending_section_migrations to nil afterward
-- so it stays self-contained and order-independent.

local migrate = require("core:module.runtime.migrate")

return {
    -- SAV-18
    plan_migrations_returns_empty_pending_when_all_current = function()
        local pending, err = migrate.plan_migrations(migrate.CURRENT_SECTION_VERSIONS)
        assert(pending ~= nil, "a save at exactly the current versions must not error, got: " .. tostring(err))
        assert(#pending == 0, "no section should be pending when saved versions equal current")
    end,

    -- SAV-18
    plan_migrations_returns_empty_pending_when_versions_absent = function()
        -- A section_versions map missing entirely (or missing a section)
        -- is treated as "already at current" — the same convention
        -- core:module.runtime.save used before section_versions existed.
        local pending, err = migrate.plan_migrations({})
        assert(pending ~= nil, "a missing section_versions map must not error, got: " .. tostring(err))
        assert(#pending == 0, "an absent section entry must default to current, not pending")
    end,

    -- SAV-18/19
    plan_migrations_lists_older_sections_as_pending_in_deterministic_order = function()
        local saved = {}
        for section_id, version in pairs(migrate.CURRENT_SECTION_VERSIONS) do
            saved[section_id] = version
        end
        -- Force exactly two sections older than current, if there are at
        -- least two declared sections (there always are — meta plus more).
        local section_ids = {}
        for section_id, _ in pairs(migrate.CURRENT_SECTION_VERSIONS) do
            section_ids[#section_ids + 1] = section_id
        end
        table.sort(section_ids)
        saved[section_ids[1]] = migrate.CURRENT_SECTION_VERSIONS[section_ids[1]] - 1
        if migrate.CURRENT_SECTION_VERSIONS[section_ids[1]] < 1 then
            error("test assumption violated: CURRENT_SECTION_VERSIONS must be >= 1")
        end

        local pending, err = migrate.plan_migrations(saved)
        assert(pending ~= nil, "got: " .. tostring(err))
        assert(#pending == 1, "exactly one section was forced older, expected exactly one pending entry")
        assert(pending[1].section_id == section_ids[1], "pending entry must name the section that is older")
        assert(pending[1].from_version == migrate.CURRENT_SECTION_VERSIONS[section_ids[1]] - 1)
        assert(pending[1].to_version == migrate.CURRENT_SECTION_VERSIONS[section_ids[1]])
        assert(pending[1].handled == false, "a freshly planned migration must start unhandled")
    end,

    -- SAV-20
    plan_migrations_rejects_downgrade = function()
        local saved = {}
        for section_id, version in pairs(migrate.CURRENT_SECTION_VERSIONS) do
            saved[section_id] = version
        end
        local any_section = next(saved)
        saved[any_section] = saved[any_section] + 1

        local pending, err = migrate.plan_migrations(saved)
        assert(pending == nil, "a section newer than current must be rejected, not planned")
        assert(err == "MigrationDowngradeUnsupported", "got: " .. tostring(err))
    end,

    -- SAV-20
    plan_migrations_rejects_malformed_saved_version = function()
        local saved = { meta = "not_a_number" }
        local pending, err = migrate.plan_migrations(saved)
        assert(pending == nil, "a non-integer saved version must be rejected")
        assert(err == "MigrationDowngradeUnsupported", "got: " .. tostring(err))
    end,

    -- SAV-20
    verify_complete_passes_when_no_pending_migrations_stashed = function()
        game.runtime.pending_section_migrations = nil
        local err = migrate.verify_complete()
        assert(err == nil, "no pending migrations must verify as complete, got: " .. tostring(err))
    end,

    -- SAV-20
    verify_complete_passes_when_every_pending_entry_is_handled = function()
        game.runtime.pending_section_migrations = {
            { section_id = "meta", from_version = 1, to_version = 2, handled = true },
        }
        local err = migrate.verify_complete()
        game.runtime.pending_section_migrations = nil
        assert(err == nil, "every entry handled must verify as complete, got: " .. tostring(err))
    end,

    -- SAV-20
    verify_complete_rejects_an_unhandled_pending_entry = function()
        game.runtime.pending_section_migrations = {
            { section_id = "meta", from_version = 1, to_version = 2, handled = false },
        }
        local err = migrate.verify_complete()
        game.runtime.pending_section_migrations = nil
        assert(err ~= nil, "an unhandled pending entry must not silently verify as complete")
        assert(tostring(err):find("^MigrationMissing:meta:1%->2$") ~= nil, "got: " .. tostring(err))
    end,
}
