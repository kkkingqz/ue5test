#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/FieldValidation.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/RepositoryBuilder.h"
#include "Misc/AutomationTest.h"

#include <map>

namespace
{
class FPresenceTestSourceProvider final : public GV2ContentCore::IContentSourceProvider
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

GV2ContentCore::FCompiledFieldSpecPtr CompilePresenceSpec(
    const std::string_view Source,
    std::vector<GV2ContentCore::FDiagnostic>& Diagnostics)
{
    using namespace GV2ContentCore;
    auto Document = ParseJson5Document(Source, FParseLimits{}, Diagnostics);
    if (!Document.has_value()) return nullptr;
    FValidationDiagnosticContext Context;
    Context.SchemaId = "core:schema.test.presence.v1";
    return CompileFieldSpec(Document->GetRootValue(), &*Document, "", Context, Diagnostics);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCorePresenceDefaultTest,
    "GV2.Runtime.ContentCore.PresenceAndDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCorePresenceDefaultTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;

    std::vector<FDiagnostic> Diagnostics;
    const FCompiledFieldSpecPtr Spec = CompilePresenceSpec(
        "{ kind: 'object', fields: {"
        "name: { kind: 'string', required: true },"
        "note: { kind: 'string' },"
        "nullable_note: { kind: 'string', nullable: true },"
        "default_null: { kind: 'string', nullable: true, default: null },"
        "count: { kind: 'int64', min: 0, default: 3 },"
        "items: { kind: 'array', items: { kind: 'string' }, default: [] },"
        "settings: { kind: 'object', fields: { level: { kind: 'int64', default: 5 } }, default: {} }"
        "} }",
        Diagnostics);
    TestTrue(TEXT("presence/default FieldSpec compiles"), Spec != nullptr && Diagnostics.empty());
    if (Spec == nullptr) return false;

    FValidationDiagnosticContext Context;
    Context.PackageId = "core";
    Context.RelativeSource = "definitions/presence.json5";
    Context.DefinitionId = "core:test.presence";
    Context.SchemaId = "core:schema.test.presence.v1";

    const FValue Input = FValue::MakeObject({
        { "name", FValue("sample") },
        { "nullable_note", FValue::MakeNull() }
    });
    FValue Materialized;
    TestTrue(TEXT("valid input materializes"), ValidateFieldValue(
        Input, *Spec, Materialized, nullptr, "/data", Context, Diagnostics));
    TestNull(TEXT("optional field without default stays absent"), Materialized.FindField("note"));
    TestTrue(TEXT("explicit nullable null stays present"),
        Materialized.FindField("nullable_note") != nullptr && Materialized.FindField("nullable_note")->IsNull());
    TestTrue(TEXT("explicit null default materializes as present null"),
        Materialized.FindField("default_null") != nullptr && Materialized.FindField("default_null")->IsNull());
    TestEqual(TEXT("scalar default materializes"),
        Materialized.FindField("count")->AsInteger(), static_cast<std::int64_t>(3));
    TestTrue(TEXT("explicit empty-array default materializes"),
        Materialized.FindField("items") != nullptr && Materialized.FindField("items")->AsArray().empty());
    TestEqual(TEXT("nested defaults are validated and materialized"),
        Materialized.FindField("settings")->FindField("level")->AsInteger(), static_cast<std::int64_t>(5));
    TestNull(TEXT("input remains unmodified"), Input.FindField("count"));

    FValue SecondMaterialization;
    Diagnostics.clear();
    TestTrue(TEXT("second materialization succeeds"), ValidateFieldValue(
        Input, *Spec, SecondMaterialization, nullptr, "/data", Context, Diagnostics));
    Materialized.FindField("items")->AsArray().push_back(FValue("changed"));
    TestTrue(TEXT("materialized defaults do not share mutable storage"),
        SecondMaterialization.FindField("items")->AsArray().empty());

    Diagnostics.clear();
    FValue UntouchedOutput("sentinel");
    auto MissingDocument = ParseJson5Document("{}", FParseLimits{}, Diagnostics);
    Diagnostics.clear();
    TestFalse(TEXT("missing required field fails"), ValidateFieldValue(
        MissingDocument->GetRootValue(), *Spec, UntouchedOutput,
        &*MissingDocument, "/data", Context, Diagnostics));
    TestTrue(TEXT("missing required field has stable code and pointer"), !Diagnostics.empty()
        && Diagnostics[0].Code == "core:diagnostic.schema.value.missing_required_field"
        && Diagnostics[0].JsonPointer == std::optional<std::string>("/data/name")
        && Diagnostics[0].Span.has_value());
    TestTrue(TEXT("failed validation does not publish partial output"),
        UntouchedOutput.IsString() && UntouchedOutput.AsString() == "sentinel");

    Diagnostics.clear();
    FValue NullOutput;
    TestFalse(TEXT("present null remains distinct and obeys nullable"), ValidateFieldValue(
        FValue::MakeObject({ { "name", FValue::MakeNull() } }),
        *Spec, NullOutput, nullptr, "/data", Context, Diagnostics));
    TestTrue(TEXT("non-nullable null has stable code"), !Diagnostics.empty()
        && Diagnostics[0].Code == "core:diagnostic.schema.value.null_not_allowed");

    Diagnostics.clear();
    TestTrue(TEXT("invalid explicit default is rejected during schema compilation"),
        CompilePresenceSpec(
            "{ kind: 'object', fields: { count: { kind: 'int64', min: 1, default: 0 } } }",
            Diagnostics) == nullptr);
    TestTrue(TEXT("invalid default points to default value"), !Diagnostics.empty()
        && Diagnostics[0].Code == "core:diagnostic.schema.value.constraint_failed"
        && Diagnostics[0].JsonPointer == std::optional<std::string>("/fields/count/default"));

    Diagnostics.clear();
    TestTrue(TEXT("required and default conflict is rejected"),
        CompilePresenceSpec(
            "{ kind: 'object', fields: { count: { kind: 'int64', required: true, default: 1 } } }",
            Diagnostics) == nullptr);
    TestTrue(TEXT("required/default conflict has stable schema code"), !Diagnostics.empty()
        && Diagnostics[0].Code == "core:diagnostic.schema.field_spec.conflicting_constraint");

    FPresenceTestSourceProvider Provider;
    Provider.Sources.emplace("core/schemas/item.json5",
        "{ id: 'core:schema.item.v1', definition_type: 'item', schema_version: 1, "
        "root: { kind: 'object', fields: { name: { kind: 'string', required: true } } }, "
        "semantic_validators: [], extensions: {} }");
    Provider.Sources.emplace("core/definitions/items.json5",
        "{ schema_version: 1, type: 'item', definitions: ["
        "{ id: 'core:item.missing_name', data: {} }] }");
    FBuildOptions Options;
    Options.SourceProvider = &Provider;
    const FBuildResult BuildResult = BuildRepository(
        { FPackageDescriptor("core", "core", 0, { "definitions/items.json5" },
            { FSchemaBinding("item", 1, "core:schema.item.v1", "schemas/item.json5") }) }, Options);
    TestTrue(TEXT("BuildRepository enforces required presence"), BuildResult.IsFailure());
    if (BuildResult.IsFailure())
    {
        const FDiagnostic& Diagnostic = BuildResult.GetDiagnostics()[0];
        TestEqual(TEXT("integrated missing-field code"), Diagnostic.Code,
            std::string("core:diagnostic.schema.value.missing_required_field"));
        TestTrue(TEXT("integrated missing-field pointer"), Diagnostic.JsonPointer
            == std::optional<std::string>("/definitions/0/data/name"));
    }
    return true;
}

#endif
