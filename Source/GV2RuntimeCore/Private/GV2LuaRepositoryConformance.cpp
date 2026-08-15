#include "GV2RuntimeCore/Testing/GV2LuaRepositoryConformance.h"

#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentCore/RepositoryBuilder.h"
#include "GV2ContentCore/RepositorySnapshot.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"

#include <map>
#include <string>
#include <vector>
#include <optional>

namespace GV2RuntimeCore::Testing
{
namespace
{
class FConformanceInMemoryContentProvider final : public GV2ContentCore::IContentSourceProvider
{
public:
    std::map<std::string, std::map<std::string, std::string>> FilesByPackage;

    std::optional<std::string> ReadSource(
        std::string_view RequestedPackageId,
        std::string_view RelativeSource) const override
    {
        auto PkgIt = FilesByPackage.find(std::string(RequestedPackageId));
        if (PkgIt == FilesByPackage.end())
        {
            return std::nullopt;
        }
        auto FileIt = PkgIt->second.find(std::string(RelativeSource));
        if (FileIt == PkgIt->second.end())
        {
            return std::nullopt;
        }
        return FileIt->second;
    }
};

GV2ContentCore::FBuildResult BuildConformanceRepository(
    FConformanceInMemoryContentProvider& Provider,
    int ExtraPrice = 10)
{
    using namespace GV2ContentCore;

    Provider.FilesByPackage.clear();

    // 1. Core package files
    Provider.FilesByPackage["core"]["schemas/item_v1.schema.json5"] = R"json5({
        id: "core:schema.definition.item.v1",
        definition_type: "item",
        schema_version: 1,
        root: {
            kind: "object",
            fields: {
                label_text_id: { kind: "text_id", required: true },
                price: { kind: "int64", required: true, min: 0 },
                nested_data: {
                    kind: "object",
                    required: false,
                    fields: {
                        level_1: {
                            kind: "object",
                            required: false,
                            fields: {
                                level_2: {
                                    kind: "object",
                                    required: false,
                                    fields: {
                                        level_3: {
                                            kind: "object",
                                            required: false,
                                            fields: {
                                                value: { kind: "string", required: false },
                                            },
                                        },
                                    },
                                },
                            },
                        },
                    },
                },
            },
        },
        semantic_validators: [],
        extensions: {},
    })json5";

    Provider.FilesByPackage["core"]["schemas/screen_v1.schema.json5"] = R"json5({
        id: "core:schema.definition.screen.v1",
        definition_type: "screen",
        schema_version: 1,
        root: {
            kind: "object",
            fields: {
                title_text_id: { kind: "text_id", required: false },
            },
        },
        semantic_validators: [],
        extensions: {},
    })json5";

    Provider.FilesByPackage["core"]["schemas/text_v1.schema.json5"] = R"json5({
        id: "core:schema.definition.text.v1",
        definition_type: "text",
        schema_version: 1,
        root: {
            kind: "object",
            fields: {
                source_message: { kind: "string", required: true },
            },
        },
        semantic_validators: [],
        extensions: {},
    })json5";

    // Item definitions (includes normal item, nested object within limits)
    Provider.FilesByPackage["core"]["definitions/items.json5"] = std::string(R"json5({
        schema_version: 1,
        type: "item",
        definitions: [
            {
                id: "core:item.weapon.iron_sword",
                data: {
                    label_text_id: "core:text.item.iron_sword.name",
                    price: )json5") + std::to_string(ExtraPrice) + R"json5(,
                    nested_data: {
                        level_1: {
                            level_2: {
                                level_3: {
                                    value: "deep_nested_ok",
                                },
                            },
                        },
                    },
                },
                tags: ["weapon", "melee"],
                deprecated: false,
            },
        ],
    })json5";

    // Screen definitions
    Provider.FilesByPackage["core"]["definitions/screens.json5"] = R"json5({
        schema_version: 1,
        type: "screen",
        definitions: [
            {
                id: "core:screen.inventory",
                data: {
                    title_text_id: "core:text.screen.inventory.title",
                },
            },
            {
                id: "core:screen.main",
                data: {
                    title_text_id: "core:text.screen.main.title",
                },
            },
        ],
    })json5";

    // Text definitions
    Provider.FilesByPackage["core"]["definitions/texts.json5"] = R"json5({
        schema_version: 1,
        type: "text",
        definitions: [
            {
                id: "core:text.item.iron_sword.name",
                data: { source_message: "Iron sword" },
            },
            {
                id: "core:text.screen.inventory.title",
                data: { source_message: "Inventory" },
            },
            {
                id: "core:text.screen.main.title",
                data: { source_message: "Main screen" },
            },
        ],
    })json5";

    // 2. Test mod package files
    Provider.FilesByPackage["test_mod"]["definitions/screens.json5"] = R"json5({
        schema_version: 1,
        type: "screen",
        definitions: [
            {
                id: "core:screen.inventory",
                data: {
                    title_text_id: "core:text.screen.inventory.title",
                },
                tags: ["test_mod_override"],
            },
            {
                id: "test_mod:screen.codex_lab",
                data: {
                    title_text_id: "test_mod:text.screen.codex_lab.title",
                },
            },
        ],
    })json5";

    Provider.FilesByPackage["test_mod"]["definitions/texts.json5"] = R"json5({
        schema_version: 1,
        type: "text",
        definitions: [
            {
                id: "test_mod:text.screen.codex_lab.title",
                data: { source_message: "Codex Lab" },
            },
        ],
    })json5";

    const FPackageDescriptor CoreDescriptor(
        "core",
        "core",
        0,
        {
            "definitions/items.json5",
            "definitions/screens.json5",
            "definitions/texts.json5",
        },
        {
            FSchemaBinding("item", 1, "core:schema.definition.item.v1", "schemas/item_v1.schema.json5"),
            FSchemaBinding("screen", 1, "core:schema.definition.screen.v1", "schemas/screen_v1.schema.json5"),
            FSchemaBinding("text", 1, "core:schema.definition.text.v1", "schemas/text_v1.schema.json5"),
        });

    const FPackageDescriptor TestModDescriptor(
        "test_mod",
        "test_mod",
        1,
        {
            "definitions/screens.json5",
            "definitions/texts.json5",
        },
        {},
        {},
        {
            FRedirectDescriptor("test_mod:screen.codex_archive", "test_mod:screen.codex_legacy"),
            FRedirectDescriptor("test_mod:screen.codex_legacy", "test_mod:screen.codex_lab"),
        },
        {
            "test_mod:screen.retired",
        });

    FBuildOptions Options;
    Options.SourceProvider = &Provider;
    return BuildRepository({ CoreDescriptor, TestModDescriptor }, Options);
}

const char* ConformanceLuaScript = R"lua(
local M = {}

-- 1. Table structure and protection
assert(type(game.repository) == "table", "game.repository must be a table")
assert(type(game.repository.get) == "function", "game.repository.get must be a function")
assert(type(game.repository.require) == "function", "game.repository.require must be a function")
assert(type(game.repository.list) == "function", "game.repository.list must be a function")
assert(type(game.repository.exists) == "function", "game.repository.exists must be a function")

local count = 0
for k, v in pairs(game.repository) do
    count = count + 1
end
assert(count == 4, "game.repository must expose exactly 4 functions")

local ro_ok, ro_err = pcall(function() game.repository.mutation = 123 end)
assert(not ro_ok and string.find(tostring(ro_err), "read%-only table"), "game.repository must be read-only")

-- 2. Absence of legacy aliases
assert(game.data == nil, "game.data alias must be absent")

-- 3. Exists queries
assert(game.repository.exists("core:item.weapon.iron_sword") == true, "iron_sword must exist")
assert(game.repository.exists("core:item.weapon.non_existent") == false, "non_existent must not exist")
assert(game.repository.exists("invalid_id_grammar") == false, "invalid ID grammar must return false")
assert(game.repository.exists(12345) == false, "non-string ID must return false")

-- 4. Get query (happy path)
local sword, sword_err = game.repository.get("core:item.weapon.iron_sword")
assert(sword ~= nil, "sword definition must be found")
assert(sword_err == nil, "sword_err must be nil on success")
assert(sword.id == "core:item.weapon.iron_sword", "sword.id must match")
assert(type(sword.data) == "table", "sword.data must be table")
assert(sword.data.price == 10, "sword.data.price must match")
assert(sword.data.nested_data.level_1.level_2.level_3.value == "deep_nested_ok", "nested data must marshal properly")

-- 5. Detached deep copy: mutating returned table does not mutate snapshot
sword.data.price = 9999
sword.data.nested_data.level_1.level_2.level_3.value = "mutated"
local sword_fresh, _ = game.repository.get("core:item.weapon.iron_sword")
assert(sword_fresh.data.price == 10, "repository data must remain unchanged after local Lua mutation")
assert(sword_fresh.data.nested_data.level_1.level_2.level_3.value == "deep_nested_ok", "nested data must remain unchanged")

-- 6. Require query (happy path)
local req_sword = game.repository.require("core:item.weapon.iron_sword")
assert(req_sword.id == "core:item.weapon.iron_sword", "require sword id must match")
assert(req_sword.data.price == 10, "require sword price must match")

-- 7. Get negative queries (typed error tables)
local miss, miss_err = game.repository.get("core:item.weapon.missing_item")
assert(miss == nil, "missing item must return nil")
assert(type(miss_err) == "table" and miss_err.code == "not_found", "miss_err.code must be not_found")
assert(miss_err.requested_id == "core:item.weapon.missing_item", "miss_err.requested_id must match")

local bad_id, bad_err = game.repository.get("Not_A_Valid_Id")
assert(bad_id == nil, "bad ID must return nil")
assert(type(bad_err) == "table" and bad_err.code == "invalid_id", "bad_err.code must be invalid_id")

local tomb, tomb_err = game.repository.get("test_mod:screen.retired")
assert(tomb == nil, "tombstoned item must return nil")
assert(type(tomb_err) == "table" and tomb_err.code == "tombstoned", "tomb_err.code must be tombstoned")
assert(tomb_err.requested_id == "test_mod:screen.retired", "tomb_err.requested_id must match")

-- 8. Require negative queries (structured errors with code prefix)
local req_miss_ok, req_miss_err = pcall(function() game.repository.require("core:item.weapon.missing_item") end)
assert(not req_miss_ok, "require missing item must fail")
assert(string.find(tostring(req_miss_err), "not_found:") ~= nil, "require missing must have not_found code prefix")

local req_bad_ok, req_bad_err = pcall(function() game.repository.require("Not_Valid") end)
assert(not req_bad_ok, "require invalid ID must fail")
assert(string.find(tostring(req_bad_err), "invalid_id:") ~= nil, "require invalid ID must have invalid_id code prefix")

local req_tomb_ok, req_tomb_err = pcall(function() game.repository.require("test_mod:screen.retired") end)
assert(not req_tomb_ok, "require tombstoned must fail")
assert(string.find(tostring(req_tomb_err), "tombstoned:") ~= nil, "require tombstoned must have tombstoned code prefix")

-- 9. Redirect resolution
local redir_get, redir_get_err = game.repository.get("test_mod:screen.codex_archive")
assert(redir_get ~= nil and redir_get_err == nil, "get redirect source must resolve")
assert(redir_get.id == "test_mod:screen.codex_lab", "get redirect target ID must be resolved active ID")

local redir_req = game.repository.require("test_mod:screen.codex_archive")
assert(redir_req.id == "test_mod:screen.codex_lab", "require redirect target ID must be resolved active ID")

-- 10. List queries (canonical byte order, exact ordering and error cases)
local screens = game.repository.list("screen")
assert(type(screens) == "table" and #screens == 3, "screens list must contain exactly 3 active screens")
assert(screens[1] == "core:screen.inventory", "screens[1] must be core:screen.inventory")
assert(screens[2] == "core:screen.main", "screens[2] must be core:screen.main")
assert(screens[3] == "test_mod:screen.codex_lab", "screens[3] must be test_mod:screen.codex_lab")

local texts = game.repository.list("text")
assert(type(texts) == "table" and #texts == 4, "texts list must contain exactly 4 active texts")
assert(texts[1] == "core:text.item.iron_sword.name", "texts[1] must match")
assert(texts[2] == "core:text.screen.inventory.title", "texts[2] must match")
assert(texts[3] == "core:text.screen.main.title", "texts[3] must match")
assert(texts[4] == "test_mod:text.screen.codex_lab.title", "texts[4] must match")

local empty_kind = game.repository.list("unknown_kind")
assert(type(empty_kind) == "table" and #empty_kind == 0, "unknown kind must return empty table")

local bad_arg_list = game.repository.list(999)
assert(type(bad_arg_list) == "table" and #bad_arg_list == 0, "non-string kind must return empty table")

-- 11. Absence of provenance and authoring metadata
local inv = game.repository.require("core:screen.inventory")
assert(inv.provenance == nil, "inv.provenance must be nil")
assert(inv.package_id == nil, "inv.package_id must be nil")
assert(inv.package == nil, "inv.package must be nil")
assert(inv.source == nil, "inv.source must be nil")
assert(inv.file == nil, "inv.file must be nil")
assert(inv.line == nil, "inv.line must be nil")
assert(inv.path == nil, "inv.path must be nil")
assert(inv.load_index == nil, "inv.load_index must be nil")
assert(inv.shadowed_providers == nil, "inv.shadowed_providers must be nil")

return M
)lua";

} // namespace

std::string RunLuaRepositoryAccessConformance()
{
    FConformanceInMemoryContentProvider Provider;
    GV2ContentCore::FBuildResult Build1 = BuildConformanceRepository(Provider, 10);
    if (Build1.IsFailure())
    {
        std::string Diags;
        for (const auto& Diagnostic : Build1.GetDiagnostics())
        {
            Diags += Diagnostic.Code + " (" + Diagnostic.Message + "); ";
        }
        return "repository_conformance_build1_failed: " + Diags;
    }

    const GV2ContentCore::FRepositoryReadHandle PinnedHandle1 = Build1.GetCandidate().GetReadHandle();
    if (!PinnedHandle1.IsValid())
    {
        return "repository_conformance_handle1_invalid";
    }

    const std::vector<FRuntimeSource> Sources = {
        {
            "@Scripts/bootstrap/manifest.lua",
            R"lua(return {
                entry_module_id = "core:module.test.conformance",
                modules = {
                    {
                        module_id = "core:module.test.conformance",
                        source = "test/conformance.lua",
                        dependencies = {},
                    },
                },
            })lua"
        },
        {
            "@Scripts/test/conformance.lua",
            ConformanceLuaScript
        }
    };

    FRuntimeSession Session;
    FRuntimeFault Fault;

    // 1. Session start with valid pinned handle
    if (!Session.Start(1, PinnedHandle1, Sources, Fault))
    {
        return "session_start_failed: " + Fault.Code + ": " + Fault.Message;
    }
    if (!Session.IsStarted())
    {
        return "session_is_started_false";
    }
    if (!Session.GetPinnedRepository().IsValid())
    {
        return "session_pinned_repository_invalid";
    }
    const std::string PinnedHash1 = Session.GetPinnedRepository().GetContentHash();
    if (PinnedHash1 != PinnedHandle1.GetContentHash())
    {
        return "session_pinned_hash_mismatch";
    }

    // 2. Unrelated rebuild / republish simulation:
    // Build a new repository with different content (ExtraPrice = 999).
    GV2ContentCore::FBuildResult Build2 = BuildConformanceRepository(Provider, 999);
    if (Build2.IsFailure())
    {
        Session.Stop();
        return "repository_conformance_build2_failed";
    }
    const GV2ContentCore::FRepositoryReadHandle PinnedHandle2 = Build2.GetCandidate().GetReadHandle();
    if (!PinnedHandle2.IsValid() || PinnedHandle2.GetContentHash() == PinnedHash1)
    {
        Session.Stop();
        return "repository_conformance_build2_hash_unexpected";
    }

    // 3. Active session must retain its pinned handle and original snapshot
    if (Session.GetPinnedRepository().GetContentHash() != PinnedHash1)
    {
        Session.Stop();
        return "session_lost_pinned_snapshot_on_republish";
    }

    // Verify session's pinned handle still queries price == 10, while Handle2 queries price == 999
    const GV2ContentCore::FValue* Def1 = Session.GetPinnedRepository().Find(
        GV2ContentCore::FDefinitionId::Require("core:item.weapon.iron_sword"));
    if (Def1 == nullptr || Def1->FindField("data")->FindField("price")->AsInteger() != 10)
    {
        Session.Stop();
        return "session_pinned_query_price_corrupted";
    }

    const GV2ContentCore::FValue* Def2 = PinnedHandle2.Find(
        GV2ContentCore::FDefinitionId::Require("core:item.weapon.iron_sword"));
    if (Def2 == nullptr || Def2->FindField("data")->FindField("price")->AsInteger() != 999)
    {
        Session.Stop();
        return "new_handle_query_price_unexpected";
    }

    // 4. Session stop cleans up pinned handle
    if (!Session.Stop())
    {
        return "session_stop_failed";
    }
    if (Session.IsStarted() || Session.GetPinnedRepository().IsValid())
    {
        return "session_stop_did_not_clear_state";
    }

    return "";
}

} // namespace GV2RuntimeCore::Testing
