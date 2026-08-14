#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/FieldValidation.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/RepositoryBuilder.h"
#include "Misc/AutomationTest.h"

#include <map>

namespace
{
class FContainerTestSourceProvider final : public GV2ContentCore::IContentSourceProvider
{
public:
    std::map<std::string, std::string> Sources;
    std::optional<std::string> ReadSource(
        const std::string_view PackageId, const std::string_view RelativeSource) const override
    {
        const auto Found = Sources.find(std::string(PackageId) + "/" + std::string(RelativeSource));
        return Found == Sources.end() ? std::nullopt : std::optional<std::string>(Found->second);
    }
};

GV2ContentCore::FCompiledFieldSpecPtr CompileSpec(
    const std::string_view Source,
    std::vector<GV2ContentCore::FDiagnostic>& Diagnostics)
{
    using namespace GV2ContentCore;
    auto Document = ParseJson5Document(Source, FParseLimits{}, Diagnostics);
    if (!Document.has_value()) return nullptr;
    FValidationDiagnosticContext Context;
    Context.SchemaId = "core:schema.test.container.v1";
    return CompileFieldSpec(Document->GetRootValue(), &*Document, "", Context, Diagnostics);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreContainerValidationTest,
    "GV2.Runtime.ContentCore.ContainerValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreContainerValidationTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;
    FValidationDiagnosticContext Context;
    Context.PackageId = "core";
    Context.RelativeSource = "definitions/containers.json5";
    Context.DefinitionId = "core:container.test";
    Context.SchemaId = "core:schema.test.container.v1";

    std::vector<FDiagnostic> Diagnostics;
    const FCompiledFieldSpecPtr ArraySpec = CompileSpec(
        "{ kind: 'array', min_items: 2, max_items: 3, unique: true, items: { kind: 'int64', min: 0 } }",
        Diagnostics);
    TestTrue(TEXT("array compiles"), ArraySpec != nullptr && Diagnostics.empty());
    const FValue Ordered = FValue::MakeArray({ FValue(2), FValue(1) });
    FValue Materialized;
    TestTrue(TEXT("ordered array validates"), ArraySpec != nullptr
        && ValidateFieldValue(Ordered, *ArraySpec, Materialized, nullptr, "/data", Context, Diagnostics));
    TestEqual(TEXT("array order remains unchanged"), Ordered.AsArray()[0].AsInteger(), static_cast<std::int64_t>(2));
    Diagnostics.clear();
    TestFalse(TEXT("duplicate array item fails"), ValidateFieldValue(
        FValue::MakeArray({ FValue(1), FValue(1) }), *ArraySpec, Materialized, nullptr, "/data", Context, Diagnostics));
    TestTrue(TEXT("duplicate item code is stable"), !Diagnostics.empty()
        && Diagnostics[0].Code == "core:diagnostic.schema.value.duplicate_array_item");

    Diagnostics.clear();
    const FCompiledFieldSpecPtr ObjectArraySpec = CompileSpec(
        "{ kind: 'array', unique: true, items: { kind: 'object', fields: {"
        "a: { kind: 'int64' }, b: { kind: 'int64' } } } }",
        Diagnostics);
    TestFalse(TEXT("unique object comparison ignores source field order"), ValidateFieldValue(
        FValue::MakeArray({
            FValue::MakeObject({ { "a", FValue(1) }, { "b", FValue(2) } }),
            FValue::MakeObject({ { "b", FValue(2) }, { "a", FValue(1) } }),
        }), *ObjectArraySpec, Materialized, nullptr, "/data", Context, Diagnostics));
    TestTrue(TEXT("permuted duplicate object has stable code"), !Diagnostics.empty()
        && Diagnostics[0].Code == "core:diagnostic.schema.value.duplicate_array_item");

    Diagnostics.clear();
    const FCompiledFieldSpecPtr MapSpec = CompileSpec(
        "{ kind: 'map', min_entries: 1, keys: { kind: 'string', pattern: '^[a-z]+$' }, values: { kind: 'bool' } }",
        Diagnostics);
    auto MapDocument = ParseJson5Document("{ valid: true, Bad: false }", FParseLimits{}, Diagnostics);
    TestTrue(TEXT("map compiles and value parses"), MapSpec != nullptr && MapDocument.has_value());
    Diagnostics.clear();
    TestFalse(TEXT("map key constraint fails"), ValidateFieldValue(
        MapDocument->GetRootValue(), *MapSpec, Materialized, &*MapDocument, "", Context, Diagnostics));
    TestTrue(TEXT("map key diagnostic uses key span"), !Diagnostics.empty()
        && Diagnostics[0].JsonPointer == std::optional<std::string>("/Bad")
        && Diagnostics[0].Span == MapDocument->FindLocation("/Bad")->KeySpan);

    Diagnostics.clear();
    const FCompiledFieldSpecPtr ObjectSpec = CompileSpec(
        "{ kind: 'object', fields: { name: { kind: 'string', required: true }, values: { kind: 'array', items: { kind: 'int64' } } } }",
        Diagnostics);
    auto ObjectDocument = ParseJson5Document("{ name: 'test', extra: 1 }", FParseLimits{}, Diagnostics);
    Diagnostics.clear();
    TestFalse(TEXT("closed object rejects unknown field"), ValidateFieldValue(
        ObjectDocument->GetRootValue(), *ObjectSpec, Materialized, &*ObjectDocument, "", Context, Diagnostics));
    TestTrue(TEXT("unknown field code and key span are stable"), !Diagnostics.empty()
        && Diagnostics[0].Code == "core:diagnostic.schema.value.unknown_field"
        && Diagnostics[0].Span == ObjectDocument->FindLocation("/extra")->KeySpan);
    Diagnostics.clear();
    TestFalse(TEXT("required field absence is rejected"), ValidateFieldValue(
        FValue::MakeObject(), *ObjectSpec, Materialized, nullptr, "/data", Context, Diagnostics));
    TestTrue(TEXT("required field absence has stable code"), !Diagnostics.empty()
        && Diagnostics[0].Code == "core:diagnostic.schema.value.missing_required_field");

    Diagnostics.clear();
    const FCompiledFieldSpecPtr UnionSpec = CompileSpec(
        "{ kind: 'union', discriminator: 'kind', variants: {"
        "heal: { kind: 'object', fields: { kind: { kind: 'enum', values: ['heal'] }, amount: { kind: 'int64', min: 1 } } },"
        "wait: { kind: 'object', fields: { kind: { kind: 'enum', values: ['wait'] } } } } }",
        Diagnostics);
    TestTrue(TEXT("union compiles"), UnionSpec != nullptr && Diagnostics.empty());
    TestTrue(TEXT("declared union variant validates"), ValidateFieldValue(
        FValue::MakeObject({ { "kind", FValue("heal") }, { "amount", FValue(3) } }),
        *UnionSpec, Materialized, nullptr, "/data", Context, Diagnostics));
    Diagnostics.clear();
    TestFalse(TEXT("unknown union variant fails"), ValidateFieldValue(
        FValue::MakeObject({ { "kind", FValue("other") } }),
        *UnionSpec, Materialized, nullptr, "/data", Context, Diagnostics));
    TestTrue(TEXT("invalid variant code is stable"), !Diagnostics.empty()
        && Diagnostics[0].Code == "core:diagnostic.schema.value.invalid_union_variant");

    FContainerTestSourceProvider Provider;
    Provider.Sources.emplace("core/schemas/item.json5",
        "{ id: 'core:schema.item.v1', definition_type: 'item', schema_version: 1, "
        "root: { kind: 'object', fields: { price: { kind: 'int64', min: 0 } } }, "
        "semantic_validators: [], extensions: {} }");
    Provider.Sources.emplace("core/definitions/items.json5",
        "{ schema_version: 1, type: 'item', definitions: ["
        "{ id: 'core:item.invalid', data: { price: 1, extra: true } }] }");
    FBuildOptions Options;
    Options.SourceProvider = &Provider;
    const FBuildResult BuildResult = BuildRepository(
        { FPackageDescriptor("core", "core", 0, { "definitions/items.json5" },
            { FSchemaBinding("item", 1, "core:schema.item.v1", "schemas/item.json5") }) }, Options);
    TestTrue(TEXT("BuildRepository validates containers"), BuildResult.IsFailure());
    if (BuildResult.IsFailure())
    {
        const FDiagnostic& Diagnostic = BuildResult.GetDiagnostics()[0];
        TestEqual(TEXT("integrated container code"), Diagnostic.Code,
            std::string("core:diagnostic.schema.value.unknown_field"));
        TestTrue(TEXT("integrated nested pointer"), Diagnostic.JsonPointer
            == std::optional<std::string>("/definitions/0/data/extra"));
        TestTrue(TEXT("integrated key span"), Diagnostic.Span.has_value());
    }
    return true;
}

#endif
