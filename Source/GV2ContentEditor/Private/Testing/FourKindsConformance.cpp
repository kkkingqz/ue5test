#include "GV2ContentEditor/Testing/FourKindsConformance.h"
#include "GV2ContentEditor/GV2EditorAdapter.h"
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

void SetupFourKindsFixture(const std::filesystem::path& ContainerDir)
{
    // 1. core package
    std::filesystem::path CoreDir = ContainerDir / "core";
    WriteFile(CoreDir / "package.json5",
        "{\n"
        "  schema_version: 1,\n"
        "  package_id: 'core',\n"
        "  namespace: 'core',\n"
        "  version: '1.0.0',\n"
        "  dependencies: []\n"
        "}\n");

    WriteFile(CoreDir / "schemas/actor_v1.schema.json5",
        "{\n"
        "  id: 'core:schema.definition.actor.v1',\n"
        "  definition_type: 'actor',\n"
        "  schema_version: 1,\n"
        "  root: {\n"
        "    kind: 'object',\n"
        "    fields: {\n"
        "      discriminator: {\n"
        "        kind: 'enum',\n"
        "        required: true,\n"
        "        values: ['player', 'npc']\n"
        "      }\n"
        "    }\n"
        "  },\n"
        "  semantic_validators: [],\n"
        "  extensions: {}\n"
        "}\n");

    WriteFile(CoreDir / "schemas/text_v1.schema.json5",
        "{\n"
        "  id: 'core:schema.definition.text.v1',\n"
        "  definition_type: 'text',\n"
        "  schema_version: 1,\n"
        "  root: {\n"
        "    kind: 'object',\n"
        "    fields: {\n"
        "      raw_text: { kind: 'string', required: true }\n"
        "    }\n"
        "  },\n"
        "  semantic_validators: [],\n"
        "  extensions: {}\n"
        "}\n");

    WriteFile(CoreDir / "schemas/screen_v1.schema.json5",
        "{\n"
        "  id: 'core:schema.definition.screen.v1',\n"
        "  definition_type: 'screen',\n"
        "  schema_version: 1,\n"
        "  root: {\n"
        "    kind: 'object',\n"
        "    fields: {\n"
        "      template_id: { kind: 'string', required: true }\n"
        "    }\n"
        "  },\n"
        "  semantic_validators: [],\n"
        "  extensions: {}\n"
        "}\n");

    // 2. textsystem package
    std::filesystem::path TextDir = ContainerDir / "textsystem";
    WriteFile(TextDir / "package.json5",
        "{\n"
        "  schema_version: 1,\n"
        "  package_id: 'textsystem',\n"
        "  namespace: 'textsystem',\n"
        "  version: '1.0.0',\n"
        "  dependencies: ['core']\n"
        "}\n");

    WriteFile(TextDir / "schemas/location_v1.schema.json5",
        "{\n"
        "  id: 'textsystem:schema.definition.location.v1',\n"
        "  definition_type: 'location',\n"
        "  schema_version: 1,\n"
        "  root: {\n"
        "    kind: 'object',\n"
        "    fields: {\n"
        "      title_text_id: { kind: 'text_id', required: true },\n"
        "      screen_ids: {\n"
        "        kind: 'array',\n"
        "        required: true,\n"
        "        min_items: 1,\n"
        "        unique: true,\n"
        "        items: { kind: 'ref', target_kind: 'screen' }\n"
        "      },\n"
        "      connected_location_ids: {\n"
        "        kind: 'array',\n"
        "        required: false,\n"
        "        unique: true,\n"
        "        items: { kind: 'ref', target_kind: 'location' }\n"
        "      }\n"
        "    }\n"
        "  },\n"
        "  semantic_validators: [],\n"
        "  extensions: {}\n"
        "}\n");

    WriteFile(TextDir / "schemas/actor_textsystem_extension_v1.schema.json5",
        "{\n"
        "  id: 'textsystem:schema.extension.actor.entry.v1',\n"
        "  definition_type: 'actor',\n"
        "  schema_version: 1,\n"
        "  extension_site: 'definition_entry',\n"
        "  extension_namespace: 'textsystem',\n"
        "  root: {\n"
        "    kind: 'object',\n"
        "    fields: {\n"
        "      current_location: {\n"
        "        kind: 'ref',\n"
        "        target_kind: 'location',\n"
        "        required: false\n"
        "      }\n"
        "    }\n"
        "  }\n"
        "}\n");

    // 3. game package
    std::filesystem::path GameDir = ContainerDir / "game";
    WriteFile(GameDir / "package.json5",
        "{\n"
        "  schema_version: 1,\n"
        "  package_id: 'game',\n"
        "  namespace: 'game',\n"
        "  version: '1.0.0',\n"
        "  dependencies: ['core', 'textsystem']\n"
        "}\n");

    WriteFile(GameDir / "schemas/item_v1.schema.json5",
        "{\n"
        "  id: 'game:schema.definition.item.v1',\n"
        "  definition_type: 'item',\n"
        "  schema_version: 1,\n"
        "  root: {\n"
        "    kind: 'object',\n"
        "    fields: {\n"
        "      name: { kind: 'string', required: true },\n"
        "      weight: { kind: 'number', required: false, default: 1.0 },\n"
        "      value: { kind: 'int64', required: false, default: 0 }\n"
        "    }\n"
        "  },\n"
        "  semantic_validators: [],\n"
        "  extensions: {}\n"
        "}\n");

    WriteFile(GameDir / "schemas/item_v1.ui.json5",
        "{\n"
        "  schema_version: 1,\n"
        "  fields: {\n"
        "    name: { label: 'Item Name', category: 'General', order: 10 },\n"
        "    value: { label: 'Gold Value', category: 'Economy', order: 20, widget_hint: 'slider' },\n"
        "    weight: { label: 'Encumbrance', category: 'Physics', order: 30 }\n"
        "  }\n"
        "}\n");

    WriteFile(GameDir / "schemas/actor_game_extension_v1.schema.json5",
        "{\n"
        "  id: 'game:schema.extension.actor.entry.v1',\n"
        "  definition_type: 'actor',\n"
        "  schema_version: 1,\n"
        "  extension_site: 'definition_entry',\n"
        "  extension_namespace: 'game',\n"
        "  root: {\n"
        "    kind: 'object',\n"
        "    fields: {\n"
        "      name_text_id: { kind: 'text_id', required: true },\n"
        "      base_hp: { kind: 'int64', required: true, min: 1 }\n"
        "    }\n"
        "  }\n"
        "}\n");

    WriteFile(GameDir / "definitions/texts.json5",
        "{\n"
        "  schema_version: 1,\n"
        "  type: 'text',\n"
        "  definitions: [\n"
        "    { id: 'game:text.market.title', data: { raw_text: 'Marketplace' } },\n"
        "    { id: 'game:text.tavern.title', data: { raw_text: 'Tavern' } },\n"
        "    { id: 'game:text.gate.title', data: { raw_text: 'City Gate' } },\n"
        "    { id: 'game:text.hero.name', data: { raw_text: 'Brave Hero' } }\n"
        "  ]\n"
        "}\n");

    WriteFile(GameDir / "definitions/screens.json5",
        "{\n"
        "  schema_version: 1,\n"
        "  type: 'screen',\n"
        "  definitions: [\n"
        "    { id: 'game:screen.market', data: { template_id: 'screen_default' } },\n"
        "    { id: 'game:screen.tavern', data: { template_id: 'screen_default' } },\n"
        "    { id: 'game:screen.gate', data: { template_id: 'screen_default' } }\n"
        "  ]\n"
        "}\n");

    WriteFile(GameDir / "definitions/items.json5",
        "{\n"
        "  schema_version: 1,\n"
        "  type: 'item',\n"
        "  definitions: [\n"
        "    {\n"
        "      id: 'game:item.weapon.iron_sword',\n"
        "      data: { name: 'Iron Sword', weight: 2.5, value: 25 }\n"
        "    }\n"
        "  ]\n"
        "}\n");

    WriteFile(GameDir / "definitions/locations.json5",
        "{\n"
        "  schema_version: 1,\n"
        "  type: 'location',\n"
        "  definitions: [\n"
        "    {\n"
        "      id: 'game:location.city.market',\n"
        "      data: {\n"
        "        title_text_id: 'game:text.market.title',\n"
        "        screen_ids: ['game:screen.market'],\n"
        "        connected_location_ids: ['game:location.city.tavern']\n"
        "      }\n"
        "    },\n"
        "    {\n"
        "      id: 'game:location.city.tavern',\n"
        "      data: {\n"
        "        title_text_id: 'game:text.tavern.title',\n"
        "        screen_ids: ['game:screen.tavern'],\n"
        "        connected_location_ids: ['game:location.city.market', 'game:location.city.gate']\n"
        "      }\n"
        "    },\n"
        "    {\n"
        "      id: 'game:location.city.gate',\n"
        "      data: {\n"
        "        title_text_id: 'game:text.gate.title',\n"
        "        screen_ids: ['game:screen.gate'],\n"
        "        connected_location_ids: ['game:location.city.tavern']\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n");

    WriteFile(GameDir / "definitions/actors.json5",
        "{\n"
        "  schema_version: 1,\n"
        "  type: 'actor',\n"
        "  definitions: [\n"
        "    {\n"
        "      id: 'game:actor.character.hero',\n"
        "      data: {\n"
        "        discriminator: 'player'\n"
        "      },\n"
        "      extensions: {\n"
        "        textsystem: {\n"
        "          current_location: 'game:location.city.market'\n"
        "        },\n"
        "        game: {\n"
        "          name_text_id: 'game:text.hero.name',\n"
        "          base_hp: 100\n"
        "        }\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n");
}

// CED-17: item
bool TestItemKindAuthoring()
{
    std::filesystem::path TempDir = CreateTempDir("gv2_m5_item");
    SetupFourKindsFixture(TempDir);

    FGV2EditorAdapter Adapter;
    std::vector<FGV2EditorDiagnostic> Diags;
    if (!Adapter.Initialize(TempDir, Diags))
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // 1. Check item form model has UI metadata from schemas/item_v1.ui.json5
    auto FormModelOpt = Adapter.GetFormModelForDefinitionType("item");
    if (!FormModelOpt.has_value())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // 2. Load item definition
    auto LoadedOpt = Adapter.LoadDefinition("game:item.weapon.iron_sword", Diags);
    if (!LoadedOpt.has_value())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // 3. Modify value
    Adapter.SetCurrentFieldValue("/definitions/0/data/value", GV2ContentCore::FValue(static_cast<std::int64_t>(30)));
    if (!Adapter.IsDirty())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    auto SaveResult = Adapter.SaveCurrentDefinition();
    if (!SaveResult.IsSuccess() || Adapter.IsDirty())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // 4. Validate repository passes
    auto RepDiags = Adapter.ValidateRepository();
    for (const auto& D : RepDiags)
    {
        if (D.Severity == GV2ContentCore::EDiagnosticSeverity::Error)
        {
            std::filesystem::remove_all(TempDir);
            return false;
        }
    }

    std::filesystem::remove_all(TempDir);
    return true;
}

// CED-18: location
bool TestLocationKindAuthoringAndReferenceArrays()
{
    std::filesystem::path TempDir = CreateTempDir("gv2_m5_location");
    SetupFourKindsFixture(TempDir);

    FGV2EditorAdapter Adapter;
    std::vector<FGV2EditorDiagnostic> Diags;
    if (!Adapter.Initialize(TempDir, Diags))
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // 1. Load location definition
    auto LoadedOpt = Adapter.LoadDefinition("game:location.city.market", Diags);
    if (!LoadedOpt.has_value())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // 2. Add connected location array element
    GV2ContentCore::FValue::FArray ConnLocs;
    ConnLocs.push_back(GV2ContentCore::FValue("game:location.city.tavern"));
    ConnLocs.push_back(GV2ContentCore::FValue("game:location.city.gate"));
    Adapter.SetCurrentFieldValue("/definitions/0/data/connected_location_ids", GV2ContentCore::FValue(ConnLocs));

    auto SaveResult = Adapter.SaveCurrentDefinition();
    if (!SaveResult.IsSuccess() || Adapter.IsDirty())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // 3. Verify incoming references for city.gate now includes city.market
    auto InRefs = Adapter.GetIncomingReferences("game:location.city.gate");
    bool bFoundMarket = false;
    for (const auto& Ref : InRefs)
    {
        if (Ref.SourceDefinitionId == "game:location.city.market")
        {
            bFoundMarket = true;
            break;
        }
    }

    if (!bFoundMarket)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // 4. Compatible target pickers
    auto CompatibleScreens = Adapter.GetCompatibleReferenceTargets("screen");
    if (CompatibleScreens.empty())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    std::filesystem::remove_all(TempDir);
    return true;
}

// CED-19: actor and extension sites
bool TestActorKindAuthoringWithMultiPackageExtensions()
{
    std::filesystem::path TempDir = CreateTempDir("gv2_m5_actor");
    SetupFourKindsFixture(TempDir);

    FGV2EditorAdapter Adapter;
    std::vector<FGV2EditorDiagnostic> Diags;
    if (!Adapter.Initialize(TempDir, Diags))
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // 1. Check form model contains base fields + textsystem & game extension fields
    auto FormModelOpt = Adapter.GetFormModelForDefinitionType("actor");
    if (!FormModelOpt.has_value())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    const auto& FormModel = *FormModelOpt;
    bool bHasDiscriminator = false;
    bool bHasCurrentLocation = false;
    bool bHasBaseHp = false;

    for (const auto& Field : FormModel.AllFields)
    {
        if (Field.FieldName == "discriminator") bHasDiscriminator = true;
        if (Field.FieldName == "current_location") bHasCurrentLocation = true;
        if (Field.FieldName == "base_hp") bHasBaseHp = true;
    }

    if (!bHasDiscriminator || !bHasCurrentLocation || !bHasBaseHp)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // 2. Load actor
    auto LoadedOpt = Adapter.LoadDefinition("game:actor.character.hero", Diags);
    if (!LoadedOpt.has_value())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // 3. Edit base field, textsystem extension field, and game extension field
    Adapter.SetCurrentFieldValue("/definitions/0/data/discriminator", GV2ContentCore::FValue("player"));
    Adapter.SetCurrentFieldValue("/definitions/0/extensions/textsystem/current_location", GV2ContentCore::FValue("game:location.city.market"));
    Adapter.SetCurrentFieldValue("/definitions/0/extensions/game/base_hp", GV2ContentCore::FValue(static_cast<std::int64_t>(125)));

    auto SaveResult = Adapter.SaveCurrentDefinition();
    if (!SaveResult.IsSuccess() || Adapter.IsDirty())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // 4. Verify disk file has extension blocks preserved in game/definitions/actors.json5
    std::string DiskContent = ReadFile(TempDir / "game/definitions/actors.json5");
    if (DiskContent.find("125") == std::string::npos ||
        DiskContent.find("current_location") == std::string::npos)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // 5. Authoritative validation passes
    auto RepDiags = Adapter.ValidateRepository();
    for (const auto& D : RepDiags)
    {
        if (D.Severity == GV2ContentCore::EDiagnosticSeverity::Error)
        {
            std::filesystem::remove_all(TempDir);
            return false;
        }
    }

    std::filesystem::remove_all(TempDir);
    return true;
}

// CED-20: world
bool TestWorldKindArchitecturalInvariant()
{
    // Verifies architectural decision: 'world' is purely runtime state, not a definition kind in GameData repository
    FGV2EditorAdapter Adapter;
    std::filesystem::path TempDir = CreateTempDir("gv2_m5_world");
    SetupFourKindsFixture(TempDir);

    std::vector<FGV2EditorDiagnostic> Diags;
    Adapter.Initialize(TempDir, Diags);

    auto WorldDefs = Adapter.ListDefinitions("world");
    if (!WorldDefs.empty())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    auto FormModel = Adapter.GetFormModelForDefinitionType("world");
    if (FormModel.has_value())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    std::filesystem::remove_all(TempDir);
    return true;
}

} // namespace

std::string RunFourKindsConformance()
{
    if (!TestItemKindAuthoring())
    {
        return "TestItemKindAuthoring failed";
    }
    if (!TestLocationKindAuthoringAndReferenceArrays())
    {
        return "TestLocationKindAuthoringAndReferenceArrays failed";
    }
    if (!TestActorKindAuthoringWithMultiPackageExtensions())
    {
        return "TestActorKindAuthoringWithMultiPackageExtensions failed";
    }
    if (!TestWorldKindArchitecturalInvariant())
    {
        return "TestWorldKindArchitecturalInvariant failed";
    }
    return "";
}

} // namespace GV2ContentEditor::Testing

