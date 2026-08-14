#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/FieldValidation.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/RepositoryBuilder.h"
#include "Misc/AutomationTest.h"

#include <algorithm>
#include <map>

namespace
{
class FSpecialFieldSourceProvider final : public GV2ContentCore::IContentSourceProvider
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

GV2ContentCore::FCompiledFieldSpecPtr CompileSpecialSpec(
    const std::string_view Source,
    std::vector<GV2ContentCore::FDiagnostic>& Diagnostics)
{
    using namespace GV2ContentCore;
    auto Document = ParseJson5Document(Source, FParseLimits{}, Diagnostics);
    if (!Document.has_value()) return nullptr;
    FValidationDiagnosticContext Context;
    Context.SchemaId = "core:schema.test.special.v1";
    return CompileFieldSpec(Document->GetRootValue(), &*Document, "", Context, Diagnostics);
}

const GV2ContentCore::FCompiledObjectField* FindCompiledField(
    const GV2ContentCore::FCompiledFieldSpec& Spec,
    const std::string_view Name)
{
    const auto Found = std::find_if(
        Spec.Fields.begin(), Spec.Fields.end(),
        [Name](const GV2ContentCore::FCompiledObjectField& Field) { return Field.Name == Name; });
    return Found == Spec.Fields.end() ? nullptr : &*Found;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreSpecialFieldValidationTest,
    "GV2.Runtime.ContentCore.SpecialFieldValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreSpecialFieldValidationTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;

    std::vector<FDiagnostic> Diagnostics;
    const FCompiledFieldSpecPtr Spec = CompileSpecialSpec(
        "{ kind: 'object', fields: {"
        "screen_id: { kind: 'ref', required: true, target_kind: 'screen' },"
        "title_text_id: { kind: 'text_id', default: 'core:text.default.title' },"
        "icon_resource_id: { kind: 'resource_ref', resource_class: 'texture_2d', bootstrap_required: true }"
        "} }",
        Diagnostics);
    TestTrue(TEXT("special FieldSpecs compile"), Spec != nullptr && Diagnostics.empty());
    if (Spec == nullptr) return false;

    const FCompiledObjectField* ScreenField = FindCompiledField(*Spec, "screen_id");
    const FCompiledObjectField* TextField = FindCompiledField(*Spec, "title_text_id");
    const FCompiledObjectField* ResourceField = FindCompiledField(*Spec, "icon_resource_id");
    TestTrue(TEXT("ref retains expected kind metadata"), ScreenField != nullptr
        && ScreenField->Spec->Kind == EFieldKind::Reference
        && ScreenField->Spec->ExpectedStableIdKind == "screen");
    TestTrue(TEXT("text_id has implicit text kind"), TextField != nullptr
        && TextField->Spec->Kind == EFieldKind::TextId
        && TextField->Spec->ExpectedStableIdKind == "text");
    TestTrue(TEXT("resource_ref retains class and bootstrap metadata"), ResourceField != nullptr
        && ResourceField->Spec->Kind == EFieldKind::ResourceReference
        && ResourceField->Spec->ExpectedStableIdKind == "resource"
        && ResourceField->Spec->ResourceClass == "texture_2d"
        && ResourceField->Spec->bBootstrapRequired);

    FValidationDiagnosticContext Context;
    Context.PackageId = "core";
    Context.RelativeSource = "definitions/special.json5";
    Context.DefinitionId = "core:test.special";
    Context.SchemaId = "core:schema.test.special.v1";

    FValue Materialized;
    TestTrue(TEXT("canonical special values validate"), ValidateFieldValue(
        FValue::MakeObject({
            { "screen_id", FValue("core:screen.does_not_need_to_exist_yet") },
            { "icon_resource_id", FValue("core:resource.ui.icon") }
        }),
        *Spec, Materialized, nullptr, "/data", Context, Diagnostics));
    TestEqual(TEXT("text_id default uses the same special validator"),
        Materialized.FindField("title_text_id")->AsString(), std::string("core:text.default.title"));

    Diagnostics.clear();
    TestFalse(TEXT("malformed Stable ID is rejected"), ValidateFieldValue(
        FValue::MakeObject({ { "screen_id", FValue("Core:screen.bad") } }),
        *Spec, Materialized, nullptr, "/data", Context, Diagnostics));
    TestTrue(TEXT("malformed Stable ID code is stable"), !Diagnostics.empty()
        && Diagnostics[0].Code == "core:diagnostic.schema.value.invalid_stable_id"
        && Diagnostics[0].JsonPointer == std::optional<std::string>("/data/screen_id"));

    Diagnostics.clear();
    TestFalse(TEXT("wrong Stable ID kind is rejected"), ValidateFieldValue(
        FValue::MakeObject({ { "screen_id", FValue("core:item.wrong_kind") } }),
        *Spec, Materialized, nullptr, "/data", Context, Diagnostics));
    TestTrue(TEXT("wrong-kind code and expected kind are stable"), !Diagnostics.empty()
        && Diagnostics[0].Code == "core:diagnostic.schema.value.stable_id_wrong_kind"
        && Diagnostics[0].Message.find("screen") != std::string::npos);

    Diagnostics.clear();
    TestFalse(TEXT("special fields do not coerce non-string values"), ValidateFieldValue(
        FValue::MakeObject({ { "screen_id", FValue(1) } }),
        *Spec, Materialized, nullptr, "/data", Context, Diagnostics));
    TestTrue(TEXT("special type mismatch uses common code"), !Diagnostics.empty()
        && Diagnostics[0].Code == "core:diagnostic.schema.value.type_mismatch");

    Diagnostics.clear();
    TestTrue(TEXT("invalid target_kind metadata is rejected"), CompileSpecialSpec(
        "{ kind: 'ref', target_kind: 'Screen' }", Diagnostics) == nullptr);
    TestTrue(TEXT("invalid target_kind code is stable"), !Diagnostics.empty()
        && Diagnostics[0].Code == "core:diagnostic.schema.field_spec.invalid_target_kind");

    Diagnostics.clear();
    TestTrue(TEXT("resource_ref requires resource_class"), CompileSpecialSpec(
        "{ kind: 'resource_ref' }", Diagnostics) == nullptr);
    TestTrue(TEXT("missing resource_class code is stable"), !Diagnostics.empty()
        && Diagnostics[0].Code == "core:diagnostic.schema.field_spec.invalid_resource_class");

    Diagnostics.clear();
    TestTrue(TEXT("special FieldSpec is closed"), CompileSpecialSpec(
        "{ kind: 'text_id', target_kind: 'text' }", Diagnostics) == nullptr);
    TestTrue(TEXT("unknown special keyword uses common schema code"), !Diagnostics.empty()
        && Diagnostics[0].Code == "core:diagnostic.schema.field_spec.unknown_field");

    Diagnostics.clear();
    TestTrue(TEXT("invalid special default fails schema compilation"), CompileSpecialSpec(
        "{ kind: 'text_id', default: 'core:screen.not_text' }", Diagnostics) == nullptr);
    TestTrue(TEXT("invalid special default retains default pointer"), !Diagnostics.empty()
        && Diagnostics[0].Code == "core:diagnostic.schema.value.stable_id_wrong_kind"
        && Diagnostics[0].JsonPointer == std::optional<std::string>("/default"));

    FSpecialFieldSourceProvider Provider;
    Provider.Sources.emplace("core/schemas/location.json5",
        "{ id: 'core:schema.location.v1', definition_type: 'location', schema_version: 1, "
        "root: { kind: 'object', fields: { screen_id: { kind: 'ref', required: true, target_kind: 'screen' } } }, "
        "semantic_validators: [], extensions: {} }");
    Provider.Sources.emplace("core/definitions/locations.json5",
        "{ schema_version: 1, type: 'location', definitions: ["
        "{ id: 'core:location.invalid', data: { screen_id: 'core:item.wrong_kind' } }] }");
    FBuildOptions Options;
    Options.SourceProvider = &Provider;
    const FBuildResult BuildResult = BuildRepository(
        { FPackageDescriptor("core", "core", 0, { "definitions/locations.json5" },
            { FSchemaBinding("location", 1, "core:schema.location.v1", "schemas/location.json5") }) }, Options);
    TestTrue(TEXT("BuildRepository validates special field shapes"), BuildResult.IsFailure());
    if (BuildResult.IsFailure())
    {
        const FDiagnostic& Diagnostic = BuildResult.GetDiagnostics()[0];
        TestEqual(TEXT("integrated wrong-kind code"), Diagnostic.Code,
            std::string("core:diagnostic.schema.value.stable_id_wrong_kind"));
        TestTrue(TEXT("integrated special pointer"), Diagnostic.JsonPointer
            == std::optional<std::string>("/definitions/0/data/screen_id"));
        TestTrue(TEXT("integrated special source span"), Diagnostic.Span.has_value());
    }
    return true;
}

#endif
