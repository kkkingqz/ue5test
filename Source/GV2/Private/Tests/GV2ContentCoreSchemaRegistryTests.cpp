#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/RepositoryBuilder.h"
#include "GV2ContentCore/SchemaRegistry.h"
#include "Misc/AutomationTest.h"

#include <map>

namespace
{
class FSchemaTestSourceProvider final : public GV2ContentCore::IContentSourceProvider
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
    FGV2ContentCoreSchemaRegistryTest,
    "GV2.Runtime.ContentCore.SchemaRegistry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreSchemaRegistryTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;

    const std::string SchemaSource =
        "{ id: 'core:schema.definition.item.v1', definition_type: 'item', schema_version: 1, "
        "root: { kind: 'object', fields: {} }, "
        "semantic_validators: ['core:validator.item.semantics'], extensions: {} }";
    std::vector<FDiagnostic> Diagnostics;
    auto Document = ParseJson5Document(SchemaSource, FParseLimits{}, Diagnostics);
    TestTrue(TEXT("Schema JSON5 parses"), Document.has_value() && Diagnostics.empty());
    if (!Document.has_value())
    {
        return false;
    }

    const FSchemaBinding Binding(
        "item", 1, "core:schema.definition.item.v1", "schemas/arbitrary_name.json5");
    auto Resource = ParseSchemaResource(
        *Document, Binding, "core", 0, Binding.GetRelativePath(), Diagnostics);
    TestTrue(TEXT("Schema resource envelope parses independently of filename"), Resource.has_value());
    TestTrue(TEXT("Valid schema resource has no diagnostics"), Diagnostics.empty());

    FSchemaRegistry Registry;
    if (Resource.has_value())
    {
        TestTrue(TEXT("Exact schema registration succeeds"), Registry.Register(std::move(*Resource), Diagnostics));
    }
    TestEqual(TEXT("Registry contains one schema"), Registry.Num(), static_cast<size_t>(1));
    TestTrue(TEXT("Exact key resolves"), Registry.Find("item", 1) != nullptr);
    TestTrue(TEXT("Version fallback is forbidden"), Registry.Find("item", 2) == nullptr);

    Diagnostics.clear();
    auto DuplicateResource = ParseSchemaResource(
        *Document, Binding, "test_mod", 1, Binding.GetRelativePath(), Diagnostics);
    TestTrue(TEXT("Second schema resource parses before registry duplicate check"), DuplicateResource.has_value());
    if (DuplicateResource.has_value())
    {
        TestFalse(TEXT("Duplicate exact registration is rejected"), Registry.Register(std::move(*DuplicateResource), Diagnostics));
    }
    TestTrue(
        TEXT("Duplicate registration has stable diagnostic"),
        !Diagnostics.empty() && Diagnostics.back().Code == "core:diagnostic.schema.binding.duplicate");

    Diagnostics.clear();
    const FSchemaBinding MismatchedBinding(
        "screen", 1, "core:schema.definition.item.v1", "schemas/arbitrary_name.json5");
    auto Mismatched = ParseSchemaResource(
        *Document, MismatchedBinding, "core", 0, MismatchedBinding.GetRelativePath(), Diagnostics);
    TestFalse(TEXT("Descriptor/resource identity mismatch is rejected"), Mismatched.has_value());
    TestTrue(
        TEXT("Mismatch has stable diagnostic"),
        !Diagnostics.empty()
            && Diagnostics[0].Code == "core:diagnostic.schema.binding.resource_mismatch");

    FSchemaTestSourceProvider MissingProvider;
    MissingProvider.Sources.emplace(
        "core/definitions/items.json5",
        "{ schema_version: 1, type: 'item', definitions: [] }");
    FBuildOptions MissingOptions;
    MissingOptions.SourceProvider = &MissingProvider;
    const FBuildResult MissingResult = BuildRepository(
        { FPackageDescriptor("core", "core", 0, { "definitions/items.json5" }) },
        MissingOptions);
    TestTrue(TEXT("Definition without exact binding is rejected"), MissingResult.IsFailure());
    if (MissingResult.IsFailure())
    {
        TestEqual(
            TEXT("Missing binding code"),
            MissingResult.GetDiagnostics()[0].Code,
            std::string("core:diagnostic.schema.binding.missing"));
        TestTrue(
            TEXT("Missing binding points to schema_version"),
            MissingResult.GetDiagnostics()[0].JsonPointer == std::optional<std::string>("/schema_version"));
    }

    FSchemaTestSourceProvider ConflictProvider;
    ConflictProvider.Sources.emplace(
        "core/schemas/item_v1.json5",
        "{ id: 'core:schema.definition.item.v1', definition_type: 'item', schema_version: 1, "
        "root: { kind: 'object', fields: {} }, semantic_validators: [], extensions: {} }");
    ConflictProvider.Sources.emplace(
        "test_mod/schemas/foreign_name.json5",
        "{ id: 'test_mod:schema.definition.item.v1', definition_type: 'item', schema_version: 1, "
        "root: { kind: 'object', fields: {} }, semantic_validators: [], extensions: {} }");
    FBuildOptions ConflictOptions;
    ConflictOptions.SourceProvider = &ConflictProvider;
    const FBuildResult ConflictResult = BuildRepository(
        {
            FPackageDescriptor(
                "core", "core", 0, {},
                { FSchemaBinding("item", 1, "core:schema.definition.item.v1", "schemas/item_v1.json5") }),
            FPackageDescriptor(
                "test_mod", "test_mod", 1, {},
                { FSchemaBinding("item", 1, "test_mod:schema.definition.item.v1", "schemas/foreign_name.json5") }),
        },
        ConflictOptions);
    TestTrue(TEXT("Conflicting exact bindings are rejected"), ConflictResult.IsFailure());
    if (ConflictResult.IsFailure())
    {
        TestEqual(
            TEXT("Conflict binding code"),
            ConflictResult.GetDiagnostics()[0].Code,
            std::string("core:diagnostic.schema.binding.conflict"));
        TestTrue(TEXT("Conflict retains related schema span"), ConflictResult.GetDiagnostics()[0].RelatedSpan.has_value());
    }

    return true;
}

#endif
