#include "GV2ContentEditor/Testing/WriteSurfaceConformance.h"
#include "GV2ContentEditor/GV2EditorAdapter.h"
#include "GV2ContentAuthoring/AuthoringService.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/Value.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace GV2ContentEditor::Testing
{

namespace
{

std::filesystem::path CreateTempDir(const std::string& Prefix)
{
    const auto Now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::filesystem::path TempDir = std::filesystem::temp_directory_path() / (Prefix + "_" + std::to_string(Now));
    std::filesystem::create_directories(TempDir);
    return TempDir;
}

bool WriteFile(const std::filesystem::path& Path, const std::string& Content)
{
    std::error_code Ec;
    std::filesystem::create_directories(Path.parent_path(), Ec);
    std::ofstream Stream(Path, std::ios::binary | std::ios::trunc);
    if (!Stream.is_open()) return false;
    Stream.write(Content.data(), static_cast<std::streamsize>(Content.size()));
    return Stream.good();
}

std::string ReadFile(const std::filesystem::path& Path)
{
    std::ifstream Stream(Path, std::ios::binary);
    if (!Stream.is_open()) return "";
    return std::string((std::istreambuf_iterator<char>(Stream)), std::istreambuf_iterator<char>());
}

void SetupWriteFixture(const std::filesystem::path& ContainerDir)
{
    std::filesystem::path CoreDir = ContainerDir / "core";

    WriteFile(CoreDir / "package.json5",
        "{\n"
        "  schema_version: 1,\n"
        "  package_id: 'core',\n"
        "  namespace: 'core',\n"
        "  version: '1.0.0',\n"
        "  dependencies: []\n"
        "}\n");

    WriteFile(CoreDir / "schemas/item.schema.json5",
        "{\n"
        "  schema_version: 1,\n"
        "  id: 'core:schema.definition.item.v1',\n"
        "  definition_type: 'item',\n"
        "  fields: {\n"
        "    name: { type: 'string', required: true },\n"
        "    weight: { type: 'number', required: false, default: 1.0 },\n"
        "    value: { type: 'integer', required: false, default: 0 }\n"
        "  }\n"
        "}\n");

    WriteFile(CoreDir / "schemas/actor.schema.json5",
        "{\n"
        "  schema_version: 1,\n"
        "  id: 'core:schema.definition.actor.v1',\n"
        "  definition_type: 'actor',\n"
        "  fields: {\n"
        "    name: { type: 'string', required: true },\n"
        "    health: { type: 'integer', required: false, default: 100 },\n"
        "    equipped_weapon: { type: 'reference', target_kind: 'item', required: false },\n"
        "    home_location: { type: 'reference', target_kind: 'location', required: false }\n"
        "  }\n"
        "}\n");

    WriteFile(CoreDir / "schemas/location.schema.json5",
        "{\n"
        "  schema_version: 1,\n"
        "  id: 'core:schema.definition.location.v1',\n"
        "  definition_type: 'location',\n"
        "  fields: {\n"
        "    name: { type: 'string', required: true }\n"
        "  }\n"
        "}\n");

    WriteFile(CoreDir / "definitions/items.json5",
        "{\n"
        "  schema_version: 1,\n"
        "  definition_type: 'item',\n"
        "  definitions: [\n"
        "    {\n"
        "      id: 'core:item.iron_sword',\n"
        "      data: { name: 'Iron Sword', weight: 2.5, value: 25 }\n"
        "    }\n"
        "  ]\n"
        "}\n");

    WriteFile(CoreDir / "definitions/locations.json5",
        "{\n"
        "  schema_version: 1,\n"
        "  definition_type: 'location',\n"
        "  definitions: [\n"
        "    {\n"
        "      id: 'core:location.tavern',\n"
        "      data: { name: 'Tavern' }\n"
        "    }\n"
        "  ]\n"
        "}\n");

    WriteFile(CoreDir / "definitions/actors.json5",
        "{\n"
        "  schema_version: 1,\n"
        "  definition_type: 'actor',\n"
        "  definitions: [\n"
        "    {\n"
        "      id: 'core:actor.hero',\n"
        "      data: {\n"
        "        name: 'Hero',\n"
        "        health: 100,\n"
        "        equipped_weapon: 'core:item.iron_sword',\n"
        "        home_location: 'core:location.tavern'\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n");
}

bool TestBatchFieldEditingAndSave()
{
    std::filesystem::path TempDir = CreateTempDir("gv2_write_save");
    SetupWriteFixture(TempDir);

    FGV2EditorAdapter Adapter;
    std::vector<FGV2EditorDiagnostic> Diags;
    if (!Adapter.Initialize(TempDir, Diags))
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    auto LoadedDefOpt = Adapter.LoadDefinition("core:item.iron_sword", Diags);
    if (!LoadedDefOpt.has_value() || Adapter.IsDirty())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Edit 3 fields
    Adapter.SetCurrentFieldValue("/definitions/0/data/name", GV2ContentCore::FValue("Enchanted Sword"));
    Adapter.SetCurrentFieldValue("/definitions/0/data/weight", GV2ContentCore::FValue(3.0));
    Adapter.SetCurrentFieldValue("/definitions/0/data/value", GV2ContentCore::FValue(static_cast<std::int64_t>(150)));

    if (!Adapter.IsDirty() || Adapter.GetDirtyFields().size() != 3)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Test discard changes
    Adapter.DiscardCurrentChanges();
    if (Adapter.IsDirty() || !Adapter.GetDirtyFields().empty())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    auto CurrentName = Adapter.GetCurrentFieldValue("/definitions/0/data/name");
    if (!CurrentName.has_value() || CurrentName->AsString() != "Iron Sword")
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Re-apply and save
    Adapter.SetCurrentFieldValue("/definitions/0/data/name", GV2ContentCore::FValue("Enchanted Sword"));
    Adapter.SetCurrentFieldValue("/definitions/0/data/weight", GV2ContentCore::FValue(3.0));
    Adapter.SetCurrentFieldValue("/definitions/0/data/value", GV2ContentCore::FValue(static_cast<std::int64_t>(150)));

    auto SaveResult = Adapter.SaveCurrentDefinition();
    if (!SaveResult.IsSuccess() || Adapter.IsDirty())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Verify on disk
    std::string DiskContent = ReadFile(TempDir / "core/definitions/items.json5");
    if (DiskContent.find("Enchanted Sword") == std::string::npos ||
        DiskContent.find("150") == std::string::npos)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    std::filesystem::remove_all(TempDir);
    return true;
}

bool TestDefinitionCreationDuplicationDeletionRenaming()
{
    std::filesystem::path TempDir = CreateTempDir("gv2_write_crud");
    SetupWriteFixture(TempDir);

    FGV2EditorAdapter Adapter;
    std::vector<FGV2EditorDiagnostic> Diags;
    Adapter.Initialize(TempDir, Diags);

    // 1. Create new definition
    GV2ContentCore::FValue::FObject Obj;
    Obj.emplace_back("name", GV2ContentCore::FValue("Steel Axe"));
    Obj.emplace_back("weight", GV2ContentCore::FValue(4.0));
    Obj.emplace_back("value", GV2ContentCore::FValue(static_cast<std::int64_t>(40)));
    GV2ContentCore::FValue InitialItemData(std::move(Obj));

    auto CreateResult = Adapter.CreateDefinition("core", "core:item.steel_axe", "item", InitialItemData);
    if (!CreateResult.IsSuccess())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    auto ItemDefs = Adapter.ListDefinitions("item");
    if (ItemDefs.size() != 2)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // 2. Duplicate definition
    auto DupResult = Adapter.DuplicateDefinition("core:item.steel_axe", "core:item.steel_axe_alt");
    if (!DupResult.IsSuccess())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    ItemDefs = Adapter.ListDefinitions("item");
    if (ItemDefs.size() != 3)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // 3. Delete unreferenced definition (steel_axe_alt)
    auto DelResult = Adapter.DeleteDefinition("core:item.steel_axe_alt");
    if (!DelResult.IsSuccess())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    ItemDefs = Adapter.ListDefinitions("item");
    if (ItemDefs.size() != 2)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // 4. Delete referenced definition (iron_sword is equipped by hero) -> should be rejected!
    auto DelReferencedResult = Adapter.DeleteDefinition("core:item.iron_sword");
    if (DelReferencedResult.IsSuccess())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // 5. Rename definition (rename tavern -> old_tavern)
    auto RenameResult = Adapter.RenameDefinition("core:location.tavern", "core:location.old_tavern");
    if (!RenameResult.IsSuccess())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Check that actor hero references the renamed location
    std::string ActorContent = ReadFile(TempDir / "core/definitions/actors.json5");
    if (ActorContent.find("core:location.old_tavern") == std::string::npos ||
        ActorContent.find("core:location.tavern") != std::string::npos)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    std::filesystem::remove_all(TempDir);
    return true;
}

bool TestCliBitwiseParityAndAtomicity()
{
    std::filesystem::path TempDirA = CreateTempDir("gv2_parity_a");
    std::filesystem::path TempDirB = CreateTempDir("gv2_parity_b");
    SetupWriteFixture(TempDirA);
    SetupWriteFixture(TempDirB);

    // Apply batch edit in TempDirA via FGV2EditorAdapter
    FGV2EditorAdapter AdapterA;
    std::vector<FGV2EditorDiagnostic> DiagsA;
    AdapterA.Initialize(TempDirA, DiagsA);
    AdapterA.LoadDefinition("core:item.iron_sword", DiagsA);
    AdapterA.SetCurrentFieldValue("/definitions/0/data/name", GV2ContentCore::FValue("Fine Iron Sword"));
    AdapterA.SetCurrentFieldValue("/definitions/0/data/value", GV2ContentCore::FValue(static_cast<std::int64_t>(35)));
    auto SaveResultA = AdapterA.SaveCurrentDefinition();
    if (!SaveResultA.IsSuccess())
    {
        std::filesystem::remove_all(TempDirA);
        std::filesystem::remove_all(TempDirB);
        return false;
    }

    // Apply exact same batch edit in TempDirB via FAuthoringService::BatchSetFields (underlying CLI operation)
    GV2ContentAuthoring::FBatchSetFieldsParams ParamsB;
    ParamsB.PackageRoot = TempDirB / "core";
    ParamsB.DefinitionId = "core:item.iron_sword";
    ParamsB.Changes.push_back({ "/definitions/0/data/name", GV2ContentCore::FValue("Fine Iron Sword") });
    ParamsB.Changes.push_back({ "/definitions/0/data/value", GV2ContentCore::FValue(static_cast<std::int64_t>(35)) });
    auto ResultB = GV2ContentAuthoring::FAuthoringService::BatchSetFields(ParamsB);
    if (!ResultB.IsSuccess())
    {
        std::filesystem::remove_all(TempDirA);
        std::filesystem::remove_all(TempDirB);
        return false;
    }

    // Compare files byte-for-byte
    std::string ContentA = ReadFile(TempDirA / "core/definitions/items.json5");
    std::string ContentB = ReadFile(TempDirB / "core/definitions/items.json5");
    if (ContentA != ContentB)
    {
        std::filesystem::remove_all(TempDirA);
        std::filesystem::remove_all(TempDirB);
        return false;
    }

    // Test mid-batch failure leaves file completely untouched
    std::string PreFailContent = ReadFile(TempDirA / "core/definitions/items.json5");
    AdapterA.LoadDefinition("core:item.iron_sword", DiagsA);
    AdapterA.SetCurrentFieldValue("/definitions/0/data/name", GV2ContentCore::FValue("Broken Sword"));
    AdapterA.SetCurrentFieldValue("/nonexistent/field", GV2ContentCore::FValue("invalid"));
    auto FailSave = AdapterA.SaveCurrentDefinition();
    if (FailSave.IsSuccess())
    {
        std::filesystem::remove_all(TempDirA);
        std::filesystem::remove_all(TempDirB);
        return false;
    }

    std::string PostFailContent = ReadFile(TempDirA / "core/definitions/items.json5");
    if (PreFailContent != PostFailContent)
    {
        std::filesystem::remove_all(TempDirA);
        std::filesystem::remove_all(TempDirB);
        return false;
    }

    std::filesystem::remove_all(TempDirA);
    std::filesystem::remove_all(TempDirB);
    return true;
}

} // namespace

std::string RunWriteSurfaceConformance()
{
    if (!TestBatchFieldEditingAndSave())
    {
        return "TestBatchFieldEditingAndSave failed";
    }
    if (!TestDefinitionCreationDuplicationDeletionRenaming())
    {
        return "TestDefinitionCreationDuplicationDeletionRenaming failed";
    }
    if (!TestCliBitwiseParityAndAtomicity())
    {
        return "TestCliBitwiseParityAndAtomicity failed";
    }
    return "";
}

} // namespace GV2ContentEditor::Testing
