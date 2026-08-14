#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/RepositoryBuilder.h"
#include "GV2ContentCore/ScalarValidation.h"
#include "Misc/AutomationTest.h"

#include <map>

namespace
{
class FScalarTestSourceProvider final : public GV2ContentCore::IContentSourceProvider
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

std::optional<GV2ContentCore::FScalarFieldSpec> CompileSpec(
    const std::string_view Source,
    std::vector<GV2ContentCore::FDiagnostic>& OutDiagnostics)
{
    using namespace GV2ContentCore;
    auto Document = ParseJson5Document(Source, FParseLimits{}, OutDiagnostics);
    if (!Document.has_value()) return std::nullopt;
    FValidationDiagnosticContext Context;
    Context.SchemaId = "core:schema.test.scalar.v1";
    return CompileScalarFieldSpec(
        Document->GetRootValue(), &*Document, "", Context, OutDiagnostics);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreScalarValidationTest,
    "GV2.Runtime.ContentCore.ScalarValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreScalarValidationTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;

    std::vector<FDiagnostic> Diagnostics;
    auto IntegerSpec = CompileSpec("{ kind: 'int64', min: 1, max: 3 }", Diagnostics);
    TestTrue(TEXT("int64 FieldSpec compiles"), IntegerSpec.has_value() && Diagnostics.empty());
    if (!IntegerSpec.has_value()) return false;

    FValidationDiagnosticContext ValueContext;
    ValueContext.PackageId = "core";
    ValueContext.PackageLoadIndex = 0;
    ValueContext.RelativeSource = "definitions/scalars.json5";
    ValueContext.DefinitionId = "core:scalar.test";
    ValueContext.SchemaId = "core:schema.test.scalar.v1";

    TestTrue(TEXT("int64 inside inclusive bounds validates"), ValidateScalarValue(FValue(2), *IntegerSpec, nullptr, "/data", ValueContext, Diagnostics));
    Diagnostics.clear();
    TestFalse(TEXT("double is not coerced to int64"), ValidateScalarValue(FValue(2.0), *IntegerSpec, nullptr, "/data", ValueContext, Diagnostics));
    TestTrue(TEXT("Type mismatch has stable code"), !Diagnostics.empty() && Diagnostics[0].Code == "core:diagnostic.schema.value.type_mismatch");
    TestTrue(TEXT("Type mismatch retains JSON pointer"), Diagnostics[0].JsonPointer == std::optional<std::string>("/data"));
    TestTrue(TEXT("Type mismatch retains definition ID"), Diagnostics[0].DefinitionId == ValueContext.DefinitionId);

    Diagnostics.clear();
    TestFalse(TEXT("int64 bound violation fails"), ValidateScalarValue(FValue(4), *IntegerSpec, nullptr, "/data", ValueContext, Diagnostics));
    TestTrue(TEXT("Bound failure has stable code"), !Diagnostics.empty() && Diagnostics[0].Code == "core:diagnostic.schema.value.constraint_failed");

    Diagnostics.clear();
    auto NumberSpec = CompileSpec("{ kind: 'number', exclusive_min: 0, max: 2.0 }", Diagnostics);
    TestTrue(TEXT("number FieldSpec compiles"), NumberSpec.has_value() && Diagnostics.empty());
    if (NumberSpec.has_value())
    {
        TestTrue(TEXT("finite double in range validates"), ValidateScalarValue(FValue(1.0), *NumberSpec, nullptr, "", ValueContext, Diagnostics));
        Diagnostics.clear();
        TestFalse(TEXT("exclusive lower bound rejects equality"), ValidateScalarValue(FValue(0.0), *NumberSpec, nullptr, "", ValueContext, Diagnostics));
        Diagnostics.clear();
        TestFalse(TEXT("int64 is not coerced to number"), ValidateScalarValue(FValue(1), *NumberSpec, nullptr, "", ValueContext, Diagnostics));
    }

    Diagnostics.clear();
    auto StringSpec = CompileSpec(
        "{ kind: 'string', min_length: 2, max_length: 4, pattern: '^[a-z]+$' }",
        Diagnostics);
    TestTrue(TEXT("string FieldSpec compiles"), StringSpec.has_value() && Diagnostics.empty());
    if (StringSpec.has_value())
    {
        TestTrue(TEXT("matching string validates"), ValidateScalarValue(FValue("abc"), *StringSpec, nullptr, "", ValueContext, Diagnostics));
        Diagnostics.clear();
        TestFalse(TEXT("pattern mismatch fails"), ValidateScalarValue(FValue("A1"), *StringSpec, nullptr, "", ValueContext, Diagnostics));
    }

    Diagnostics.clear();
    auto UnicodeLengthSpec = CompileSpec("{ kind: 'string', min_length: 2, max_length: 2 }", Diagnostics);
    TestTrue(TEXT("UTF-8 length counts Unicode code points"), UnicodeLengthSpec.has_value() && ValidateScalarValue(FValue("яю"), *UnicodeLengthSpec, nullptr, "", ValueContext, Diagnostics));

    Diagnostics.clear();
    auto FormatSpec = CompileSpec("{ kind: 'string', format: 'stable_id' }", Diagnostics);
    TestTrue(TEXT("stable_id format compiles"), FormatSpec.has_value() && Diagnostics.empty());
    if (FormatSpec.has_value())
    {
        TestTrue(TEXT("canonical Stable ID satisfies format"), ValidateScalarValue(FValue("core:item.test"), *FormatSpec, nullptr, "", ValueContext, Diagnostics));
        Diagnostics.clear();
        TestFalse(TEXT("noncanonical Stable ID fails format"), ValidateScalarValue(FValue("Core:item.test"), *FormatSpec, nullptr, "", ValueContext, Diagnostics));
    }

    Diagnostics.clear();
    auto EnumSpec = CompileSpec("{ kind: 'enum', values: ['small', 'large', 2] }", Diagnostics);
    TestTrue(TEXT("enum FieldSpec compiles"), EnumSpec.has_value() && Diagnostics.empty());
    if (EnumSpec.has_value())
    {
        TestTrue(TEXT("declared enum member validates"), ValidateScalarValue(FValue("large"), *EnumSpec, nullptr, "", ValueContext, Diagnostics));
        Diagnostics.clear();
        TestFalse(TEXT("undeclared enum member fails"), ValidateScalarValue(FValue("medium"), *EnumSpec, nullptr, "", ValueContext, Diagnostics));
    }

    Diagnostics.clear();
    auto NullableBoolean = CompileSpec("{ kind: 'bool', nullable: true }", Diagnostics);
    TestTrue(TEXT("bool accepts bool without coercion"), NullableBoolean.has_value() && ValidateScalarValue(FValue(true), *NullableBoolean, nullptr, "", ValueContext, Diagnostics));
    TestTrue(TEXT("nullable bool accepts explicit null"), NullableBoolean.has_value() && ValidateScalarValue(FValue::MakeNull(), *NullableBoolean, nullptr, "", ValueContext, Diagnostics));
    Diagnostics.clear();
    auto NonNullableBoolean = CompileSpec("{ kind: 'bool' }", Diagnostics);
    TestFalse(TEXT("non-nullable bool rejects explicit null"), ValidateScalarValue(FValue::MakeNull(), *NonNullableBoolean, nullptr, "", ValueContext, Diagnostics));
    TestTrue(TEXT("Null failure has stable code"), !Diagnostics.empty() && Diagnostics[0].Code == "core:diagnostic.schema.value.null_not_allowed");

    Diagnostics.clear();
    TestFalse(TEXT("Invalid range fails FieldSpec compilation"), CompileSpec("{ kind: 'int64', min: 5, max: 2 }", Diagnostics).has_value());
    TestTrue(TEXT("Invalid range has stable code"), !Diagnostics.empty() && Diagnostics[0].Code == "core:diagnostic.schema.field_spec.invalid_constraint_range");

    Diagnostics.clear();
    TestFalse(TEXT("Invalid regex fails FieldSpec compilation"), CompileSpec("{ kind: 'string', pattern: '[' }", Diagnostics).has_value());
    TestTrue(TEXT("Invalid regex has stable code"), !Diagnostics.empty() && Diagnostics[0].Code == "core:diagnostic.schema.field_spec.invalid_pattern");

    Diagnostics.clear();
    TestFalse(TEXT("Unknown string format fails FieldSpec compilation"), CompileSpec("{ kind: 'string', format: 'host_path' }", Diagnostics).has_value());
    TestTrue(TEXT("Unknown format has stable code"), !Diagnostics.empty() && Diagnostics[0].Code == "core:diagnostic.schema.field_spec.invalid_format");

    Diagnostics.clear();
    TestFalse(TEXT("Duplicate enum member fails FieldSpec compilation"), CompileSpec("{ kind: 'enum', values: ['same', 'same'] }", Diagnostics).has_value());
    TestTrue(TEXT("Duplicate enum has stable code"), !Diagnostics.empty() && Diagnostics[0].Code == "core:diagnostic.schema.field_spec.duplicate_enum_value");

    FScalarTestSourceProvider Provider;
    Provider.Sources.emplace(
        "core/schemas/score.json5",
        "{ id: 'core:schema.score.v1', definition_type: 'score', schema_version: 1, "
        "root: { kind: 'int64', min: 0 }, semantic_validators: [], extensions: {} }");
    Provider.Sources.emplace(
        "core/definitions/scores.json5",
        "{ schema_version: 1, type: 'score', definitions: ["
        "{ id: 'core:score.invalid', data: -1 }] }");
    FBuildOptions Options;
    Options.SourceProvider = &Provider;
    const FBuildResult BuildResult = BuildRepository(
        { FPackageDescriptor(
            "core", "core", 0,
            { "definitions/scores.json5" },
            { FSchemaBinding("score", 1, "core:schema.score.v1", "schemas/score.json5") }) },
        Options);
    TestTrue(TEXT("BuildRepository runs scalar validation"), BuildResult.IsFailure());
    if (BuildResult.IsFailure())
    {
        const FDiagnostic& Diagnostic = BuildResult.GetDiagnostics()[0];
        TestEqual(TEXT("Integrated constraint diagnostic"), Diagnostic.Code, std::string("core:diagnostic.schema.value.constraint_failed"));
        TestTrue(TEXT("Integrated diagnostic retains data pointer"), Diagnostic.JsonPointer == std::optional<std::string>("/definitions/0/data"));
        TestTrue(TEXT("Integrated diagnostic retains value span"), Diagnostic.Span.has_value());
        TestTrue(TEXT("Integrated diagnostic retains schema version"),
            Diagnostic.SchemaVersion == std::optional<std::int64_t>(1));
    }

    return true;
}

#endif
