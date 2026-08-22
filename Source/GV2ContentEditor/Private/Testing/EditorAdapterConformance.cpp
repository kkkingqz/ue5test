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
