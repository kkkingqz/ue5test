#include "GV2RuntimeCore/Testing/GV2ColdStartLoadConformance.h"

#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentCore/RepositoryBuilder.h"
#include "GV2ContentCore/RepositorySnapshot.h"
#include "GV2RuntimeCore/GV2HostServices.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace GV2RuntimeCore::Testing
{
namespace
{
// Mirrors GV2ValidatorRegistryConformance.cpp's MakeEmptyRepository — no
// content is needed here (SAV-15/16's redirect/referential-integrity
// logic against real content is covered by
// Tests/Lua/save/load_path.lua instead).
GV2ContentCore::FRepositoryReadHandle MakeEmptyRepository()
{
    const GV2ContentCore::FPackageDescriptor EmptyCore("core", "core", 0u, {}, {});
    GV2ContentCore::FBuildOptions Options;
    GV2ContentCore::FBuildResult Result = GV2ContentCore::BuildRepository({EmptyCore}, Options);
    if (Result.IsFailure())
    {
        return {};
    }
    return Result.GetCandidate().GetReadHandle();
}

// Every module below is a deliberately trivial, synthetic stand-in, not a
// copy of the real Scripts/ module of the same id: this suite exists to
// exercise FRuntimeSession::StartFromSave's C++ orchestration (tree
// obtained from decode_and_prepare instead of defaults, "restore_instances"
// firing at the right point, state assigned only after every stage
// succeeds, slot-read-before-VM-creation on failure) — never to re-check a
// Lua rule. The real preflight/integrity/redirect/safe-point rules these
// module ids carry in production are already fully covered by
// Tests/Lua/save/{canonical_codec,save_path,load_path}.lua (ADR-0024: a
// rule expressed in Lua belongs to a Lua spec, not a C++ conformance
// fixture). Module ids match production only so requiring them here reads
// naturally.
const char* SaveStubSource = R"lua(
local M = {
    id = "core:module.runtime.save",
}

M.SAVE_VERSION = 1

function M.is_safe_point()
    return game and game.runtime and game.runtime.phase == "idle"
end

-- Not a real container: just enough for the paired load stub above/below
-- to recover the marker value and the "data" section version the
-- mechanism test seeded and checks for. data_version defaults to this
-- stub's own CURRENT_SECTION_VERSIONS-equivalent (2) when omitted; tests
-- exercising a migration pass an older value explicitly.
function M.save(slot_id, data_version)
    if not M.is_safe_point() then
        return false, "SaveNotAtSafePoint"
    end
    if not game or not game.save_slots or not game.save_slots.write then
        return false, "SaveSlotStorageUnavailable"
    end
    local container = "SYNTHETIC_CONTAINER:" .. tostring(game.state.data.marker) .. ":" .. tostring(data_version or 2)
    local ok, err = game.save_slots.write(slot_id, container)
    if not ok then
        return false, "SaveWriteFailed:" .. tostring(err)
    end
    return true
end

return M
)lua";

const char* MigrateStubSource = R"lua(
local M = {
    id = "core:module.runtime.migrate",
}

-- "data" bumped to 2 so this suite can exercise a real migration (SAV-18):
-- a container written at version 1 needs a migrate_state hook to reach it.
M.CURRENT_SECTION_VERSIONS = {
    meta = 1,
    data = 2,
}

function M.plan_migrations(saved_section_versions)
    saved_section_versions = saved_section_versions or {}
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
        if type(saved) ~= "number" or saved > current then
            return nil, "MigrationDowngradeUnsupported"
        end
        if saved < current then
            pending[#pending + 1] = { section_id = section_id, from_version = saved, to_version = current, handled = false }
        end
    end
    return pending, nil
end

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
)lua";

const char* LoadStubSource = R"lua(
local migrate = require("core:module.runtime.migrate")

local M = {
    id = "core:module.runtime.load",
}

function M.decode_and_prepare(container_bytes)
    if type(container_bytes) ~= "string" then
        return nil, "SaveContainerCorrupt"
    end
    local marker, saved_version_str = container_bytes:match("^SYNTHETIC_CONTAINER:([^:]*):(%-?%d+)$")
    if not marker then
        return nil, "SaveContainerCorrupt"
    end

    local pending, plan_err = migrate.plan_migrations({ data = tonumber(saved_version_str) })
    if not pending then
        return nil, plan_err
    end
    if game and game.runtime then
        game.runtime.pending_section_migrations = pending
    end

    return { meta = { save_version = 1 }, data = { marker = marker } }, nil
end

return M
)lua";
// Deliberately minimal stand-in for the real state_validator: this suite
// is about StartFromSave's C++ mechanics, not gameplay validation rules
// (those are covered elsewhere). "data" carries a marker field the
// save-writer session seeds and the load session's driver checks survived
// the round trip.
const char* StateValidatorStubSource = R"lua(
local M = {
    id = "core:module.runtime.state_validator",
}

local SECTIONS = { "meta", "data" }
M.canonical_sections = SECTIONS
M.definition_reference_fields = {}

function M.is_canonical_section(name)
    for _, s in ipairs(SECTIONS) do
        if s == name then return true end
    end
    return false
end

function M.create_empty_canonical_state()
    return { meta = { save_version = 1 }, data = {} }
end

function M.validate_state_tree(tree)
    if type(tree) ~= "table" then
        error("LuaStateValidationInvalid: root state must be a table")
    end
    for k, _ in pairs(tree) do
        if not M.is_canonical_section(k) then
            error("LuaStateValidationInvalid: invalid top-level section '" .. tostring(k) .. "'")
        end
    end
    return true
end

return M
)lua";

// SAV-17/18/19: proves "migrate_state" fires before "restore_instances",
// and only on a cold-start load — game.debug is outside canonical state,
// so setting a flag there cannot affect the state_hash roundtrip
// comparison the test also makes. migrate_state only claims the "data"
// section when from_version == 1 (the version this suite's "needs
// migration" case writes); a from_version of 0 is deliberately left
// unclaimed so RunColdStartLoadConformance can prove SAV-20's "missing
// migration is rejected, not silently skipped".
const char* RestoreMarkerSource = R"lua(
local M = {
    id = "core:module.test.restore_marker",
}

function M.migrate_state(ctx, tree)
    local pending = game.runtime.pending_section_migrations
    if not pending then
        return
    end
    for _, entry in ipairs(pending) do
        if entry.section_id == "data" and entry.from_version == 1 then
            tree.data.marker = tostring(tree.data.marker) .. ":migrated"
            entry.handled = true
        end
    end
end

function M.restore_instances(ctx, tree)
    assert(type(tree) == "table", "restore_instances must receive the loaded tree")
    assert(tree.data ~= nil, "restore_instances must see the loaded data section")
    game.debug.restore_instances_called = true
end

return M
)lua";

// Save-writer session driver: seeds a marker field via create_default_state,
// then writes a real container through the real game.save_slots.write
// binding during "start" (game.runtime.phase is "idle" by then — this
// minimal manifest has no event_bus/command_dispatcher to set it, so the
// driver sets it itself in "register", exactly like event_bus normally does).
const char* SaveDriverSource = R"lua(
local save = require("core:module.runtime.save")

local M = {
    id = "core:module.test.save_driver",
}

function M.register(ctx)
    game.runtime.phase = "idle"
    -- Synthetic stand-in for boundary/outbound.lua's real hash wiring —
    -- proves the C++ before/after comparison mechanism, not the real
    -- canonical hashing algorithm (that is Tests/Lua/save/canonical_codec.lua's job).
    game.runtime.get_canonical_state_hash = function()
        return "hash:" .. tostring(game.state.data.marker)
    end
end

function M.create_default_state(ctx)
    return { data = { marker = "before_save" } }
end

function M.start(ctx)
    -- Current section version (2) — no migration needed, so this slot is
    -- reserved for the SAV-17 roundtrip proof below (state_hash must be
    -- byte-identical before/after, which a real migration would disturb).
    local ok, err = save.save("cold_start_conformance_slot")
    assert(ok, "save.save must succeed: " .. tostring(err))

    -- Same state, saved a second time at the OLDER section version (1) —
    -- exercises a real migration on load (SAV-18/19), on its own slot so
    -- it never interferes with the roundtrip proof above.
    local ok2, err2 = save.save("cold_start_migration_slot", 1)
    assert(ok2, "save.save (versioned) must succeed: " .. tostring(err2))

    -- Same again but at from_version = 0, which RestoreMarkerSource's
    -- migrate_state deliberately does not claim — SAV-20's "missing
    -- migration is rejected explicitly" negative case.
    local ok3, err3 = save.save("cold_start_missing_migration_slot", 0)
    assert(ok3, "save.save (unclaimed version) must succeed: " .. tostring(err3))
end

return M
)lua";

// Load session driver: only wires reachability and lets state_validator +
// restore_marker + load do the real work; RunColdStartLoadConformance
// checks the resulting state after StartFromSave returns.
const char* LoadDriverSource = R"lua(
local M = {
    id = "core:module.test.load_driver",
}

function M.register(ctx)
    game.runtime.get_canonical_state_hash = function()
        return "hash:" .. tostring(game.state.data.marker)
    end
end

return M
)lua";

std::vector<FRuntimeSource> MakeSharedSources(const char* DriverModuleId, const char* DriverSource)
{
    std::string Manifest;
    Manifest += "return {\n            entry_module_id = \"";
    Manifest += DriverModuleId;
    Manifest += R"lua(",
            modules = {
                {
                    module_id = "core:module.runtime.migrate",
                    source = "runtime/migrate.lua",
                    dependencies = {},
                },
                {
                    module_id = "core:module.runtime.state_validator",
                    source = "runtime/state_validator.lua",
                    dependencies = {},
                },
                {
                    module_id = "core:module.runtime.save",
                    source = "runtime/save.lua",
                    dependencies = {},
                },
                {
                    module_id = "core:module.runtime.load",
                    source = "runtime/load.lua",
                    dependencies = { "core:module.runtime.migrate" },
                },
                {
                    module_id = "core:module.test.restore_marker",
                    source = "test/restore_marker.lua",
                    dependencies = {},
                },
                {
                    module_id = ")lua";
    Manifest += DriverModuleId;
    Manifest += R"lua(",
                    source = "test/driver.lua",
                    dependencies = {
                        "core:module.runtime.migrate",
                        "core:module.runtime.state_validator",
                        "core:module.runtime.save",
                        "core:module.runtime.load",
                        "core:module.test.restore_marker",
                    },
                },
            },
        })lua";

    return {
        {"@Scripts/bootstrap/manifest.lua", Manifest},
        {"@Scripts/runtime/migrate.lua", MigrateStubSource},
        {"@Scripts/runtime/state_validator.lua", StateValidatorStubSource},
        {"@Scripts/runtime/save.lua", SaveStubSource},
        {"@Scripts/runtime/load.lua", LoadStubSource},
        {"@Scripts/test/restore_marker.lua", RestoreMarkerSource},
        {"@Scripts/test/driver.lua", DriverSource},
    };
}
} // namespace

std::string RunColdStartLoadConformance()
{
    const GV2ContentCore::FRepositoryReadHandle RepoHandle = MakeEmptyRepository();
    if (!RepoHandle.IsValid())
    {
        return "cold_start_load_conformance.empty_repository_build_failed";
    }

    const std::filesystem::path SlotRoot = std::filesystem::temp_directory_path()
        / ("gv2_cold_start_load_conformance_"
            + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    FFilesystemSaveSlotStorage Storage(SlotRoot);

    // 1. Write session (NewGame): seeds data.marker, saves through the
    //    real host primitive.
    std::string StateHashBeforeSave;
    {
        FRuntimeSession WriteSession;
        FRuntimeFault Fault;
        WriteSession.SetSaveSlotStorage(&Storage);
        const std::vector<FRuntimeSource> Sources =
            MakeSharedSources("core:module.test.save_driver", SaveDriverSource);
        if (!WriteSession.Start(1, RepoHandle, Sources, Fault))
        {
            std::error_code Ec;
            std::filesystem::remove_all(SlotRoot, Ec);
            return "cold_start_load_conformance.write_session_start_failed: " + Fault.Code + ": " + Fault.Message;
        }
        StateHashBeforeSave = WriteSession.GetCanonicalStateHash();
        WriteSession.Stop();
    }

    if (StateHashBeforeSave.empty())
    {
        std::error_code Ec;
        std::filesystem::remove_all(SlotRoot, Ec);
        return "cold_start_load_conformance.write_session_state_hash_empty";
    }

    // 2. Load session (SAV-12): StartFromSave against the slot the write
    //    session just published.
    {
        FRuntimeSession LoadSession;
        FRuntimeFault Fault;
        const std::vector<FRuntimeSource> Sources =
            MakeSharedSources("core:module.test.load_driver", LoadDriverSource);
        if (!LoadSession.StartFromSave(1, RepoHandle, Sources, Storage, "cold_start_conformance_slot", Fault))
        {
            std::error_code Ec;
            std::filesystem::remove_all(SlotRoot, Ec);
            return "cold_start_load_conformance.load_session_start_failed: " + Fault.Code + ": " + Fault.Message;
        }

        // SAV-17: state_hash must match — decode/rewrite/restore_instances
        // above must not have altered canonical state.
        const std::string StateHashAfterLoad = LoadSession.GetCanonicalStateHash();
        if (StateHashAfterLoad != StateHashBeforeSave)
        {
            LoadSession.Stop();
            std::error_code Ec;
            std::filesystem::remove_all(SlotRoot, Ec);
            return "cold_start_load_conformance.state_hash_mismatch_after_load";
        }

        std::vector<FLuaSpecCaseResult> CaseResults;
        const bool bSpecOk = LoadSession.RunLuaSpec(
            "@cold_start_load_conformance_assertions",
            R"lua(return {
                restore_instances_hook_fired = function()
                    assert(game.debug.restore_instances_called == true,
                        "restore_instances hook must have run during cold-start load")
                end,
                loaded_state_carries_saved_marker = function()
                    assert(game.state.data.marker == "before_save",
                        "loaded canonical state must carry the value the write session saved, got: "
                            .. tostring(game.state.data.marker))
                end,
            })lua",
            CaseResults,
            Fault);
        LoadSession.Stop();

        if (!bSpecOk)
        {
            std::error_code Ec;
            std::filesystem::remove_all(SlotRoot, Ec);
            return "cold_start_load_conformance.assertions_spec_failed: " + Fault.Code + ": " + Fault.Message;
        }
        for (const FLuaSpecCaseResult& Case : CaseResults)
        {
            if (!Case.Success)
            {
                std::error_code Ec;
                std::filesystem::remove_all(SlotRoot, Ec);
                return "cold_start_load_conformance.assertion_failed: " + Case.CaseId + ": " + Case.ErrorMessage;
            }
        }
    }

    // 2b. SAV-18/19: a slot saved at an older section version loads
    //     through a real migrate_state hook, in the right order (before
    //     restore_instances/validate_state — proven by the marker
    //     mutation surviving into final assigned state).
    {
        FRuntimeSession MigrationSession;
        FRuntimeFault Fault;
        const std::vector<FRuntimeSource> Sources =
            MakeSharedSources("core:module.test.load_driver", LoadDriverSource);
        if (!MigrationSession.StartFromSave(1, RepoHandle, Sources, Storage, "cold_start_migration_slot", Fault))
        {
            std::error_code Ec;
            std::filesystem::remove_all(SlotRoot, Ec);
            return "cold_start_load_conformance.migration_session_start_failed: " + Fault.Code + ": " + Fault.Message;
        }

        std::vector<FLuaSpecCaseResult> CaseResults;
        const bool bSpecOk = MigrationSession.RunLuaSpec(
            "@cold_start_load_conformance_migration_assertions",
            R"lua(return {
                migrate_state_hook_transformed_state = function()
                    assert(game.state.data.marker == "before_save:migrated",
                        "migrate_state must have run and transformed state before assignment, got: "
                            .. tostring(game.state.data.marker))
                end,
            })lua",
            CaseResults,
            Fault);
        MigrationSession.Stop();

        if (!bSpecOk)
        {
            std::error_code Ec;
            std::filesystem::remove_all(SlotRoot, Ec);
            return "cold_start_load_conformance.migration_assertions_spec_failed: " + Fault.Code + ": " + Fault.Message;
        }
        for (const FLuaSpecCaseResult& Case : CaseResults)
        {
            if (!Case.Success)
            {
                std::error_code Ec;
                std::filesystem::remove_all(SlotRoot, Ec);
                return "cold_start_load_conformance.migration_assertion_failed: " + Case.CaseId + ": " + Case.ErrorMessage;
            }
        }
    }

    // 2c. SAV-20: a section version no module claims is rejected
    //     explicitly (verify_complete), not silently left stale.
    {
        FRuntimeSession MissingMigrationSession;
        FRuntimeFault Fault;
        const std::vector<FRuntimeSource> Sources =
            MakeSharedSources("core:module.test.load_driver", LoadDriverSource);
        const bool bStarted = MissingMigrationSession.StartFromSave(
            1, RepoHandle, Sources, Storage, "cold_start_missing_migration_slot", Fault);
        MissingMigrationSession.Stop();
        if (bStarted)
        {
            std::error_code Ec;
            std::filesystem::remove_all(SlotRoot, Ec);
            return "cold_start_load_conformance.missing_migration_did_not_fail";
        }
        if (Fault.Code.rfind("MigrationMissing:", 0) != 0)
        {
            std::error_code Ec;
            std::filesystem::remove_all(SlotRoot, Ec);
            return "cold_start_load_conformance.missing_migration_wrong_fault_code: " + Fault.Code;
        }
    }

    // 2d. SAV-20: a section saved at a version newer than this build
    //     understands is a downgrade — rejected before any migrate_state
    //     hook runs, let alone assigning state.
    {
        const FSaveSlotWriteResult WriteResult =
            Storage.WriteSlot("cold_start_downgrade_slot", "SYNTHETIC_CONTAINER:before_save:99");
        if (WriteResult.Result != ESaveSlotResult::Ok)
        {
            std::error_code Ec;
            std::filesystem::remove_all(SlotRoot, Ec);
            return "cold_start_load_conformance.downgrade_slot_setup_failed";
        }

        FRuntimeSession DowngradeSession;
        FRuntimeFault Fault;
        const std::vector<FRuntimeSource> Sources =
            MakeSharedSources("core:module.test.load_driver", LoadDriverSource);
        const bool bStarted =
            DowngradeSession.StartFromSave(1, RepoHandle, Sources, Storage, "cold_start_downgrade_slot", Fault);
        DowngradeSession.Stop();
        if (bStarted)
        {
            std::error_code Ec;
            std::filesystem::remove_all(SlotRoot, Ec);
            return "cold_start_load_conformance.downgrade_did_not_fail";
        }
        if (Fault.Code != "MigrationDowngradeUnsupported")
        {
            std::error_code Ec;
            std::filesystem::remove_all(SlotRoot, Ec);
            return "cold_start_load_conformance.downgrade_wrong_fault_code: " + Fault.Code;
        }
    }

    // 3. Negative: a corrupt container is rejected without ever assigning
    //    state, and never crashes the host.
    {
        const FSaveSlotWriteResult WriteResult = Storage.WriteSlot("corrupt_conformance_slot", "not a valid container");
        if (WriteResult.Result != ESaveSlotResult::Ok)
        {
            std::error_code Ec;
            std::filesystem::remove_all(SlotRoot, Ec);
            return "cold_start_load_conformance.corrupt_slot_setup_failed";
        }

        FRuntimeSession CorruptSession;
        FRuntimeFault Fault;
        const std::vector<FRuntimeSource> Sources =
            MakeSharedSources("core:module.test.load_driver", LoadDriverSource);
        const bool bStarted =
            CorruptSession.StartFromSave(1, RepoHandle, Sources, Storage, "corrupt_conformance_slot", Fault);
        CorruptSession.Stop();
        if (bStarted)
        {
            std::error_code Ec;
            std::filesystem::remove_all(SlotRoot, Ec);
            return "cold_start_load_conformance.corrupt_container_did_not_fail";
        }
        if (Fault.Code != "SaveContainerCorrupt")
        {
            std::error_code Ec;
            std::filesystem::remove_all(SlotRoot, Ec);
            return "cold_start_load_conformance.corrupt_container_wrong_fault_code: " + Fault.Code;
        }
    }

    // 4. Negative: a slot that was never written fails before any Lua VM
    //    is created (SAV-12).
    {
        FRuntimeSession MissingSlotSession;
        FRuntimeFault Fault;
        const std::vector<FRuntimeSource> Sources =
            MakeSharedSources("core:module.test.load_driver", LoadDriverSource);
        const bool bStarted = MissingSlotSession.StartFromSave(
            1, RepoHandle, Sources, Storage, "slot_that_was_never_written", Fault);
        MissingSlotSession.Stop();
        std::error_code Ec;
        std::filesystem::remove_all(SlotRoot, Ec);
        if (bStarted)
        {
            return "cold_start_load_conformance.missing_slot_did_not_fail";
        }
        if (Fault.Code != "SaveSlotNotFound")
        {
            return "cold_start_load_conformance.missing_slot_wrong_fault_code: " + Fault.Code;
        }
    }

    return "";
}
}
