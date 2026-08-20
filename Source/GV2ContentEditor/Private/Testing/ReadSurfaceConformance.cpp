#include "GV2ContentEditor/Testing/ReadSurfaceConformance.h"
#include "GV2ContentEditor/FieldAdapterRegistry.h"
#include "GV2ContentEditor/GV2EditorAdapter.h"
#include "GV2ContentEditor/ReferenceScanner.h"
#include "GV2ContentEditor/SchemaFormModel.h"
#include "GV2ContentCore/AuthoringMetadata.h"
#include "GV2ContentCore/ExtensionSchema.h"
#include "GV2ContentCore/FieldValidation.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"
#include "GV2ContentCore/ScalarValidation.h"
#include "GV2ContentCore/SchemaRegistry.h"

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

void SetupComprehensiveGameData(const std::filesystem::path& ContainerDir)
{
    std::filesystem::path CoreDir = ContainerDir / "core";

    // package.json5
    WriteFile(CoreDir / "package.json5",
        "{\n"
        "  schema_version: 1,\n"
        "  package_id: 'core',\n"
        "  namespace: 'core',\n"
        "  version: '1.0.0',\n"
        "  dependencies: []\n"
        "}\n");

    // schemas/item.schema.json5
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

    // schemas/item.ui.json5
    WriteFile(CoreDir / "schemas/item.ui.json5",
        "{\n"
        "  schema_version: 1,\n"
        "  fields: {\n"
        "    name: { label: 'Item Name', description: 'Canonical display name of the item', category: 'General', order: 10 },\n"
        "    value: { label: 'Gold Value', description: 'Base gold cost in merchant shops', category: 'Economy', order: 20, widget_hint: 'slider' },\n"
        "    weight: { label: 'Encumbrance Weight', description: 'Weight in kilograms', category: 'Physics', order: 30 }\n"
        "  }\n"
        "}\n");

    // schemas/actor.schema.json5
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

    // schemas/location.schema.json5
    WriteFile(CoreDir / "schemas/location.schema.json5",
        "{\n"
        "  schema_version: 1,\n"
        "  id: 'core:schema.definition.location.v1',\n"
        "  definition_type: 'location',\n"
        "  fields: {\n"
        "    name: { type: 'string', required: true }\n"
        "  }\n"
        "}\n");

    // definitions/items.json5
    WriteFile(CoreDir / "definitions/items.json5",
        "{\n"
        "  schema_version: 1,\n"
        "  definition_type: 'item',\n"
        "  definitions: [\n"
        "    {\n"
        "      id: 'core:item.iron_sword',\n"
        "      data: { name: 'Iron Sword', weight: 2.5, value: 25 }\n"
        "    },\n"
        "    {\n"
        "      id: 'core:item.magic_potion',\n"
        "      data: { name: 'Magic Potion', weight: 0.5, value: 50 }\n"
        "    }\n"
        "  ]\n"
        "}\n");

    // definitions/locations.json5
    WriteFile(CoreDir / "definitions/locations.json5",
        "{\n"
        "  schema_version: 1,\n"
        "  definition_type: 'location',\n"
        "  definitions: [\n"
        "    {\n"
        "      id: 'core:location.tavern',\n"
        "      data: { name: 'Cozy Tavern' }\n"
        "    }\n"
        "  ]\n"
        "}\n");

    // definitions/actors.json5
    WriteFile(CoreDir / "definitions/actors.json5",
        "{\n"
        "  schema_version: 1,\n"
        "  definition_type: 'actor',\n"
        "  definitions: [\n"
        "    {\n"
        "      id: 'core:actor.hero',\n"
        "      data: {\n"
        "        name: 'Brave Hero',\n"
        "        health: 120,\n"
        "        equipped_weapon: 'core:item.iron_sword',\n"
        "        home_location: 'core:location.tavern'\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n");
}

bool TestDefinitionBrowserIndexingAndFiltering()
{
    std::filesystem::path TempDir = CreateTempDir("gv2_read_browser");
    SetupComprehensiveGameData(TempDir);

    FGV2EditorAdapter Adapter;
    std::vector<FGV2EditorDiagnostic> Diags;
    if (!Adapter.Initialize(TempDir, Diags))
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    auto AllDefs = Adapter.ListDefinitions();
    if (AllDefs.size() != 4) // 2 items + 1 location + 1 actor
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    auto Items = Adapter.ListDefinitions("item");
    if (Items.size() != 2)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    auto Actors = Adapter.ListDefinitions("actor");
    if (Actors.size() != 1 || Actors[0].Id != "core:actor.hero")
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    std::filesystem::remove_all(TempDir);
    return true;
}

bool TestFieldAdapterRegistryResolution()
{
    auto& Registry = FGV2FieldAdapterRegistry::Get();

    // 1. Integer without hint
    GV2ContentCore::FCompiledFieldSpec IntSpec;
    IntSpec.Kind = GV2ContentCore::EFieldKind::Scalar;
    IntSpec.Scalar = GV2ContentCore::FScalarFieldSpec{};
    IntSpec.Scalar->Kind = GV2ContentCore::EScalarFieldKind::Integer;

    auto IntDesc = Registry.DescribeField(IntSpec, nullptr);
    if (IntDesc.ControlType != EFieldControlType::IntegerNumeric)
    {
        return false;
    }

    // 2. Integer with slider hint
    GV2ContentCore::FFieldUiMetadata SliderMeta;
    SliderMeta.WidgetHint = "slider";
    auto SliderDesc = Registry.DescribeField(IntSpec, &SliderMeta);
    if (SliderDesc.ControlType != EFieldControlType::Slider)
    {
        return false;
    }

    // 3. String with multiline hint
    GV2ContentCore::FCompiledFieldSpec StrSpec;
    StrSpec.Kind = GV2ContentCore::EFieldKind::Scalar;
    StrSpec.Scalar = GV2ContentCore::FScalarFieldSpec{};
    StrSpec.Scalar->Kind = GV2ContentCore::EScalarFieldKind::String;

    GV2ContentCore::FFieldUiMetadata MultilineMeta;
    MultilineMeta.WidgetHint = "multiline";
    auto MultilineDesc = Registry.DescribeField(StrSpec, &MultilineMeta);
    if (MultilineDesc.ControlType != EFieldControlType::MultilineText)
    {
        return false;
    }

    // 4. Reference
    GV2ContentCore::FCompiledFieldSpec RefSpec;
    RefSpec.Kind = GV2ContentCore::EFieldKind::Reference;
    RefSpec.ExpectedStableIdKind = "location";
    auto RefDesc = Registry.DescribeField(RefSpec, nullptr);
    if (RefDesc.ControlType != EFieldControlType::ReferencePicker || RefDesc.TargetReferenceKind != "location")
    {
        return false;
    }

    // 5. Enum
    GV2ContentCore::FCompiledFieldSpec EnumSpec;
    EnumSpec.Kind = GV2ContentCore::EFieldKind::Scalar;
    EnumSpec.Scalar = GV2ContentCore::FScalarFieldSpec{};
    EnumSpec.Scalar->Kind = GV2ContentCore::EScalarFieldKind::Enum;
    EnumSpec.Scalar->EnumValues = { GV2ContentCore::FValue("Common"), GV2ContentCore::FValue("Rare"), GV2ContentCore::FValue("Epic") };

    auto EnumDesc = Registry.DescribeField(EnumSpec, nullptr);
    if (EnumDesc.ControlType != EFieldControlType::EnumDropdown || EnumDesc.EnumChoices.size() != 3)
    {
        return false;
    }

    return true;
}

bool TestDynamicSchemaFormGeneration()
{
    std::filesystem::path TempDir = CreateTempDir("gv2_read_form");
    SetupComprehensiveGameData(TempDir);

    FGV2EditorAdapter Adapter;
    std::vector<FGV2EditorDiagnostic> Diags;
    Adapter.Initialize(TempDir, Diags);

    auto FormModelOpt = Adapter.GetFormModelForDefinitionType("item");
    if (!FormModelOpt.has_value())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    const auto& Model = *FormModelOpt;
    if (Model.DefinitionType != "item")
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // CED-11: UI metadata applied correctly
    if (Model.AllFields.size() != 3)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Fields should be ordered by Order: name (10), value (20), weight (30)
    if (Model.AllFields[0].FieldName != "name" || Model.AllFields[0].DisplayLabel != "Item Name" || Model.AllFields[0].Category != "General")
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    if (Model.AllFields[1].FieldName != "value" || Model.AllFields[1].DisplayLabel != "Gold Value" || Model.AllFields[1].Category != "Economy")
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    if (Model.AllFields[2].FieldName != "weight" || Model.AllFields[2].DisplayLabel != "Encumbrance Weight" || Model.AllFields[2].Category != "Physics")
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Check categories
    if (Model.Categories.size() != 3)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    std::filesystem::remove_all(TempDir);
    return true;
}

bool TestReferenceScannerBidirectional()
{
    std::filesystem::path TempDir = CreateTempDir("gv2_read_refs");
    SetupComprehensiveGameData(TempDir);

    FGV2EditorAdapter Adapter;
    std::vector<FGV2EditorDiagnostic> Diags;
    Adapter.Initialize(TempDir, Diags);

    // Load hero actor
    Adapter.LoadDefinition("core:actor.hero", Diags);

    // Outgoing references for hero (Uses)
    auto OutRefs = Adapter.GetOutgoingReferences();
    if (OutRefs.size() != 2)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    bool bFoundSword = false;
    bool bFoundTavern = false;
    for (const auto& Ref : OutRefs)
    {
        if (Ref.TargetDefinitionId == "core:item.iron_sword" && Ref.TargetKind == "item") bFoundSword = true;
        if (Ref.TargetDefinitionId == "core:location.tavern" && Ref.TargetKind == "location") bFoundTavern = true;
    }

    if (!bFoundSword || !bFoundTavern)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Incoming references for iron_sword (Used by)
    auto InRefs = Adapter.GetIncomingReferences("core:item.iron_sword");
    if (InRefs.size() != 1 || InRefs[0].SourceDefinitionId != "core:actor.hero")
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Compatible targets for item picker
    auto CompatibleItems = Adapter.GetCompatibleReferenceTargets("item");
    if (CompatibleItems.size() != 2) // iron_sword, magic_potion
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    std::filesystem::remove_all(TempDir);
    return true;
}

} // namespace

std::string RunReadSurfaceConformance()
{
    if (!TestDefinitionBrowserIndexingAndFiltering())
    {
        return "TestDefinitionBrowserIndexingAndFiltering failed";
    }
    if (!TestFieldAdapterRegistryResolution())
    {
        return "TestFieldAdapterRegistryResolution failed";
    }
    if (!TestDynamicSchemaFormGeneration())
    {
        return "TestDynamicSchemaFormGeneration failed";
    }
    if (!TestReferenceScannerBidirectional())
    {
        return "TestReferenceScannerBidirectional failed";
    }
    return "";
}

} // namespace GV2ContentEditor::Testing
