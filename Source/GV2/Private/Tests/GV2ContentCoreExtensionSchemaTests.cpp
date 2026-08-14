#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/ExtensionSchema.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/RepositoryBuilder.h"
#include "Misc/AutomationTest.h"

#include <map>

namespace
{
class FExtensionSourceProvider final : public GV2ContentCore::IContentSourceProvider
{
public:
    std::map<std::string, std::string> Sources;

    std::optional<std::string> ReadSource(
        const std::string_view PackageId,
        const std::string_view RelativeSource) const override
    {
        const auto Found = Sources.find(std::string(PackageId) + "/" + std::string(RelativeSource));
        return Found == Sources.end() ? std::nullopt : std::optional<std::string>(Found->second);
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreExtensionSchemaTest,
    "GV2.Runtime.ContentCore.ExtensionSchema",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreExtensionSchemaTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;

    const FExtensionSchemaBinding Binding(
        "item", 1, "definition_entry", "weather_mod",
        "weather_mod:schema.extension.item.entry.v1",
        "schemas/item_weather_entry_v1.schema.json5");
    const std::string ExtensionSchemaSource =
        "{ id: 'weather_mod:schema.extension.item.entry.v1', definition_type: 'item', schema_version: 1, "
        "extension_site: 'definition_entry', extension_namespace: 'weather_mod', "
        "root: { kind: 'object', fields: { "
        "wet_grip_multiplier: { kind: 'number', min: 0.0, max: 1.0, default: 0.5 }, "
        "linked_item: { kind: 'ref', target_kind: 'item' } } } }";
    std::vector<FDiagnostic> Diagnostics;
    auto SchemaDocument = ParseJson5Document(ExtensionSchemaSource, FParseLimits{}, Diagnostics);
    TestTrue(TEXT("extension schema JSON5 parses"), SchemaDocument.has_value());
    if (!SchemaDocument.has_value()) return false;
    auto Resource = ParseExtensionSchemaResource(
        *SchemaDocument, Binding, "weather_mod", 1, Binding.GetRelativePath(), Diagnostics);
    TestTrue(TEXT("exact extension schema resource parses"), Resource.has_value() && Diagnostics.empty());
    if (!Resource.has_value()) return false;

    FExtensionSchemaRegistry Registry;
    TestTrue(TEXT("extension schema registers"), Registry.Register(std::move(*Resource), Diagnostics));
    TestTrue(TEXT("exact extension site lookup succeeds"),
        Registry.Find("item", 1, EExtensionSite::DefinitionEntry, "weather_mod") != nullptr);
    TestTrue(TEXT("site fallback is forbidden"),
        Registry.Find("item", 1, EExtensionSite::DefinitionFile, "weather_mod") == nullptr);

    FValidationDiagnosticContext Context;
    Context.PackageId = "weather_mod";
    Context.PackageLoadIndex = 1;
    Context.RelativeSource = "definitions/items.json5";
    Context.DefinitionId = "weather_mod:item.rain_boots";
    FValue MaterializedExtensions;
    TestTrue(TEXT("registered owned extension validates"), ValidateExtensionBlocks(
        FValue::MakeObject({
            { "weather_mod", FValue::MakeObject({ { "wet_grip_multiplier", FValue(0.8) } }) }
        }), MaterializedExtensions,
        Registry, "item", 1, EExtensionSite::DefinitionEntry, "weather_mod",
        nullptr, "/definitions/0/extensions", Context, Diagnostics));
    TestTrue(TEXT("present extension value is preserved"),
        MaterializedExtensions.FindField("weather_mod") != nullptr
        && MaterializedExtensions.FindField("weather_mod")->FindField("wet_grip_multiplier") != nullptr
        && MaterializedExtensions.FindField("weather_mod")->FindField("wet_grip_multiplier")->AsNumber() == 0.8);

    Diagnostics.clear();
    TestTrue(TEXT("extension defaults are materialized"), ValidateExtensionBlocks(
        FValue::MakeObject({ { "weather_mod", FValue::MakeObject() } }),
        MaterializedExtensions,
        Registry, "item", 1, EExtensionSite::DefinitionEntry, "weather_mod",
        nullptr, "/definitions/0/extensions", Context, Diagnostics));
    TestTrue(TEXT("materialized extension contains declared default"),
        MaterializedExtensions.FindField("weather_mod") != nullptr
        && MaterializedExtensions.FindField("weather_mod")->FindField("wet_grip_multiplier") != nullptr
        && MaterializedExtensions.FindField("weather_mod")->FindField("wet_grip_multiplier")->AsNumber() == 0.5);

    Diagnostics.clear();
    TestFalse(TEXT("foreign extension namespace is rejected"), ValidateExtensionBlocks(
        FValue::MakeObject({ { "core", FValue::MakeObject() } }),
        MaterializedExtensions,
        Registry, "item", 1, EExtensionSite::DefinitionEntry, "weather_mod",
        nullptr, "/definitions/0/extensions", Context, Diagnostics));
    TestTrue(TEXT("foreign namespace code is stable"), !Diagnostics.empty()
        && Diagnostics[0].Code == "core:diagnostic.extension.block.foreign_namespace");

    Diagnostics.clear();
    TestFalse(TEXT("unregistered extension site is rejected"), ValidateExtensionBlocks(
        FValue::MakeObject({ { "weather_mod", FValue::MakeObject() } }),
        MaterializedExtensions,
        Registry, "item", 1, EExtensionSite::DefinitionFile, "weather_mod",
        nullptr, "/extensions", Context, Diagnostics));
    TestTrue(TEXT("unregistered site code is stable"), !Diagnostics.empty()
        && Diagnostics[0].Code == "core:diagnostic.extension.block.unregistered_site");

    Diagnostics.clear();
    TestFalse(TEXT("extension block is a closed typed DTO"), ValidateExtensionBlocks(
        FValue::MakeObject({
            { "weather_mod", FValue::MakeObject({ { "unknown", FValue(true) } }) }
        }), MaterializedExtensions,
        Registry, "item", 1, EExtensionSite::DefinitionEntry, "weather_mod",
        nullptr, "/definitions/0/extensions", Context, Diagnostics));
    TestTrue(TEXT("block validation uses common field validator"), !Diagnostics.empty()
        && Diagnostics[0].Code == "core:diagnostic.schema.value.unknown_field"
        && Diagnostics[0].SchemaId == std::optional<std::string>(Binding.GetSchemaId()));

    Diagnostics.clear();
    const FExtensionSchemaBinding MismatchBinding(
        "item", 1, "definition_file", "weather_mod",
        Binding.GetSchemaId(), Binding.GetRelativePath());
    TestFalse(TEXT("descriptor/resource site mismatch is rejected"), ParseExtensionSchemaResource(
        *SchemaDocument, MismatchBinding, "weather_mod", 1,
        MismatchBinding.GetRelativePath(), Diagnostics).has_value());
    TestTrue(TEXT("resource mismatch code is stable"), !Diagnostics.empty()
        && Diagnostics.back().Code == "core:diagnostic.extension.schema.resource_mismatch");

    FExtensionSourceProvider Provider;
    Provider.Sources.emplace("core/schemas/item.json5",
        "{ id: 'core:schema.item.v1', definition_type: 'item', schema_version: 1, "
        "root: { kind: 'object', fields: {} }, semantic_validators: [], extensions: {} }");
    Provider.Sources.emplace("weather_mod/schemas/item_weather_entry_v1.schema.json5", ExtensionSchemaSource);
    Provider.Sources.emplace("weather_mod/definitions/items.json5",
        "{ schema_version: 1, type: 'item', definitions: ["
        "{ id: 'weather_mod:item.rain_boots', data: {}, extensions: {"
        "weather_mod: { wet_grip_multiplier: 0.8 } } }] }");
    FBuildOptions Options;
    Options.SourceProvider = &Provider;
    const std::vector<FPackageDescriptor> Packages{
        FPackageDescriptor("core", "core", 0, {},
            { FSchemaBinding("item", 1, "core:schema.item.v1", "schemas/item.json5") }),
        FPackageDescriptor("weather_mod", "weather_mod", 1,
            { "definitions/items.json5" }, {}, { Binding }),
    };
    const FBuildResult ValidBuild = BuildRepository(Packages, Options);
    TestTrue(TEXT("BuildRepository resolves registered owned extension"), ValidBuild.IsSuccess());
    if (ValidBuild.IsSuccess())
    {
        TestTrue(TEXT("resolved extension artifact has M4 stage"),
            ValidBuild.GetCandidate().GetStage() == ECandidateStage::RepositoryResolved);
    }

    Provider.Sources["weather_mod/definitions/items.json5"] =
        "{ schema_version: 1, type: 'item', definitions: ["
        "{ id: 'weather_mod:item.rain_boots', data: {}, extensions: {"
        "weather_mod: { linked_item: 'weather_mod:item.missing' } } }] }";
    const FBuildResult MissingExtensionReference = BuildRepository(Packages, Options);
    TestTrue(TEXT("missing reference in a materialized extension blocks publication"),
        MissingExtensionReference.IsFailure());
    if (MissingExtensionReference.IsFailure())
    {
        TestEqual(TEXT("extension reference uses the common missing-target diagnostic"),
            MissingExtensionReference.GetDiagnostics()[0].Code,
            std::string("core:diagnostic.reference.target_missing"));
        TestTrue(TEXT("extension reference diagnostic retains extension schema identity"),
            MissingExtensionReference.GetDiagnostics()[0].SchemaId
                == std::optional<std::string>(Binding.GetSchemaId())
            && MissingExtensionReference.GetDiagnostics()[0].JsonPointer
                == std::optional<std::string>(
                    "/definitions/0/extensions/weather_mod/linked_item"));
    }

    Provider.Sources["weather_mod/definitions/items.json5"] =
        "{ schema_version: 1, type: 'item', definitions: ["
        "{ id: 'weather_mod:item.rain_boots', data: {}, extensions: {"
        "weather_mod: { linked_item: 'weather_mod:item.old_boots' } } }] }";
    const std::vector<FPackageDescriptor> RedirectPackages{
        Packages[0],
        FPackageDescriptor("weather_mod", "weather_mod", 1,
            { "definitions/items.json5" }, {}, { Binding },
            { FRedirectDescriptor(
                "weather_mod:item.old_boots", "weather_mod:item.rain_boots") }),
    };
    const FBuildResult RedirectedExtensionReference = BuildRepository(RedirectPackages, Options);
    TestTrue(TEXT("extension reference follows the common redirect pipeline"),
        RedirectedExtensionReference.IsSuccess());
    if (RedirectedExtensionReference.IsSuccess())
    {
        const FValue& Entry = RedirectedExtensionReference.GetCandidate()
            .GetRootValue().FindField("definitions")->AsArray()[0];
        TestEqual(TEXT("extension reference stores final canonical target"),
            Entry.FindField("extensions")->FindField("weather_mod")
                ->FindField("linked_item")->AsString(),
            std::string("weather_mod:item.rain_boots"));
    }

    Provider.Sources["weather_mod/definitions/items.json5"] =
        "{ schema_version: 1, type: 'item', definitions: ["
        "{ id: 'weather_mod:item.rain_boots', data: {}, extensions: {"
        "core: { wet_grip_multiplier: 0.8 } } }] }";
    const FBuildResult ForeignBuild = BuildRepository(Packages, Options);
    TestTrue(TEXT("BuildRepository rejects foreign extension block"), ForeignBuild.IsFailure());
    if (ForeignBuild.IsFailure())
    {
        TestEqual(TEXT("integrated foreign namespace code"), ForeignBuild.GetDiagnostics()[0].Code,
            std::string("core:diagnostic.extension.block.foreign_namespace"));
        TestTrue(TEXT("integrated extension diagnostic retains definition ID"),
            ForeignBuild.GetDiagnostics()[0].DefinitionId
                == std::optional<std::string>("weather_mod:item.rain_boots"));
    }

    FExtensionSourceProvider MissingTargetProvider;
    MissingTargetProvider.Sources.emplace(
        "weather_mod/schemas/item_weather_entry_v1.schema.json5", ExtensionSchemaSource);
    FBuildOptions MissingTargetOptions;
    MissingTargetOptions.SourceProvider = &MissingTargetProvider;
    const FBuildResult MissingTargetBuild = BuildRepository(
        {
            FPackageDescriptor("core", "core", 0),
            FPackageDescriptor("weather_mod", "weather_mod", 1, {}, {}, { Binding }),
        },
        MissingTargetOptions);
    TestTrue(TEXT("extension binding without exact definition schema is rejected"),
        MissingTargetBuild.IsFailure());
    if (MissingTargetBuild.IsFailure())
    {
        TestEqual(TEXT("missing target schema code"), MissingTargetBuild.GetDiagnostics()[0].Code,
            std::string("core:diagnostic.extension.schema.target_schema_missing"));
    }

    FExtensionSourceProvider CoreProvider;
    CoreProvider.Sources.emplace("core/schemas/item.json5",
        "{ id: 'core:schema.item.v1', definition_type: 'item', schema_version: 1, "
        "root: { kind: 'object', fields: {} }, semantic_validators: [], extensions: {} }");
    CoreProvider.Sources.emplace("core/schemas/item_entry_extension.json5",
        "{ id: 'core:schema.extension.item.entry.v1', definition_type: 'item', schema_version: 1, "
        "extension_site: 'definition_entry', extension_namespace: 'core', root: { kind: 'object', "
        "fields: { bonus: { kind: 'int64', default: 7 } } } }");
    CoreProvider.Sources.emplace("core/definitions/items.json5",
        "{ schema_version: 1, type: 'item', definitions: ["
        "{ id: 'core:item.with_extension_default', data: {}, extensions: { core: {} } }] }");
    FBuildOptions CoreOptions;
    CoreOptions.SourceProvider = &CoreProvider;
    const FBuildResult CoreBuild = BuildRepository({ FPackageDescriptor(
        "core", "core", 0, { "definitions/items.json5" },
        { FSchemaBinding("item", 1, "core:schema.item.v1", "schemas/item.json5") },
        { FExtensionSchemaBinding(
            "item", 1, "definition_entry", "core",
            "core:schema.extension.item.entry.v1", "schemas/item_entry_extension.json5") }) }, CoreOptions);
    TestTrue(TEXT("single-package extension build succeeds"), CoreBuild.IsSuccess());
    if (CoreBuild.IsSuccess())
    {
        const FValue& CandidateEntry =
            CoreBuild.GetCandidate().GetRootValue().FindField("definitions")->AsArray()[0];
        const FValue* CoreBlock = CandidateEntry.FindField("extensions")->FindField("core");
        TestTrue(TEXT("candidate stores materialized extension defaults"), CoreBlock != nullptr
            && CoreBlock->FindField("bonus") != nullptr
            && CoreBlock->FindField("bonus")->AsInteger() == 7);
    }

    return true;
}

#endif
