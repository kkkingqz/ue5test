#include "GV2ContentEditor/Testing/EditorAdapterConformance.h"
#include "GV2ContentEditor/Testing/ReadSurfaceConformance.h"
#include "GV2ContentEditor/Testing/WriteSurfaceConformance.h"
#include "GV2ContentEditor/Testing/FourKindsConformance.h"
#include "GV2ContentEditor/GV2EditorAdapter.h"
#include "GV2ContentEditor/EditorAdapterTypes.h"
#include "GV2ContentCore/Diagnostic.h"
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
    return std::string(
        std::istreambuf_iterator<char>(Stream),
        std::istreambuf_iterator<char>());
}

void SetupTestGameData(const std::filesystem::path& ContainerDir, const std::string& ItemsContent)
{
    std::filesystem::path CoreDir = ContainerDir / "core";

    // package.json5
    std::string PackageJson5 =
        "{\n"
        "  schema_version: 1,\n"
        "  package_id: 'core',\n"
        "  namespace: 'core',\n"
        "  version: '1.0.0',\n"
        "  dependencies: []\n"
        "}\n";
    WriteFile(CoreDir / "package.json5", PackageJson5);

    // schemas/item.schema.json5
    std::string SchemaJson5 =
        "{\n"
        "  schema_version: 1,\n"
        "  id: 'core:schema.definition.item.v1',\n"
        "  definition_type: 'item',\n"
        "  root: { kind: 'object', fields: {\n"
        "    name: { kind: 'string', required: true },\n"
        "    weight: { kind: 'number', required: false, default: 1.0 },\n"
        "    value: { kind: 'int64', required: false, default: 0 }\n"
        "  } },\n"
        "  semantic_validators: [],\n"
        "  extensions: {}\n"
        "}\n";
    WriteFile(CoreDir / "schemas/item.schema.json5", SchemaJson5);

    // definitions/items.json5
    WriteFile(CoreDir / "definitions/items.json5", ItemsContent);
}

bool TestAdapterInitializationAndIndexing()
{
    std::filesystem::path TempDir = CreateTempDir("gv2_adapter_init");
    std::string InitialItems =
        "{\n"
        "  schema_version: 1,\n"
        "  type: 'item',\n"
        "  definitions: [\n"
        "    {\n"
        "      id: 'core:item.sword',\n"
        "      data: {\n"
        "        name: 'Iron Sword',\n"
        "        weight: 2.5,\n"
        "        value: 10\n"
        "      }\n"
        "    },\n"
        "    {\n"
        "      id: 'core:item.shield',\n"
        "      data: {\n"
        "        name: 'Wooden Shield',\n"
        "        weight: 4.0,\n"
        "        value: 15\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n";

    SetupTestGameData(TempDir, InitialItems);

    FGV2EditorAdapter Adapter;
    std::vector<FGV2EditorDiagnostic> Diags;
    if (!Adapter.Initialize(TempDir, Diags))
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    auto AllDefs = Adapter.ListDefinitions();
    if (AllDefs.size() != 2)
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

    auto OtherDefs = Adapter.ListDefinitions("actor");
    if (!OtherDefs.empty())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    auto Types = Adapter.GetAvailableDefinitionTypes();
    if (std::find(Types.begin(), Types.end(), "item") == Types.end())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    std::filesystem::remove_all(TempDir);
    return true;
}

bool TestAdapterLoadAndDirtyState()
{
    std::filesystem::path TempDir = CreateTempDir("gv2_adapter_dirty");
    std::string InitialItems =
        "{\n"
        "  schema_version: 1,\n"
        "  type: 'item',\n"
        "  definitions: [\n"
        "    {\n"
        "      id: 'core:item.sword',\n"
        "      data: {\n"
        "        name: 'Iron Sword',\n"
        "        weight: 2.5,\n"
        "        value: 10\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n";

    SetupTestGameData(TempDir, InitialItems);

    FGV2EditorAdapter Adapter;
    std::vector<FGV2EditorDiagnostic> Diags;
    Adapter.Initialize(TempDir, Diags);

    auto Loaded = Adapter.LoadDefinition("core:item.sword", Diags);
    if (!Loaded.has_value() || Adapter.GetCurrentDefinition() == nullptr)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    if (Adapter.IsDirty())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Modify a field
    Adapter.SetCurrentFieldValue("/data/name", GV2ContentCore::FValue("Steel Sword"));
    if (!Adapter.IsDirty())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    auto CurrentVal = Adapter.GetCurrentFieldValue("/data/name");
    if (!CurrentVal.has_value() || !CurrentVal->IsString() || CurrentVal->AsString() != "Steel Sword")
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Revert back to baseline value -> should clear dirty
    Adapter.SetCurrentFieldValue("/data/name", GV2ContentCore::FValue("Iron Sword"));
    if (Adapter.IsDirty())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Set dirty again then discard
    Adapter.SetCurrentFieldValue("/data/name", GV2ContentCore::FValue("Steel Sword"));
    if (!Adapter.IsDirty())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    Adapter.DiscardCurrentChanges();
    if (Adapter.IsDirty())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    auto RevertedVal = Adapter.GetCurrentFieldValue("/data/name");
    if (!RevertedVal.has_value() || !RevertedVal->IsString() || RevertedVal->AsString() != "Iron Sword")
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    std::filesystem::remove_all(TempDir);
    return true;
}

bool TestAdapterAtomicSave()
{
    std::filesystem::path TempDir = CreateTempDir("gv2_adapter_save");
    std::string InitialItems =
        "// Sword definition file\n"
        "{\n"
        "  schema_version: 1,\n"
        "  type: 'item',\n"
        "  definitions: [\n"
        "    {\n"
        "      // The classic iron blade\n"
        "      id: 'core:item.sword',\n"
        "      data: {\n"
        "        name: 'Iron Sword',\n"
        "        weight: 2.5,\n"
        "        value: 10\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n";

    SetupTestGameData(TempDir, InitialItems);

    FGV2EditorAdapter Adapter;
    std::vector<FGV2EditorDiagnostic> Diags;
    Adapter.Initialize(TempDir, Diags);

    Adapter.LoadDefinition("core:item.sword", Diags);
    Adapter.SetCurrentFieldValue("/data/name", GV2ContentCore::FValue("Mithril Sword"));
    Adapter.SetCurrentFieldValue("/data/value", GV2ContentCore::FValue(static_cast<std::int64_t>(100)));

    auto SaveRes = Adapter.SaveCurrentDefinition();
    if (!SaveRes.IsSuccess())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    if (Adapter.IsDirty())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    std::filesystem::path ItemsFile = TempDir / "core/definitions/items.json5";
    std::string DiskContent = ReadFile(ItemsFile);

    if (DiskContent.find("Mithril Sword") == std::string::npos ||
        DiskContent.find("// The classic iron blade") == std::string::npos ||
        DiskContent.find("// Sword definition file") == std::string::npos)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    std::filesystem::remove_all(TempDir);
    return true;
}

bool TestAdapterStaleFileDetectionAndRejection()
{
    std::filesystem::path TempDir = CreateTempDir("gv2_adapter_stale");
    std::string InitialItems =
        "{\n"
        "  schema_version: 1,\n"
        "  type: 'item',\n"
        "  definitions: [\n"
        "    {\n"
        "      id: 'core:item.sword',\n"
        "      data: {\n"
        "        name: 'Iron Sword',\n"
        "        weight: 2.5,\n"
        "        value: 10\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n";

    SetupTestGameData(TempDir, InitialItems);

    FGV2EditorAdapter Adapter;
    std::vector<FGV2EditorDiagnostic> Diags;
    Adapter.Initialize(TempDir, Diags);

    Adapter.LoadDefinition("core:item.sword", Diags);

    // Initial state -> file matches loaded stamp
    if (!Adapter.CheckFileState())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Modify file externally on disk
    std::filesystem::path ItemsFile = TempDir / "core/definitions/items.json5";
    std::string ExternalContent =
        "{\n"
        "  schema_version: 1,\n"
        "  type: 'item',\n"
        "  definitions: [\n"
        "    {\n"
        "      id: 'core:item.sword',\n"
        "      data: {\n"
        "        name: 'External Sword',\n"
        "        weight: 5.0,\n"
        "        value: 50\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n";
    WriteFile(ItemsFile, ExternalContent);

    // CED-07: Adapter detects external change without writing
    if (Adapter.CheckFileState())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Attempting to save dirty edits must fail with StaleFileState
    Adapter.SetCurrentFieldValue("/data/name", GV2ContentCore::FValue("Conflicted Sword"));
    auto SaveRes = Adapter.SaveCurrentDefinition();

    if (SaveRes.IsSuccess() ||
        SaveRes.Outcome != EEditorAuthoringOutcome::StaleFileState ||
        SaveRes.ErrorCode != "stale_file_state")
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    std::filesystem::remove_all(TempDir);
    return true;
}

bool TestAdapterStructuredDiagnosticsLossless()
{
    // CED-08: verify FGV2EditorDiagnostic preserves all fields from FDiagnostic without regex
    GV2ContentCore::FDiagnostic SourceDiag;
    SourceDiag.Severity = GV2ContentCore::EDiagnosticSeverity::Warning;
    SourceDiag.Code = "core:diagnostic.schema.missing_optional";
    SourceDiag.Message = "Field 'description' is missing";
    SourceDiag.PackageId = "core";
    SourceDiag.RelativeSource = "definitions/items.json5";
    GV2ContentCore::FSourceSpan Span;
    Span.StartLine = 42;
    Span.StartColumn = 15;
    Span.EndLine = 42;
    Span.EndColumn = 30;

    SourceDiag.Span = Span;
    SourceDiag.DefinitionId = "core:item.sword";
    SourceDiag.JsonPointer = "/definitions/0/data/description";

    FGV2EditorDiagnostic EditorDiag = FGV2EditorDiagnostic::FromDiagnostic(SourceDiag);

    if (EditorDiag.Severity != GV2ContentCore::EDiagnosticSeverity::Warning ||
        EditorDiag.Code != "core:diagnostic.schema.missing_optional" ||
        EditorDiag.Message != "Field 'description' is missing" ||
        EditorDiag.PackageId != "core" ||
        EditorDiag.RelativeSource != "definitions/items.json5" ||
        EditorDiag.Line != 42 ||
        EditorDiag.Column != 15 ||
        EditorDiag.StableId != "core:item.sword" ||
        EditorDiag.JsonPointer != "/definitions/0/data/description")
    {
        return false;
    }

    return true;
}

bool TestAuthoringIndexAndLocators()
{
    GV2ContentAuthoring::FAuthoringIndex Index;

    GV2ContentAuthoring::FAuthoringLocator BaseSword;
    BaseSword.PackageId = "core";
    BaseSword.RelativeSource = "definitions/items.json5";
    BaseSword.DefinitionId = "core:item.sword";
    BaseSword.DefinitionType = "item";
    BaseSword.LoadIndex = 0;
    Index.AddEntry(BaseSword);

    GV2ContentAuthoring::FAuthoringLocator ModSword;
    ModSword.PackageId = "mod_a";
    ModSword.RelativeSource = "definitions/items.json5";
    ModSword.DefinitionId = "core:item.sword";
    ModSword.DefinitionType = "item";
    ModSword.LoadIndex = 1;
    Index.AddEntry(ModSword);

    GV2ContentAuthoring::FAuthoringLocator BaseShield;
    BaseShield.PackageId = "core";
    BaseShield.RelativeSource = "definitions/items.json5";
    BaseShield.DefinitionId = "core:item.shield";
    BaseShield.DefinitionType = "item";
    BaseShield.LoadIndex = 0;
    Index.AddEntry(BaseShield);

    Index.Finalize();

    if (Index.NumEffective() != 2 || Index.NumTotal() != 3) return false;

    auto WinnerSword = Index.GetEffectiveWinner("core:item.sword");
    if (!WinnerSword.has_value() || WinnerSword->PackageId != "mod_a" || !WinnerSword->bIsWinner || WinnerSword->bIsShadowed)
    {
        return false;
    }

    auto SwordProviders = Index.GetEntriesForDefinition("core:item.sword");
    if (SwordProviders.size() != 2) return false;

    auto ShadowedSword = Index.FindLocator("core", "core:item.sword");
    if (!ShadowedSword.has_value() || ShadowedSword->PackageId != "core" || ShadowedSword->bIsWinner || !ShadowedSword->bIsShadowed)
    {
        return false;
    }

    auto EffectiveDefs = Index.GetEffectiveDefinitions();
    if (EffectiveDefs.size() != 2) return false;

    return true;
}

bool TestProviderAwareAdapterSelection()
{
    std::filesystem::path ContainerDir = CreateTempDir("gv2_adapter_override");
    std::filesystem::path CoreDir = ContainerDir / "core";
    std::filesystem::path ModDir = ContainerDir / "mod_a";

    // core package
    std::string CorePkgJson5 =
        "{\n"
        "  schema_version: 1,\n"
        "  package_id: 'core',\n"
        "  namespace: 'core',\n"
        "  version: '1.0.0',\n"
        "  dependencies: []\n"
        "}\n";
    WriteFile(CoreDir / "package.json5", CorePkgJson5);

    std::string SchemaJson5 =
        "{\n"
        "  schema_version: 1,\n"
        "  id: 'core:schema.definition.item.v1',\n"
        "  definition_type: 'item',\n"
        "  root: { kind: 'object', fields: {\n"
        "    name: { kind: 'string', required: true }\n"
        "  } },\n"
        "  semantic_validators: [],\n"
        "  extensions: {}\n"
        "}\n";
    WriteFile(CoreDir / "schemas/item.schema.json5", SchemaJson5);

    std::string CoreItems =
        "{\n"
        "  schema_version: 1,\n"
        "  type: 'item',\n"
        "  definitions: [\n"
        "    {\n"
        "      id: 'core:item.sword',\n"
        "      data: {\n"
        "        name: 'Iron Sword'\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n";
    WriteFile(CoreDir / "definitions/items.json5", CoreItems);

    // mod package overriding core:item.sword
    std::string ModPkgJson5 =
        "{\n"
        "  schema_version: 1,\n"
        "  package_id: 'mod_a',\n"
        "  namespace: 'mod_a',\n"
        "  version: '1.0.0',\n"
        "  dependencies: [\n"
        "    { package_id: 'core' }\n"
        "  ]\n"
        "}\n";
    WriteFile(ModDir / "package.json5", ModPkgJson5);

    std::string ModItems =
        "{\n"
        "  schema_version: 1,\n"
        "  type: 'item',\n"
        "  definitions: [\n"
        "    {\n"
        "      id: 'core:item.sword',\n"
        "      data: {\n"
        "        name: 'Legendary Modded Sword'\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n";
    WriteFile(ModDir / "definitions/items.json5", ModItems);

    FGV2EditorAdapter Adapter;
    std::vector<FGV2EditorDiagnostic> InitDiags;
    if (!Adapter.Initialize(ContainerDir, InitDiags))
    {
        return false;
    }

    auto EffectiveDefs = Adapter.ListDefinitions();
    if (EffectiveDefs.size() != 1) return false;
    if (EffectiveDefs[0].PackageId != "mod_a") return false;

    // Load by ID loads the effective winner (mod_a)
    std::vector<FGV2EditorDiagnostic> LoadDiags;
    auto LoadedOpt = Adapter.LoadDefinition("core:item.sword", LoadDiags);
    if (!LoadedOpt.has_value() || LoadedOpt->PackageId != "mod_a")
    {
        return false;
    }
    const auto* NameVal = LoadedOpt->CanonicalData.FindField("name");
    if (NameVal == nullptr || !NameVal->IsString() || NameVal->AsString() != "Legendary Modded Sword")
    {
        return false;
    }

    // Inspect providers
    auto Providers = Adapter.GetProvidersForDefinition("core:item.sword");
    if (Providers.size() != 2) return false;

    GV2ContentAuthoring::FAuthoringLocator CoreLoc;
    for (const auto& P : Providers)
    {
        if (P.PackageId == "core") CoreLoc = P;
    }
    if (CoreLoc.PackageId != "core") return false;

    // Load explicit shadowed locator
    auto CoreLoadedOpt = Adapter.LoadDefinition(CoreLoc, LoadDiags);
    if (!CoreLoadedOpt.has_value() || CoreLoadedOpt->PackageId != "core")
    {
        return false;
    }
    const auto* CoreNameVal = CoreLoadedOpt->CanonicalData.FindField("name");
    if (CoreNameVal == nullptr || !CoreNameVal->IsString() || CoreNameVal->AsString() != "Iron Sword")
    {
        return false;
    }

    // Modify shadowed and save -> modifies core file without touching mod
    Adapter.SetCurrentFieldValue("/data/name", GV2ContentCore::FValue::MakeString("Steel Sword"));
    auto SaveResult = Adapter.SaveCurrentDefinition();
    if (!SaveResult.IsSuccess()) return false;

    std::string CoreContentAfter = ReadFile(CoreDir / "definitions/items.json5");
    std::string ModContentAfter = ReadFile(ModDir / "definitions/items.json5");
    if (CoreContentAfter.find("Steel Sword") == std::string::npos) return false;
    if (ModContentAfter.find("Legendary Modded Sword") == std::string::npos) return false;

    return true;
}

bool TestStableIdTreeHierarchyAndFiltering()
{
    // Test parsing Stable IDs into Namespace -> Kind -> PathSegments
    std::vector<std::string> TestIds = {
        "core:item.sword",
        "core:item.armor.plate",
        "textsystem:screen.location",
        "textsystem:resource.ui.missing_portrait"
    };

    std::map<std::string, std::map<std::string, std::vector<std::string>>> Hierarchy;
    for (const auto& FullId : TestIds)
    {
        auto ColonPos = FullId.find(':');
        std::string Ns = FullId.substr(0, ColonPos);
        std::string Rem = FullId.substr(ColonPos + 1);
        auto DotPos = Rem.find('.');
        std::string Kind = Rem.substr(0, DotPos);
        std::string Path = Rem.substr(DotPos + 1);
        Hierarchy[Ns][Kind].push_back(Path);
    }

    if (Hierarchy.size() != 2) return false; // "core", "textsystem"
    if (Hierarchy["core"].size() != 1) return false; // "item"
    if (Hierarchy["core"]["item"].size() != 2) return false; // "sword", "armor.plate"
    if (Hierarchy["textsystem"].size() != 2) return false; // "screen", "resource"

    return true;
}

} // namespace

#include "GV2ContentEditor/FieldAdapterRegistry.h"
#include "GV2ContentCore/ScalarValidation.h"

bool TestSemanticKindAndFieldAdapter()
{
    GV2ContentCore::FCompiledFieldSpec IntSpec;
    IntSpec.Kind = GV2ContentCore::EFieldKind::Scalar;
    GV2ContentCore::FScalarFieldSpec IntScalar;
    IntScalar.Kind = GV2ContentCore::EScalarFieldKind::Integer;
    IntSpec.Scalar = IntScalar;

    GV2ContentCore::FFieldUiMetadata SliderMeta;
    SliderMeta.WidgetHint = "slider";

    auto Desc = FGV2FieldAdapterRegistry::Get().DescribeField(IntSpec, &SliderMeta);
    if (Desc.ControlType != EFieldControlType::Slider ||
        Desc.SemanticKind != GV2ContentCore::EFieldKind::Scalar ||
        !Desc.ScalarKind.has_value() ||
        *Desc.ScalarKind != GV2ContentCore::EScalarFieldKind::Integer)
    {
        return false;
    }

    return true;
}

bool TestPropertyPresenceAndStructuralOperations()
{
    std::filesystem::path TempDir = CreateTempDir("gv2_adapter_presence");
    std::string InitialItems =
        "{\n"
        "  schema_version: 1,\n"
        "  type: 'item',\n"
        "  definitions: [\n"
        "    {\n"
        "      id: 'core:item.presence_test',\n"
        "      data: {\n"
        "        name: 'Presence Test Item'\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n";

    SetupTestGameData(TempDir, InitialItems);

    FGV2EditorAdapter Adapter;
    std::vector<FGV2EditorDiagnostic> Diags;
    Adapter.Initialize(TempDir, Diags);
    auto Loaded = Adapter.LoadDefinition("core:item.presence_test", Diags);
    if (!Loaded.has_value())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Name is explicit
    auto NamePresence = Adapter.GetPropertyPresence("/data/name", nullptr, true);
    if (NamePresence != EPropertyPresence::Explicit)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Weight has default 1.0 -> should be ImplicitDefault
    GV2ContentCore::FCompiledFieldSpec WeightSpec;
    WeightSpec.Kind = GV2ContentCore::EFieldKind::Scalar;
    WeightSpec.DefaultValue = GV2ContentCore::FValue(1.0);
    auto WeightPresence = Adapter.GetPropertyPresence("/data/weight", &WeightSpec, false);
    if (WeightPresence != EPropertyPresence::ImplicitDefault)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Absent optional property
    auto AbsentPresence = Adapter.GetPropertyPresence("/data/nonexistent", nullptr, false);
    if (AbsentPresence != EPropertyPresence::Absent)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Required missing property
    auto MissingReqPresence = Adapter.GetPropertyPresence("/data/missing_req", nullptr, true);
    if (MissingReqPresence != EPropertyPresence::RequiredMissing)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Materialize optional property
    Adapter.AddCurrentOptionalProperty("/data/weight");
    if (Adapter.GetPropertyPresence("/data/weight", &WeightSpec, false) != EPropertyPresence::Explicit)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Save and verify atomic disk update
    auto SaveRes = Adapter.SaveCurrentDefinition();
    if (!SaveRes.IsSuccess())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    std::string DiskContent = ReadFile(TempDir / "core/definitions/items.json5");
    if (DiskContent.find("weight") == std::string::npos)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Remove property
    Adapter.RemoveCurrentProperty("/data/weight");
    auto SaveRes2 = Adapter.SaveCurrentDefinition();
    if (!SaveRes2.IsSuccess())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    std::filesystem::remove_all(TempDir);
    return true;
}

bool TestArrayStructuralOperations()
{
    std::filesystem::path TempDir = CreateTempDir("gv2_adapter_arrays");
    std::string InitialItems =
        "{\n"
        "  schema_version: 1,\n"
        "  type: 'item',\n"
        "  definitions: [\n"
        "    {\n"
        "      id: 'core:item.array_test',\n"
        "      data: {\n"
        "        name: 'Array Test Item'\n"
        "      },\n"
        "      tags: ['a', 'b', 'c']\n"
        "    }\n"
        "  ]\n"
        "}\n";

    SetupTestGameData(TempDir, InitialItems);

    FGV2EditorAdapter Adapter;
    std::vector<FGV2EditorDiagnostic> Diags;
    Adapter.Initialize(TempDir, Diags);
    Adapter.LoadDefinition("core:item.array_test", Diags);

    // Insert element at index 1
    Adapter.InsertCurrentArrayElement("/tags", 1, GV2ContentCore::FValue("inserted"));
    auto TagsVal = Adapter.GetCurrentFieldValue("/tags");
    if (!TagsVal.has_value() || !TagsVal->IsArray() || TagsVal->AsArray().size() != 4 || TagsVal->AsArray()[1].AsString() != "inserted")
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Move element from 1 to 3
    Adapter.MoveCurrentArrayElement("/tags", 1, 3);
    TagsVal = Adapter.GetCurrentFieldValue("/tags");
    if (!TagsVal.has_value() || !TagsVal->IsArray() || TagsVal->AsArray()[3].AsString() != "inserted")
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Remove element at index 3
    Adapter.RemoveCurrentArrayElement("/tags", 3);
    TagsVal = Adapter.GetCurrentFieldValue("/tags");
    if (!TagsVal.has_value() || !TagsVal->IsArray() || TagsVal->AsArray().size() != 3)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    auto SaveRes = Adapter.SaveCurrentDefinition();
    if (!SaveRes.IsSuccess())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    std::filesystem::remove_all(TempDir);
    return true;
}

bool TestTypedReferencesAndLiveOverlay()
{
    std::filesystem::path TempDir = CreateTempDir("gv2_typed_refs");
    std::filesystem::path CoreDir = TempDir / "core";

    std::string PackageJson5 =
        "{\n"
        "  schema_version: 1,\n"
        "  package_id: 'core',\n"
        "  namespace: 'core',\n"
        "  version: '1.0.0',\n"
        "  dependencies: []\n"
        "}\n";
    WriteFile(CoreDir / "package.json5", PackageJson5);

    std::string ItemSchema =
        "{\n"
        "  schema_version: 1,\n"
        "  id: 'core:schema.definition.item.v1',\n"
        "  definition_type: 'item',\n"
        "  root: { kind: 'object', fields: {\n"
        "    name: { kind: 'string', required: true },\n"
        "    description: { kind: 'string', required: false, default: '' },\n"
        "    ref_item: { kind: 'ref', target_kind: 'item', required: false },\n"
        "    text_title: { kind: 'text_id', required: false },\n"
        "    icon_ref: { kind: 'resource_ref', resource_class: 'icon', required: false }\n"
        "  } },\n"
        "  semantic_validators: [],\n"
        "  extensions: {}\n"
        "}\n";
    WriteFile(CoreDir / "schemas/item.schema.json5", ItemSchema);

    std::string ResourceSchema =
        "{\n"
        "  schema_version: 1,\n"
        "  id: 'core:schema.definition.resource.v1',\n"
        "  definition_type: 'resource',\n"
        "  root: { kind: 'object', fields: {\n"
        "    resource_class: { kind: 'string', required: true },\n"
        "    path: { kind: 'string', required: true }\n"
        "  } },\n"
        "  semantic_validators: [],\n"
        "  extensions: {}\n"
        "}\n";
    WriteFile(CoreDir / "schemas/resource.schema.json5", ResourceSchema);

    std::string ItemsContent =
        "{\n"
        "  schema_version: 1,\n"
        "  type: 'item',\n"
        "  definitions: [\n"
        "    {\n"
        "      id: 'core:item.target_sword',\n"
        "      data: { name: 'Target Sword', description: 'Just a sword' }\n"
        "    },\n"
        "    {\n"
        "      id: 'core:item.other_sword',\n"
        "      data: { name: 'Other Sword', description: 'Another sword' }\n"
        "    },\n"
        "    {\n"
        "      id: 'core:item.referencing_hero',\n"
        "      data: {\n"
        "        name: 'Hero',\n"
        "        description: 'core:item.fake_ref',\n"
        "        ref_item: 'core:item.target_sword',\n"
        "        text_title: 'core:text.sample.title',\n"
        "        icon_ref: 'core:resource.item_icon'\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n";
    WriteFile(CoreDir / "definitions/items.json5", ItemsContent);

    std::string ResourcesContent =
        "{\n"
        "  schema_version: 1,\n"
        "  type: 'resource',\n"
        "  definitions: [\n"
        "    {\n"
        "      id: 'core:resource.item_icon',\n"
        "      data: { resource_class: 'icon', path: 'textures/icon.png' }\n"
        "    }\n"
        "  ]\n"
        "}\n";
    WriteFile(CoreDir / "definitions/resources.json5", ResourcesContent);

    FGV2EditorAdapter Adapter;
    std::vector<FGV2EditorDiagnostic> Diags;
    if (!Adapter.Initialize(TempDir, Diags))
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // CEH-14: Test pickers without disk re-reading
    auto IconTargets = Adapter.GetCompatibleResourceTargets("icon");
    if (IconTargets.size() != 1 || IconTargets[0] != "core:resource.item_icon")
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    auto ItemTargets = Adapter.GetCompatibleReferenceTargets("item");
    if (ItemTargets.size() != 3)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // CEH-13: Negative false-positive test - description string 'core:item.fake_ref' is NOT a reference
    auto FakeIncoming = Adapter.GetIncomingReferences("core:item.fake_ref");
    if (!FakeIncoming.empty())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Incoming reference for true typed ref target
    auto TargetIncoming = Adapter.GetIncomingReferences("core:item.target_sword");
    if (TargetIncoming.size() != 1 || TargetIncoming[0].SourceDefinitionId != "core:item.referencing_hero")
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Load referencing definition
    Adapter.LoadDefinition("core:item.referencing_hero", Diags);
    auto Outgoing = Adapter.GetOutgoingReferences();
    if (Outgoing.size() != 3)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // CEH-15: Dynamic in-memory overlay test
    // Change ref_item to 'core:item.other_sword' without saving to disk
    Adapter.SetCurrentFieldValue("/data/ref_item", GV2ContentCore::FValue("core:item.other_sword"));

    auto OutgoingAfterEdit = Adapter.GetOutgoingReferences();
    bool bHasOtherSword = false;
    for (const auto& Ref : OutgoingAfterEdit)
    {
        if (Ref.TargetDefinitionId == "core:item.other_sword") bHasOtherSword = true;
    }
    if (!bHasOtherSword)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Check that target_sword no longer has incoming reference in overlay
    auto TargetIncomingAfterEdit = Adapter.GetIncomingReferences("core:item.target_sword");
    if (!TargetIncomingAfterEdit.empty())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Check that other_sword now has incoming reference in overlay
    auto OtherIncomingAfterEdit = Adapter.GetIncomingReferences("core:item.other_sword");
    if (OtherIncomingAfterEdit.size() != 1 || OtherIncomingAfterEdit[0].SourceDefinitionId != "core:item.referencing_hero")
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Discard changes
    Adapter.DiscardCurrentChanges();
    auto TargetIncomingAfterDiscard = Adapter.GetIncomingReferences("core:item.target_sword");
    if (TargetIncomingAfterDiscard.size() != 1)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    std::filesystem::remove_all(TempDir);
    return true;
}

bool TestRenameImpactAndSchemaAwareRename()
{
    std::filesystem::path TempDir = CreateTempDir("gv2_rename_impact");
    std::filesystem::path CoreDir = TempDir / "core";

    std::string PackageJson5 =
        "{\n"
        "  schema_version: 1,\n"
        "  package_id: 'core',\n"
        "  namespace: 'core',\n"
        "  version: '1.0.0',\n"
        "  dependencies: []\n"
        "}\n";
    WriteFile(CoreDir / "package.json5", PackageJson5);

    std::string ItemSchema =
        "{\n"
        "  schema_version: 1,\n"
        "  id: 'core:schema.definition.item.v1',\n"
        "  definition_type: 'item',\n"
        "  root: { kind: 'object', fields: {\n"
        "    name: { kind: 'string', required: true },\n"
        "    description: { kind: 'string', required: false, default: '' },\n"
        "    ref_item: { kind: 'ref', target_kind: 'item', required: false }\n"
        "  } },\n"
        "  semantic_validators: [],\n"
        "  extensions: {}\n"
        "}\n";
    WriteFile(CoreDir / "schemas/item.schema.json5", ItemSchema);

    std::string ItemsContent =
        "{\n"
        "  schema_version: 1,\n"
        "  type: 'item',\n"
        "  definitions: [\n"
        "    {\n"
        "      id: 'core:item.original_sword',\n"
        "      data: { name: 'Original Sword', description: 'Original description' }\n"
        "    },\n"
        "    {\n"
        "      id: 'core:item.wielder',\n"
        "      data: {\n"
        "        name: 'Wielder',\n"
        "        description: 'core:item.original_sword',\n"
        "        ref_item: 'core:item.original_sword'\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n";
    WriteFile(CoreDir / "definitions/items.json5", ItemsContent);

    FGV2EditorAdapter Adapter;
    std::vector<FGV2EditorDiagnostic> Diags;
    if (!Adapter.Initialize(TempDir, Diags))
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // CEH-16: Rename Impact Report
    auto Impact = Adapter.CalculateRenameImpact("core:item.original_sword", "core:item.renamed_sword");
    if (!Impact.bCanRenameDirectly || Impact.bHasExternalReferences || Impact.TotalReplacements != 2 || Impact.OwnPackageReferences.size() != 1)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // CEH-17: Schema-Aware Rename
    auto RenameRes = Adapter.RenameDefinition("core:item.original_sword", "core:item.renamed_sword", true);
    if (!RenameRes.IsSuccess())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Verify rewritten file content
    std::string Rewritten = ReadFile(CoreDir / "definitions/items.json5");
    // Definition ID must be updated
    if (Rewritten.find("'core:item.renamed_sword'") == std::string::npos &&
        Rewritten.find("\"core:item.renamed_sword\"") == std::string::npos)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Scalar description string must NOT be changed!
    if (Rewritten.find("description: 'core:item.original_sword'") == std::string::npos &&
        Rewritten.find("description: \"core:item.original_sword\"") == std::string::npos)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Redirect created in package.json5
    std::string PkgJson = ReadFile(CoreDir / "package.json5");
    if (PkgJson.find("core:item.original_sword") == std::string::npos ||
        PkgJson.find("core:item.renamed_sword") == std::string::npos)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    std::filesystem::remove_all(TempDir);
    return true;
}

bool TestTransactionJournalRecovery()
{
    std::filesystem::path TempDir = CreateTempDir("gv2_journal_recovery");
    std::filesystem::path CoreDir = TempDir / "core";

    std::string PackageJson5 =
        "{\n"
        "  schema_version: 1,\n"
        "  package_id: 'core',\n"
        "  namespace: 'core',\n"
        "  version: '1.0.0',\n"
        "  dependencies: []\n"
        "}\n";
    WriteFile(CoreDir / "package.json5", PackageJson5);

    std::string OrigContent = "{\n  schema_version: 1,\n  type: 'item',\n  definitions: []\n}\n";
    WriteFile(CoreDir / "definitions/items.json5", OrigContent);

    // Create journal simulating crash mid-transaction
    std::string JournalContent =
        "{\n"
        "  transaction_id: 'crash_test',\n"
        "  entries: [\n"
        "    {\n"
        "      relative_path: 'definitions/items.json5',\n"
        "      original_content: '{\\n  schema_version: 1,\\n  type: \\'item\\',\\n  definitions: []\\n}\\n'\n"
        "    }\n"
        "  ]\n"
        "}\n";
    WriteFile(CoreDir / ".gv2_authoring_journal.json5", JournalContent);

    // Corrupt the file on disk as if partially written
    WriteFile(CoreDir / "definitions/items.json5", "{ corrupted partial content");

    auto RecovRes = GV2ContentAuthoring::FAuthoringService::RecoverPendingJournal(CoreDir);
    if (RecovRes.Status != GV2ContentAuthoring::EAuthoringStatus::Success)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Verify file restored
    std::string Restored = ReadFile(CoreDir / "definitions/items.json5");
    if (Restored != OrigContent)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Verify journal file cleaned up
    if (std::filesystem::exists(CoreDir / ".gv2_authoring_journal.json5"))
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    std::filesystem::remove_all(TempDir);
    return true;
}

std::string RunEditorAdapterConformance()
{
    if (!TestAdapterInitializationAndIndexing())
    {
        return "TestAdapterInitializationAndIndexing failed";
    }
    if (!TestAdapterLoadAndDirtyState())
    {
        return "TestAdapterLoadAndDirtyState failed";
    }
    if (!TestAdapterAtomicSave())
    {
        return "TestAdapterAtomicSave failed";
    }
    if (!TestAdapterStaleFileDetectionAndRejection())
    {
        return "TestAdapterStaleFileDetectionAndRejection failed";
    }
    if (!TestAdapterStructuredDiagnosticsLossless())
    {
        return "TestAdapterStructuredDiagnosticsLossless failed";
    }
    if (!TestAuthoringIndexAndLocators())
    {
        return "TestAuthoringIndexAndLocators failed";
    }
    if (!TestProviderAwareAdapterSelection())
    {
        return "TestProviderAwareAdapterSelection failed";
    }
    if (!TestStableIdTreeHierarchyAndFiltering())
    {
        return "TestStableIdTreeHierarchyAndFiltering failed";
    }
    if (!TestSemanticKindAndFieldAdapter())
    {
        return "TestSemanticKindAndFieldAdapter failed";
    }
    if (!TestPropertyPresenceAndStructuralOperations())
    {
        return "TestPropertyPresenceAndStructuralOperations failed";
    }
    if (!TestArrayStructuralOperations())
    {
        return "TestArrayStructuralOperations failed";
    }
    if (!TestTypedReferencesAndLiveOverlay())
    {
        return "TestTypedReferencesAndLiveOverlay failed";
    }
    if (!TestRenameImpactAndSchemaAwareRename())
    {
        return "TestRenameImpactAndSchemaAwareRename failed";
    }
    if (!TestTransactionJournalRecovery())
    {
        return "TestTransactionJournalRecovery failed";
    }

    std::string ReadSurfaceError = RunReadSurfaceConformance();
    if (!ReadSurfaceError.empty())
    {
        return ReadSurfaceError;
    }

    std::string WriteSurfaceError = RunWriteSurfaceConformance();
    if (!WriteSurfaceError.empty())
    {
        return WriteSurfaceError;
    }

    std::string FourKindsError = RunFourKindsConformance();
    if (!FourKindsError.empty())
    {
        return FourKindsError;
    }

    return "";
}

} // namespace GV2ContentEditor::Testing
