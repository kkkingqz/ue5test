#include "GV2ContentCore/BuildResult.h"
#include "GV2ContentCore/DefinitionEnvelope.h"
#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/ExtensionSchema.h"
#include "GV2ContentCore/FieldValidation.h"
#include "GV2ContentCore/Json5Lexer.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentCore/PackageDiscovery.h"
#include "GV2ContentCore/ParseLimits.h"
#include "GV2ContentCore/RepositoryBuilder.h"
#include "GV2ContentCore/SchemaRegistry.h"
#include "GV2ContentCore/ScalarValidation.h"
#include "GV2ContentCore/Testing/Json5Conformance.h"
#include "GV2ContentCore/Testing/RepresentativeCore.h"
#include "GV2ContentCore/Value.h"
#include "GV2RuntimeCore/GV2HostServices.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"
#include "GV2RuntimeCore/Testing/GV2StableIdConformance.h"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
class FMemoryContentSourceProvider final : public GV2ContentCore::IContentSourceProvider
{
public:
    std::map<std::string, std::string> Sources;
    mutable std::vector<std::string> Reads;

    std::optional<std::string> ReadSource(
        const std::string_view PackageId,
        const std::string_view RelativeSource) const override
    {
        const std::string Key = std::string(PackageId) + "/" + std::string(RelativeSource);
        Reads.push_back(Key);
        const auto Found = Sources.find(Key);
        return Found == Sources.end() ? std::nullopt : std::optional<std::string>(Found->second);
    }
};

class FDuplicateCoreValidator final : public GV2ContentCore::ISemanticValidator
{
public:
    std::string_view GetId() const override
    {
        return "core:validator.item.positive_price";
    }

    void Validate(
        const GV2ContentCore::FValue&,
        const GV2ContentCore::FSemanticCandidateView&,
        const GV2ContentCore::FSemanticValidationContext&,
        std::vector<GV2ContentCore::FDiagnostic>&) const override
    {
    }
};

static bool RunContentCoreJson5ParserSelfTest()
{
    using namespace GV2ContentCore;

    FParseLimits Limits;

    // 1. Complex valid document (Cyrillic, comments, trailing commas, hex, floats, escaped strings)
    std::vector<FDiagnostic> Diags;
    std::string ConformanceDoc = R"(
        // Subsystem configuration fixture
        {
            /* General metadata */
            title: "Тестовый пакет",
            version: 1,
            ratio: 3.14159,
            hex_id: 0x1F,
            exp_val: 1e3,
            neg_zero: -0.0, // Should normalize to +0.0
            multiline: "Первая строка \
Вторая строка",
            tags: ["тест", "кириллица", 100,],
            nested: {
                flag: true,
                empty_val: null,
            },
        }
    )";

    auto Val1 = ParseJson5(ConformanceDoc, Limits, Diags);
    if (!Val1.has_value() || !Diags.empty() || !Val1->IsObject()) return false;

    // Check parsed fields
    const auto& Obj1 = Val1->AsObject();
    if (Obj1.size() != 9) return false;

    // Check -0.0 normalization
    auto NegZeroProp = Val1->FindField("neg_zero");
    if (!NegZeroProp || !NegZeroProp->IsNumber() || std::signbit(NegZeroProp->AsNumber())) return false;

    // 2. Determinism check: repeat parse produces exact same result
    std::vector<FDiagnostic> Diags2;
    auto Val2 = ParseJson5(ConformanceDoc, Limits, Diags2);
    if (!Val2.has_value() || !Diags2.empty() || (*Val1 != *Val2)) return false;

    // 3. Error cases: Syntax error
    Diags.clear();
    auto FailVal = ParseJson5("{ key: ", Limits, Diags);
    if (FailVal.has_value() || Diags.empty() || Diags[0].Code != "core:diagnostic.json5.unexpected_eof") return false;

    // 4. Error cases: Duplicate key
    Diags.clear();
    auto DupVal = ParseJson5("{ key: 1, key: 2 }", Limits, Diags);
    if (DupVal.has_value() || Diags.empty() || Diags[0].Code != "core:diagnostic.json5.duplicate_key" || !Diags[0].RelatedSpan.has_value()) return false;

    // 5. Error cases: Reject NaN/Infinity
    Diags.clear();
    auto NanVal = ParseJson5("NaN", Limits, Diags);
    if (NanVal.has_value() || Diags.empty() || Diags[0].Code != "core:diagnostic.json5.invalid_number") return false;

    Diags.clear();
    auto InfVal = ParseJson5("Infinity", Limits, Diags);
    if (InfVal.has_value() || Diags.empty() || Diags[0].Code != "core:diagnostic.json5.invalid_number") return false;

    Diags.clear();
    auto MinHex = ParseJson5("-0x8000000000000000", Limits, Diags);
    if (!MinHex.has_value() || MinHex->AsInteger() != std::numeric_limits<std::int64_t>::min()) return false;

    Diags.clear();
    auto Emoji = ParseJson5("\"\\uD83D\\uDE00\"", Limits, Diags);
    if (!Emoji.has_value() || Emoji->AsString() != std::string("\xF0\x9F\x98\x80", 4)) return false;

    return true;
}

bool RunContentCoreSchemaRegistrySelfTest()
{
    using namespace GV2ContentCore;

    const std::string SchemaSource =
        "{ id: 'core:schema.definition.item.v1', definition_type: 'item', schema_version: 1, "
        "root: { kind: 'object', fields: {} }, semantic_validators: [], extensions: {} }";
    std::vector<FDiagnostic> Diagnostics;
    auto Document = ParseJson5Document(SchemaSource, FParseLimits{}, Diagnostics);
    if (!Document.has_value() || !Diagnostics.empty()) return false;

    const FSchemaBinding Binding(
        "item", 1, "core:schema.definition.item.v1", "schemas/not_derived_from_name.json5");
    auto Resource = ParseSchemaResource(
        *Document, Binding, "core", 0, Binding.GetRelativePath(), Diagnostics);
    if (!Resource.has_value() || !Diagnostics.empty()) return false;

    FSchemaRegistry Registry;
    if (!Registry.Register(std::move(*Resource), Diagnostics)
        || Registry.Find("item", 1) == nullptr
        || Registry.Find("item", 2) != nullptr) return false;

    Diagnostics.clear();
    auto Duplicate = ParseSchemaResource(
        *Document, Binding, "test_mod", 1, Binding.GetRelativePath(), Diagnostics);
    if (!Duplicate.has_value()
        || Registry.Register(std::move(*Duplicate), Diagnostics)
        || Diagnostics.empty()
        || Diagnostics.back().Code != "core:diagnostic.schema.binding.duplicate") return false;

    FMemoryContentSourceProvider MissingProvider;
    MissingProvider.Sources.emplace(
        "core/definitions/items.json5",
        "{ schema_version: 1, type: 'item', definitions: [] }");
    FBuildOptions Options;
    Options.SourceProvider = &MissingProvider;
    const FBuildResult MissingResult = BuildRepository(
        { FPackageDescriptor("core", "core", 0, { "definitions/items.json5" }) },
        Options);
    return MissingResult.IsFailure()
        && MissingResult.GetDiagnostics().front().Code == "core:diagnostic.schema.binding.missing"
        && MissingResult.GetDiagnostics().front().JsonPointer == std::optional<std::string>("/schema_version");
}

bool RunContentCoreScalarValidationSelfTest()
{
    using namespace GV2ContentCore;

    std::vector<FDiagnostic> Diagnostics;
    auto SpecDocument = ParseJson5Document(
        "{ kind: 'int64', min: 1, max: 3 }", FParseLimits{}, Diagnostics);
    if (!SpecDocument.has_value()) return false;
    FValidationDiagnosticContext Context;
    Context.PackageId = "core";
    Context.RelativeSource = "scalar.self_test.json5";
    Context.SchemaId = "core:schema.scalar.self_test";
    auto IntegerSpec = CompileScalarFieldSpec(
        SpecDocument->GetRootValue(), &*SpecDocument, "", Context, Diagnostics);
    if (!IntegerSpec.has_value() || !Diagnostics.empty()) return false;
    if (!ValidateScalarValue(FValue(2), *IntegerSpec, nullptr, "/data", Context, Diagnostics)) return false;

    Diagnostics.clear();
    if (ValidateScalarValue(FValue(2.0), *IntegerSpec, nullptr, "/data", Context, Diagnostics)
        || Diagnostics.empty()
        || Diagnostics.front().Code != "core:diagnostic.schema.value.type_mismatch") return false;

    Diagnostics.clear();
    auto StringDocument = ParseJson5Document(
        "{ kind: 'string', min_length: 2, pattern: '^[a-z_]+$', format: 'stable_id_segment' }",
        FParseLimits{}, Diagnostics);
    if (!StringDocument.has_value()) return false;
    auto StringSpec = CompileScalarFieldSpec(
        StringDocument->GetRootValue(), &*StringDocument, "", Context, Diagnostics);
    if (!StringSpec.has_value() || !Diagnostics.empty()) return false;
    if (!ValidateScalarValue(FValue("valid_name"), *StringSpec, nullptr, "/data", Context, Diagnostics)) return false;

    Diagnostics.clear();
    auto EnumDocument = ParseJson5Document(
        "{ kind: 'enum', nullable: true, values: ['small', 'large'] }",
        FParseLimits{}, Diagnostics);
    if (!EnumDocument.has_value()) return false;
    auto EnumSpec = CompileScalarFieldSpec(
        EnumDocument->GetRootValue(), &*EnumDocument, "", Context, Diagnostics);
    return EnumSpec.has_value()
        && Diagnostics.empty()
        && ValidateScalarValue(FValue("large"), *EnumSpec, nullptr, "/data", Context, Diagnostics)
        && ValidateScalarValue(FValue::MakeNull(), *EnumSpec, nullptr, "/data", Context, Diagnostics);
}

bool RunContentCoreContainerValidationSelfTest()
{
    using namespace GV2ContentCore;

    std::vector<FDiagnostic> Diagnostics;
    auto Document = ParseJson5Document(
        "{ kind: 'object', fields: { entries: { kind: 'array', unique: true, items: { "
        "kind: 'union', discriminator: 'kind', variants: { add: { kind: 'object', fields: { "
        "kind: { kind: 'enum', values: ['add'] }, value: { kind: 'int64' } } } } } } } }",
        FParseLimits{}, Diagnostics);
    if (!Document.has_value()) return false;
    FValidationDiagnosticContext Context;
    Context.SchemaId = "core:schema.container.self_test";
    const FCompiledFieldSpecPtr Spec = CompileFieldSpec(
        Document->GetRootValue(), &*Document, "", Context, Diagnostics);
    if (Spec == nullptr || !Diagnostics.empty()) return false;

    const FValue Valid = FValue::MakeObject({
        { "entries", FValue::MakeArray({ FValue::MakeObject({
            { "kind", FValue("add") }, { "value", FValue(2) } }) }) }
    });
    FValue Materialized;
    if (!ValidateFieldValue(Valid, *Spec, Materialized, nullptr, "/data", Context, Diagnostics)
        || !Diagnostics.empty()
        || Materialized.FindField("entries")->AsArray()[0].FindField("value")->AsInteger() != 2) return false;

    Diagnostics.clear();
    const FValue Invalid = FValue::MakeObject({
        { "entries", FValue::MakeArray({ FValue::MakeObject({
            { "kind", FValue("unknown") } }) }) }
    });
    if (ValidateFieldValue(Invalid, *Spec, Materialized, nullptr, "/data", Context, Diagnostics)
        || Diagnostics.empty()
        || Diagnostics.front().Code != "core:diagnostic.schema.value.invalid_union_variant"
        || Diagnostics.front().JsonPointer != std::optional<std::string>("/data/entries/0/kind")) return false;

    Diagnostics.clear();
    auto UniqueObjectDocument = ParseJson5Document(
        "{ kind: 'array', unique: true, items: { kind: 'object', fields: {"
        "a: { kind: 'int64' }, b: { kind: 'int64' } } } }",
        FParseLimits{}, Diagnostics);
    if (!UniqueObjectDocument.has_value()) return false;
    const FCompiledFieldSpecPtr UniqueObjectSpec = CompileFieldSpec(
        UniqueObjectDocument->GetRootValue(), &*UniqueObjectDocument, "", Context, Diagnostics);
    if (UniqueObjectSpec == nullptr || !Diagnostics.empty()) return false;
    const FValue PermutedDuplicates = FValue::MakeArray({
        FValue::MakeObject({ { "a", FValue(1) }, { "b", FValue(2) } }),
        FValue::MakeObject({ { "b", FValue(2) }, { "a", FValue(1) } }),
    });
    return !ValidateFieldValue(
            PermutedDuplicates, *UniqueObjectSpec, Materialized, nullptr, "/data", Context, Diagnostics)
        && !Diagnostics.empty()
        && Diagnostics.front().Code == "core:diagnostic.schema.value.duplicate_array_item";
}

bool RunContentCorePresenceDefaultSelfTest()
{
    using namespace GV2ContentCore;

    std::vector<FDiagnostic> Diagnostics;
    auto Document = ParseJson5Document(
        "{ kind: 'object', fields: { name: { kind: 'string', required: true }, "
        "optional: { kind: 'string' }, count: { kind: 'int64', default: 2 }, "
        "items: { kind: 'array', items: { kind: 'string' }, default: [] } } }",
        FParseLimits{}, Diagnostics);
    if (!Document.has_value()) return false;
    FValidationDiagnosticContext Context;
    Context.SchemaId = "core:schema.presence.self_test";
    const FCompiledFieldSpecPtr Spec = CompileFieldSpec(
        Document->GetRootValue(), &*Document, "", Context, Diagnostics);
    if (Spec == nullptr || !Diagnostics.empty()) return false;

    const FValue Input = FValue::MakeObject({ { "name", FValue("valid") } });
    FValue First;
    if (!ValidateFieldValue(Input, *Spec, First, nullptr, "/data", Context, Diagnostics)
        || First.FindField("optional") != nullptr
        || First.FindField("count") == nullptr
        || First.FindField("count")->AsInteger() != 2
        || First.FindField("items") == nullptr
        || !First.FindField("items")->AsArray().empty()
        || Input.FindField("count") != nullptr) return false;

    FValue Second;
    if (!ValidateFieldValue(Input, *Spec, Second, nullptr, "/data", Context, Diagnostics)) return false;
    First.FindField("items")->AsArray().push_back(FValue("changed"));
    if (!Second.FindField("items")->AsArray().empty()) return false;

    Diagnostics.clear();
    FValue Unchanged("sentinel");
    return !ValidateFieldValue(
            FValue::MakeObject(), *Spec, Unchanged, nullptr, "/data", Context, Diagnostics)
        && !Diagnostics.empty()
        && Diagnostics.front().Code == "core:diagnostic.schema.value.missing_required_field"
        && Unchanged.IsString()
        && Unchanged.AsString() == "sentinel";
}

bool RunContentCoreSpecialFieldSelfTest()
{
    using namespace GV2ContentCore;

    std::vector<FDiagnostic> Diagnostics;
    auto Document = ParseJson5Document(
        "{ kind: 'object', fields: { target: { kind: 'ref', target_kind: 'screen' }, "
        "title: { kind: 'text_id', default: 'core:text.default.title' }, "
        "icon: { kind: 'resource_ref', resource_class: 'texture_2d', bootstrap_required: true } } }",
        FParseLimits{}, Diagnostics);
    if (!Document.has_value()) return false;
    FValidationDiagnosticContext Context;
    Context.SchemaId = "core:schema.special.self_test";
    const FCompiledFieldSpecPtr Spec = CompileFieldSpec(
        Document->GetRootValue(), &*Document, "", Context, Diagnostics);
    if (Spec == nullptr || !Diagnostics.empty()) return false;

    FValue Materialized;
    if (!ValidateFieldValue(
            FValue::MakeObject({
                { "target", FValue("core:screen.main") },
                { "icon", FValue("core:resource.ui.icon") }
            }),
            *Spec, Materialized, nullptr, "/data", Context, Diagnostics)
        || Materialized.FindField("title") == nullptr
        || Materialized.FindField("title")->AsString() != "core:text.default.title") return false;

    Diagnostics.clear();
    return !ValidateFieldValue(
            FValue::MakeObject({ { "target", FValue("core:item.wrong_kind") } }),
            *Spec, Materialized, nullptr, "/data", Context, Diagnostics)
        && !Diagnostics.empty()
        && Diagnostics.front().Code == "core:diagnostic.schema.value.stable_id_wrong_kind";
}

bool RunContentCoreDefinitionEnvelopeSelfTest()
{
    using namespace GV2ContentCore;

    std::vector<FDiagnostic> Diagnostics;
    auto Document = ParseJson5Document(
        "{ schema_version: 1, type: 'item', definitions: ["
        "{ id: 'core:item.sword', data: {}, tags: ['weapon'], deprecated: false },"
        "{ id: 'core:item.potion', data: null }"
        "] }",
        FParseLimits{}, Diagnostics);
    if (!Document.has_value()) return false;
    auto File = ParseDefinitionFileEnvelope(
        *Document, "core", 0, "definitions/items.json5", Diagnostics);
    if (!File.has_value() || !Diagnostics.empty()
        || File->GetDefinitionType() != "item"
        || File->GetDefinitions().size() != 2
        || !File->GetDefinitions()[1].GetData().IsNull()
        || File->GetDefinitions()[1].IsDeprecated()) return false;

    Diagnostics.clear();
    auto InvalidDocument = ParseJson5Document(
        "{ schema_version: 1, type: 'item', definitions: ["
        "{ id: 'core:screen.bad_kind', data: {} }] }",
        FParseLimits{}, Diagnostics);
    if (!InvalidDocument.has_value()) return false;
    if (ParseDefinitionFileEnvelope(
            *InvalidDocument, "core", 0, "definitions/invalid.json5", Diagnostics).has_value()
        || Diagnostics.empty()
        || Diagnostics.front().Code != "core:diagnostic.definition.entry.id_kind_mismatch") return false;

    Diagnostics.clear();
    auto DuplicateDocument = ParseJson5Document(
        "{ schema_version: 1, type: 'item', definitions: ["
        "{ id: 'core:item.same', data: {} }, { id: 'core:item.same', data: {} }] }",
        FParseLimits{}, Diagnostics);
    if (!DuplicateDocument.has_value()) return false;
    auto DuplicateFile = ParseDefinitionFileEnvelope(
        *DuplicateDocument, "core", 0, "definitions/duplicate.json5", Diagnostics);
    if (!DuplicateFile.has_value()) return false;
    std::vector<FDefinitionFile> Files;
    Files.push_back(std::move(*DuplicateFile));
    return !ValidatePackageDefinitionIds(Files, Diagnostics)
        && !Diagnostics.empty()
        && Diagnostics.back().Code == "core:diagnostic.definition.entry.duplicate_id";
}

bool RunContentCoreExtensionSchemaSelfTest()
{
    using namespace GV2ContentCore;

    const FExtensionSchemaBinding Binding(
        "item", 1, "definition_entry", "weather_mod",
        "weather_mod:schema.extension.item.entry.v1",
        "schemas/item_weather_entry_v1.schema.json5");
    std::vector<FDiagnostic> Diagnostics;
    auto Document = ParseJson5Document(
        "{ id: 'weather_mod:schema.extension.item.entry.v1', definition_type: 'item', "
        "schema_version: 1, extension_site: 'definition_entry', extension_namespace: 'weather_mod', "
        "root: { kind: 'object', fields: { enabled: { kind: 'bool', default: false }, "
        "linked_item: { kind: 'ref', target_kind: 'item' } } } }",
        FParseLimits{}, Diagnostics);
    if (!Document.has_value()) return false;
    auto Resource = ParseExtensionSchemaResource(
        *Document, Binding, "weather_mod", 1, Binding.GetRelativePath(), Diagnostics);
    if (!Resource.has_value() || !Diagnostics.empty()) return false;
    FExtensionSchemaRegistry Registry;
    if (!Registry.Register(std::move(*Resource), Diagnostics)) return false;

    FValidationDiagnosticContext Context;
    Context.PackageId = "weather_mod";
    Context.PackageLoadIndex = 1;
    Context.DefinitionId = "weather_mod:item.test";
    FValue MaterializedExtensions;
    if (!ValidateExtensionBlocks(
            FValue::MakeObject({
                { "weather_mod", FValue::MakeObject() }
            }), MaterializedExtensions,
            Registry, "item", 1, EExtensionSite::DefinitionEntry, "weather_mod",
            nullptr, "/extensions", Context, Diagnostics)) return false;
    const FValue* MaterializedBlock = MaterializedExtensions.FindField("weather_mod");
    if (MaterializedBlock == nullptr
        || MaterializedBlock->FindField("enabled") == nullptr
        || MaterializedBlock->FindField("enabled")->AsBoolean()) return false;

    Diagnostics.clear();
    if (ValidateExtensionBlocks(
            FValue::MakeObject({ { "core", FValue::MakeObject() } }),
            MaterializedExtensions,
            Registry, "item", 1, EExtensionSite::DefinitionEntry, "weather_mod",
            nullptr, "/extensions", Context, Diagnostics)
        || Diagnostics.empty()
        || Diagnostics.front().Code != "core:diagnostic.extension.block.foreign_namespace") return false;

    FMemoryContentSourceProvider Provider;
    Provider.Sources.emplace("core/schemas/item.json5",
        "{id:'core:schema.item.v1',definition_type:'item',schema_version:1,"
        "root:{kind:'object',fields:{}},semantic_validators:[],extensions:{}}");
    Provider.Sources.emplace(
        "weather_mod/schemas/item_weather_entry_v1.schema.json5",
        "{ id: 'weather_mod:schema.extension.item.entry.v1', definition_type: 'item', "
        "schema_version: 1, extension_site: 'definition_entry', extension_namespace: 'weather_mod', "
        "root: { kind: 'object', fields: { linked_item: { kind: 'ref', "
        "target_kind: 'item', required: true } } } }");
    Provider.Sources.emplace("weather_mod/definitions/items.json5",
        "{schema_version:1,type:'item',definitions:[{id:'weather_mod:item.one',data:{},"
        "extensions:{weather_mod:{linked_item:'weather_mod:item.missing'}}}]}");
    FBuildOptions Options;
    Options.SourceProvider = &Provider;
    const FBuildResult MissingExtensionReference = BuildRepository({
        FPackageDescriptor("core", "core", 0, {}, {
            FSchemaBinding("item", 1, "core:schema.item.v1", "schemas/item.json5") }),
        FPackageDescriptor("weather_mod", "weather_mod", 1, { "definitions/items.json5" }, {}, {
            Binding }),
    }, Options);
    return MissingExtensionReference.IsFailure()
        && MissingExtensionReference.GetDiagnostics().front().Code
            == "core:diagnostic.reference.target_missing"
        && MissingExtensionReference.GetDiagnostics().front().SchemaId
            == std::optional<std::string>(Binding.GetSchemaId());
}

bool RunContentCoreJson5LexerSelfTest()
{
    using namespace GV2ContentCore;

    FParseLimits Limits;
    std::string_view Input = "{ a: 10, b: \"test\" }";
    std::vector<FJson5Token> Tokens;
    std::vector<FDiagnostic> Diags;

    if (!LexJson5(Input, Limits, Tokens, Diags) || !Diags.empty()) return false;
    if (Tokens.size() != 10) return false;

    // Unclosed string
    Tokens.clear(); Diags.clear();
    if (LexJson5("{\"a\": \"unclosed}", Limits, Tokens, Diags) || Diags.empty()) return false;

    Tokens.clear(); Diags.clear();
    if (!LexJson5("{ name: '\xD0\xAF', next: 1 }", Limits, Tokens, Diags)
        || Tokens.size() <= 5
        || Tokens[5].Span.StartColumn != 14) return false;

    return true;
}

bool RunContentCoreParseLimitsSelfTest()
{
    using namespace GV2ContentCore;

    FParseLimits Limits;
    Limits.MaxFileSizeBytes = 50;

    std::vector<FDiagnostic> Diags;
    if (!ValidateUtf8AndLimits("{\"a\": 1}", Limits, Diags).has_value() || !Diags.empty()) return false;

    // Invalid UTF-8
    Diags.clear();
    if (ValidateUtf8AndLimits("\xFF\xFF", Limits, Diags).has_value() || Diags.empty()) return false;

    // Limit file size
    Diags.clear();
    if (ValidateUtf8AndLimits(std::string(100, 'x'), Limits, Diags).has_value() || Diags.empty()) return false;

    FParseLimits UnsafeLimits;
    UnsafeLimits.MaxNestingDepth = FParseLimits::MaxSupportedNestingDepth + 1;
    Diags.clear();
    if (ValidateUtf8AndLimits("{}", UnsafeLimits, Diags).has_value()
        || Diags.empty()
        || Diags.front().Code != "core:diagnostic.json5.limit.invalid_configuration") return false;

    return true;
}

bool RunContentCorePackageSelfTest()
{
    using namespace GV2ContentCore;

    const FPackageDescriptor Core("core", "core", 0);
    const FPackageDescriptor Mod("mod", "mod", 1);

    if (!ValidatePackageDescriptors({ Core, Mod }).empty()) return false;
    if (ValidatePackageDescriptors({ Mod }).empty()) return false; // Missing core
    if (ValidatePackageDescriptors({ Core, Core }).empty()) return false; // Duplicate ID
    const FPackageDescriptor InvalidRedirects(
        "core", "core", 0, {}, {}, {},
        {
            FRedirectDescriptor("mod:item.foreign", "core:item.target"),
            FRedirectDescriptor("core:item.old", "core:screen.wrong_kind"),
            FRedirectDescriptor("core:item.conflict", "core:item.target"),
        },
        { "core:item.conflict" });
    const auto RedirectDiagnostics = ValidatePackageDescriptors({ InvalidRedirects });
    const auto HasRedirectCode = [&RedirectDiagnostics](const std::string_view Code)
    {
        return std::any_of(RedirectDiagnostics.begin(), RedirectDiagnostics.end(),
            [Code](const FDiagnostic& Diagnostic) { return Diagnostic.Code == Code; });
    };
    if (!HasRedirectCode("core:diagnostic.repository.redirect.foreign_source")
        || !HasRedirectCode("core:diagnostic.repository.redirect.kind_mismatch")
        || !HasRedirectCode("core:diagnostic.repository.redirect.conflict")) return false;

    const FBuildResult EmptyBuild = BuildRepository({ Core });
    if (!EmptyBuild.IsSuccess()
        || EmptyBuild.GetCandidate().GetStage() != ECandidateStage::RepositoryResolved
        || !EmptyBuild.GetCandidate().IsPublishable()
        || !EmptyBuild.GetCandidate().GetRootValue().IsObject()
        || EmptyBuild.GetCandidate().GetRootValue().FindField("definitions") == nullptr
        || !EmptyBuild.GetCandidate().GetRootValue().FindField("definitions")->AsArray().empty()) return false;

    const FBuildResult InvalidBuild = BuildRepository({ Mod });
    if (!InvalidBuild.IsFailure() || InvalidBuild.GetDiagnostics().empty()) return false;

    FMemoryContentSourceProvider Provider;
    Provider.Sources.emplace(
        "core/definitions/items.json5",
        "{ schema_version: 1, type: 'item', definitions: [] }");
    Provider.Sources.emplace(
        "core/schemas/item.schema.json5",
        "{ id: 'core:schema.item', definition_type: 'item', schema_version: 1, "
        "root: { kind: 'object', fields: {} }, semantic_validators: [], extensions: {} }");
    FBuildOptions Options;
    Options.SourceProvider = &Provider;
    const FPackageDescriptor ParsedCore(
        "core",
        "core",
        0,
        { "definitions/items.json5" },
        { FSchemaBinding("item", 1, "core:schema.item", "schemas/item.schema.json5") });
    const FBuildResult ParsedBuild = BuildRepository({ ParsedCore }, Options);
    if (!ParsedBuild.IsSuccess()
        || ParsedBuild.GetCandidate().GetStage() != ECandidateStage::RepositoryResolved
        || !ParsedBuild.GetCandidate().IsPublishable()
        || ParsedBuild.GetCandidate().GetRootValue().FindField("definitions") == nullptr
        || Provider.Reads.size() != 2) return false;

    FMemoryContentSourceProvider InvalidProvider;
    InvalidProvider.Sources.emplace("core/definitions/items.json5", "{ broken:");
    FBuildOptions InvalidOptions;
    InvalidOptions.SourceProvider = &InvalidProvider;
    const FBuildResult InvalidJsonBuild = BuildRepository(
        { FPackageDescriptor("core", "core", 0, { "definitions/items.json5" }) },
        InvalidOptions);
    if (!InvalidJsonBuild.IsFailure()
        || InvalidJsonBuild.GetDiagnostics().front().Code != "core:diagnostic.json5.unexpected_eof") return false;

    return true;
}

bool RunContentCoreBuildResultSelfTest()
{
    using namespace GV2ContentCore;

    FCandidate Candidate(
        FValue::MakeObject({ { "test", FValue(true) } }),
        ECandidateStage::RepositoryResolved);
    FBuildResult SuccessRes = FBuildResult::Success(Candidate);

    if (!SuccessRes.IsSuccess() || SuccessRes.IsFailure()) return false;
    if (SuccessRes.GetCandidate() != Candidate) return false;

    try
    {
        SuccessRes.GetDiagnostics();
        return false;
    }
    catch (const std::logic_error&)
    {
        // Expected
    }

    FDiagnostic Diag1; Diag1.Code = "core:diagnostic.test.b"; Diag1.Message = "m";
    FDiagnostic Diag2; Diag2.Code = "core:diagnostic.test.a"; Diag2.Message = "m";

    FBuildResult FailureRes = FBuildResult::Failure({ Diag1, Diag2 });
    if (!FailureRes.IsFailure() || FailureRes.IsSuccess()) return false;
    if (FailureRes.GetDiagnostics().size() != 2) return false;
    if (FailureRes.GetDiagnostics()[0].Code != "core:diagnostic.test.a"
        || FailureRes.GetDiagnostics()[1].Code != "core:diagnostic.test.b") return false;

    try
    {
        FailureRes.GetCandidate();
        return false;
    }
    catch (const std::logic_error&)
    {
        // Expected
    }

    try
    {
        FBuildResult::Failure({});
        return false;
    }
    catch (const std::invalid_argument&)
    {
        // Expected
    }

    return true;
}

bool RunContentCoreDiagnosticSelfTest()
{
    using namespace GV2ContentCore;

    FDiagnostic Diag;
    Diag.Code = "core:diagnostic.test.generic";
    Diag.Severity = EDiagnosticSeverity::Error;
    Diag.Message = "Test diagnostic";

    if (Diag.PackageId.has_value() || Diag.Span.has_value() || Diag.SchemaVersion.has_value()) return false;

    FDiagnostic Diag1 = Diag;
    Diag1.PackageId = "core";
    Diag1.PackageLoadIndex = 1;
    Diag1.SchemaVersion = 2;
    Diag1.Span = FSourceSpan{ 10, 1, 10, 5 };

    FDiagnostic Diag2 = Diag;
    Diag2.PackageId = "core";
    Diag2.PackageLoadIndex = 0;
    Diag2.Span = FSourceSpan{ 2, 1, 2, 5 };

    std::vector<FDiagnostic> List = { Diag1, Diag2 };
    std::sort(List.begin(), List.end());

    return *List[0].PackageLoadIndex == 0u && *List[1].PackageLoadIndex == 1u;
}

bool RunContentCoreValueSelfTest()
{
    using namespace GV2ContentCore;

    // 1. Null
    FValue NullVal;
    if (!NullVal.IsNull() || NullVal.GetKind() != EValueKind::Null) return false;
    FValue NullVal2(nullptr);
    if (NullVal != NullVal2) return false;

    // 2. Bool
    FValue BoolVal(true);
    if (!BoolVal.IsBoolean() || !BoolVal.AsBoolean()) return false;

    // 3. Int vs Double
    FValue IntVal(static_cast<std::int64_t>(100));
    FValue DoubleVal(100.0);
    if (!IntVal.IsInteger() || IntVal.AsInteger() != 100) return false;
    if (!DoubleVal.IsNumber() || DoubleVal.AsNumber() != 100.0) return false;
    if (IntVal == DoubleVal) return false;

    // 4. Non-finite double check
    try
    {
        FValue NanVal(std::numeric_limits<double>::quiet_NaN());
        return false;
    }
    catch (const std::invalid_argument&)
    {
        // Expected
    }

    // 5. String
    FValue StrVal("hello");
    if (!StrVal.IsString() || StrVal.AsString() != "hello") return false;
    FValue Utf8Str(std::string(reinterpret_cast<const char*>(u8"Привет")));
    if (!Utf8Str.IsString()) return false;
    try
    {
        FValue InvalidUtf8(std::string("\xc0\xaf", 2));
        return false;
    }
    catch (const std::invalid_argument&)
    {
        // Expected
    }

    // 6. Array & Object & Copy/Move
    FValue Arr = FValue::MakeArray({ FValue(1), FValue(2) });
    FValue Obj = FValue::MakeObject({ { "key", FValue("value") } });
    if (Obj.FindField("key") == nullptr || Obj.FindField("key")->AsString() != "value") return false;

    FValue CopyObj = Obj;
    if (CopyObj != Obj) return false;
    FValue MoveObj = std::move(CopyObj);
    if (MoveObj != Obj || !CopyObj.IsNull()) return false;
    CopyObj = FValue("reused");
    if (CopyObj.AsString() != "reused") return false;

    return true;
}

class FMetadataOnlyResourceCatalog final : public GV2RuntimeCore::IResourceCatalog
{
public:
    FMetadataOnlyResourceCatalog()
    {
        GV2RuntimeCore::FResourceMetadata Metadata;
        Metadata.ResourceId = "core:resource.image.headless_fixture";
        Metadata.Kind = GV2RuntimeCore::EResourceKind::Image;
        Metadata.bAvailable = true;
        Resources.emplace(Metadata.ResourceId, std::move(Metadata));
    }

    std::optional<GV2RuntimeCore::FResourceMetadata> FindMetadata(
        const std::string& ResourceId) const override
    {
        const auto Found = Resources.find(ResourceId);
        return Found != Resources.end()
            ? std::optional<GV2RuntimeCore::FResourceMetadata>(Found->second)
            : std::nullopt;
    }

private:
    std::map<std::string, GV2RuntimeCore::FResourceMetadata, std::less<>> Resources;
};

class FUnresolvedLocalizationAdapter final : public GV2RuntimeCore::ILocalizationAdapter
{
public:
    std::optional<std::string> Resolve(
        const GV2RuntimeCore::FTextSpec&,
        const std::string&) const override
    {
        return std::nullopt;
    }
};

bool TryParsePositive(const std::string& Text, std::int64_t& OutValue)
{
    const char* Begin = Text.data();
    const char* End = Begin + Text.size();
    const auto Result = std::from_chars(Begin, End, OutValue);
    return Result.ec == std::errc{} && Result.ptr == End && OutValue > 0;
}

bool LoadRuntimeSources(
    const char* ExecutableArgument,
    std::vector<GV2RuntimeCore::FRuntimeSource>& OutSources)
{
    std::vector<std::filesystem::path> ScriptDirectories{
        std::filesystem::current_path() / "Scripts",
        std::filesystem::current_path() / ".." / "Scripts",
    };
    std::error_code PathError;
    const std::filesystem::path ExecutablePath = std::filesystem::absolute(
        ExecutableArgument,
        PathError);
    if (!PathError)
    {
        ScriptDirectories.push_back(
            ExecutablePath.parent_path().parent_path().parent_path() / "Scripts");
    }

    for (const std::filesystem::path& Directory : ScriptDirectories)
    {
        std::error_code IterationError;
        if (!std::filesystem::is_directory(Directory, IterationError) || IterationError)
        {
            continue;
        }

        std::vector<std::filesystem::path> SourcePaths;
        for (std::filesystem::recursive_directory_iterator Iterator(Directory, IterationError), End;
             !IterationError && Iterator != End;
             Iterator.increment(IterationError))
        {
            if (Iterator->is_regular_file() && Iterator->path().extension() == ".lua")
            {
                SourcePaths.emplace_back(Iterator->path());
            }
        }
        if (IterationError || SourcePaths.empty())
        {
            continue;
        }
        std::sort(SourcePaths.begin(), SourcePaths.end());

        std::vector<GV2RuntimeCore::FRuntimeSource> Candidate;
        Candidate.reserve(SourcePaths.size());
        for (const std::filesystem::path& SourcePath : SourcePaths)
        {
            std::ifstream Stream(SourcePath, std::ios::binary);
            if (!Stream)
            {
                Candidate.clear();
                break;
            }
            std::string Text{
                std::istreambuf_iterator<char>(Stream),
                std::istreambuf_iterator<char>()};
            if (Text.starts_with("\xef\xbb\xbf"))
            {
                Text.erase(0, 3);
            }
            const std::string RelativePath = std::filesystem::relative(SourcePath, Directory).generic_string();
            Candidate.push_back({std::string("@Scripts/") + RelativePath, std::move(Text)});
        }
        if (!Candidate.empty())
        {
            OutSources = std::move(Candidate);
            return true;
        }
    }
    return false;
}

class FHeadlessFilesystemContentSourceProvider final : public GV2ContentCore::IContentSourceProvider
{
public:
    FHeadlessFilesystemContentSourceProvider(std::filesystem::path InPackageRoot, std::string InPackageId)
        : PackageRoot(std::move(InPackageRoot))
        , PackageId(std::move(InPackageId))
    {
    }

    std::optional<std::string> ReadSource(
        const std::string_view RequestedPackageId,
        const std::string_view RelativeSource) const override
    {
        if (RequestedPackageId != PackageId)
        {
            return std::nullopt;
        }
        std::ifstream Stream(PackageRoot / std::string(RelativeSource), std::ios::binary);
        if (!Stream)
        {
            return std::nullopt;
        }
        return std::string{std::istreambuf_iterator<char>(Stream), std::istreambuf_iterator<char>()};
    }

private:
    std::filesystem::path PackageRoot;
    std::string PackageId;
};

std::vector<std::filesystem::path> ListSortedJson5Files(const std::filesystem::path& Dir)
{
    std::vector<std::filesystem::path> Entries;
    std::error_code Ec;
    if (!std::filesystem::is_directory(Dir, Ec) || Ec)
    {
        return Entries;
    }
    for (const auto& Entry : std::filesystem::directory_iterator(Dir, Ec))
    {
        if (Entry.is_regular_file() && Entry.path().extension() == ".json5")
        {
            Entries.push_back(Entry.path());
        }
    }
    std::sort(Entries.begin(), Entries.end());
    return Entries;
}

// PCC-35: builds the single-package pinned repository read handle used
// before Lua bootstrap. Mirrors gv2-content's BuildFromPackageRoot
// (Content/Source/main.cpp) so CLI/headless/UE stay on one discovery
// convention (self-describing schema resources) and the same
// GV2ContentCore::BuildRepository() reference path (PCC-38 parity).
GV2ContentCore::FBuildResult BuildRepositoryFromDirectory(const std::filesystem::path& PackageRoot)
{
    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    std::optional<GV2ContentCore::FPackageDescriptor> Descriptor =
        GV2ContentCore::DiscoverPackageFromDirectory(PackageRoot, Diagnostics);
    if (!Descriptor)
    {
        return GV2ContentCore::FBuildResult::Failure(std::move(Diagnostics));
    }

    FHeadlessFilesystemContentSourceProvider Provider(PackageRoot, Descriptor->GetPackageId());
    GV2ContentCore::FBuildOptions Options;
    Options.SourceProvider = &Provider;

    return GV2ContentCore::BuildRepository({*Descriptor}, Options);
}

std::optional<std::filesystem::path> LoadContentRoot(
    const char* ExecutableArgument,
    const std::optional<std::string>& ExplicitRoot)
{
    if (ExplicitRoot)
    {
        return std::filesystem::path(*ExplicitRoot);
    }

    std::vector<std::filesystem::path> CandidateRoots{
        std::filesystem::current_path() / "GameData" / "core",
        std::filesystem::current_path() / ".." / "GameData" / "core",
    };
    std::error_code PathError;
    const std::filesystem::path ExecutablePath = std::filesystem::absolute(ExecutableArgument, PathError);
    if (!PathError)
    {
        CandidateRoots.push_back(
            ExecutablePath.parent_path().parent_path().parent_path()
            / "GameData" / "core");
    }

    for (const std::filesystem::path& Candidate : CandidateRoots)
    {
        std::error_code CandidateError;
        if (std::filesystem::is_directory(Candidate, CandidateError) && !CandidateError)
        {
            return Candidate;
        }
    }
    return std::nullopt;
}

void PrintRepositoryDiagnostics(const std::vector<GV2ContentCore::FDiagnostic>& Diagnostics)
{
    for (const GV2ContentCore::FDiagnostic& Diagnostic : Diagnostics)
    {
        std::cerr << (Diagnostic.Severity == GV2ContentCore::EDiagnosticSeverity::Error ? "error" : "warning")
                   << " " << Diagnostic.Code;
        if (Diagnostic.RelativeSource)
        {
            std::cerr << " " << *Diagnostic.RelativeSource;
            if (Diagnostic.Span)
            {
                std::cerr << ":" << Diagnostic.Span->StartLine << ":" << Diagnostic.Span->StartColumn;
            }
        }
        std::cerr << " " << Diagnostic.Message << "\n";
    }
}

bool RunSharedJson5FixtureConformance()
{
    namespace Fs = std::filesystem;
    using GV2ContentCore::Testing::FJson5Fixture;

    const std::vector<Fs::path> CandidateRoots{
        Fs::current_path() / "Tests" / "Fixtures" / "PortableContentCore",
        Fs::current_path() / ".." / "Tests" / "Fixtures" / "PortableContentCore",
    };

    Fs::path FixtureRoot;
    for (const Fs::path& Candidate : CandidateRoots)
    {
        if (Fs::is_regular_file(Candidate / "fixtures.index"))
        {
            FixtureRoot = Candidate;
            break;
        }
    }
    if (FixtureRoot.empty()) return false;

    std::ifstream IndexStream(FixtureRoot / "fixtures.index");
    if (!IndexStream) return false;

    std::vector<FJson5Fixture> Fixtures;
    std::string RelativePath;
    while (std::getline(IndexStream, RelativePath))
    {
        if (!RelativePath.empty() && RelativePath.back() == '\r') RelativePath.pop_back();
        if (RelativePath.empty() || RelativePath.front() == '#') continue;

        std::ifstream SourceStream(FixtureRoot / RelativePath, std::ios::binary);
        if (!SourceStream) return false;
        FJson5Fixture Fixture;
        Fixture.RelativePath = RelativePath;
        Fixture.Source.assign(
            std::istreambuf_iterator<char>(SourceStream),
            std::istreambuf_iterator<char>());
        if (RelativePath.starts_with("invalid/duplicate_key/"))
        {
            Fixture.ExpectedDiagnosticCode = "core:diagnostic.json5.duplicate_key";
        }
        Fixtures.push_back(std::move(Fixture));
    }

    std::string Failure;
    if (!GV2ContentCore::Testing::RunJson5FixtureConformance(Fixtures, Failure))
    {
        std::cerr << Failure << '\n';
        return false;
    }

    class FFixtureProvider final : public GV2ContentCore::IContentSourceProvider
    {
    public:
        std::map<std::string, Fs::path> PackageRoots;
        std::map<std::string, std::string> InlineSources;

        std::optional<std::string> ReadSource(
            const std::string_view PackageId,
            const std::string_view RelativeSource) const override
        {
            const std::string Key = std::string(PackageId) + "/" + std::string(RelativeSource);
            if (const auto Inline = InlineSources.find(Key); Inline != InlineSources.end())
            {
                return Inline->second;
            }
            const auto Root = PackageRoots.find(std::string(PackageId));
            if (Root == PackageRoots.end()) return std::nullopt;
            std::ifstream Stream(
                Root->second / std::string(RelativeSource),
                std::ios::binary);
            if (!Stream) return std::nullopt;
            return std::string{
                std::istreambuf_iterator<char>(Stream),
                std::istreambuf_iterator<char>()};
        }

    };

    FFixtureProvider Provider;
    Provider.PackageRoots.emplace("core", FixtureRoot / "valid" / "core");
    Provider.PackageRoots.emplace("test_mod", FixtureRoot / "valid" / "test_mod");
    GV2ContentCore::FBuildOptions Options;
    Options.SourceProvider = &Provider;
    const GV2ContentCore::FBuildResult CoreBuild = GV2ContentCore::BuildRepository(
        { GV2ContentCore::Testing::MakeRepresentativeCorePackageDescriptor() }, Options);
    if (!CoreBuild.IsSuccess()) return false;
    const GV2ContentCore::FValue* Definitions =
        CoreBuild.GetCandidate().GetRootValue().FindField("definitions");
    if (Definitions == nullptr || !Definitions->IsArray()
        || Definitions->AsArray().size() != 9) return false;

    const auto Core = GV2ContentCore::Testing::MakeRepresentativeCorePackageDescriptor();
    const auto TestMod = GV2ContentCore::Testing::MakeRepresentativeTestModPackageDescriptor();
    const GV2ContentCore::FBuildResult ModBuild = GV2ContentCore::BuildRepository(
        { Core, TestMod }, Options);
    if (!ModBuild.IsSuccess())
    {
        std::cerr << "representative M4 build failed: "
            << ModBuild.GetDiagnostics().front().Code << '\n';
        return false;
    }
    if (ModBuild.GetCandidate().GetStage()
        != GV2ContentCore::ECandidateStage::RepositoryResolved) return false;
    Definitions = ModBuild.GetCandidate().GetRootValue().FindField("definitions");
    if (Definitions == nullptr || Definitions->AsArray().size() != 11) return false;
    const GV2ContentCore::FValue* Inventory = nullptr;
    for (const GV2ContentCore::FValue& Definition : Definitions->AsArray())
    {
        if (Definition.FindField("id")->AsString() == "core:screen.inventory") Inventory = &Definition;
    }
    if (Inventory == nullptr
        || Inventory->FindField("tags")->AsArray().size() != 1
        || Inventory->FindField("tags")->AsArray()[0].AsString() != "test_mod_override"
        || Inventory->FindField("deprecated")->AsBoolean()) return false;
    const GV2ContentCore::FRepositoryReadHandle Handle = ModBuild.GetCandidate().GetReadHandle();
    if (!Handle.IsValid() || Handle.GetContentHash().size() != 64) return false;
    const auto RedirectId = GV2ContentCore::FDefinitionId::Require(
        "test_mod:screen.codex_archive");
    const auto CanonicalId = GV2ContentCore::FDefinitionId::Require(
        "test_mod:screen.codex_lab");
    if (Handle.Find(RedirectId) != Handle.Find(CanonicalId)) return false;
    const auto* RedirectProvenance = Handle.GetProvenance(RedirectId);
    if (RedirectProvenance == nullptr
        || RedirectProvenance->CanonicalId != "test_mod:screen.codex_lab"
        || RedirectProvenance->RedirectChain.size() != 3) return false;
    const auto* InventoryProvenance = Handle.GetProvenance(
        GV2ContentCore::FDefinitionId::Require("core:screen.inventory"));
    if (InventoryProvenance == nullptr
        || InventoryProvenance->Winner.PackageId != "test_mod"
        || InventoryProvenance->ShadowedProviders.size() != 1
        || InventoryProvenance->ShadowedProviders[0].PackageId != "core"
        || InventoryProvenance->Winner.SchemaId != "core:schema.definition.screen.v1"
        || InventoryProvenance->Winner.SchemaVersion != 1
        || InventoryProvenance->Winner.SourceSpan.StartLine <= 1) return false;
    const auto Screens = Handle.List("screen");
    if (Screens.size() != 3
        || Screens[0].ToString() != "core:screen.inventory"
        || Screens[2].ToString() != "test_mod:screen.codex_lab") return false;
    const auto Tombstoned = Handle.Require(GV2ContentCore::FDefinitionId::Require(
        "test_mod:screen.retired"));
    if (Tombstoned || !Tombstoned.Error.has_value()
        || Tombstoned.Error->Code != "core:diagnostic.repository.read.tombstoned") return false;
    const GV2ContentCore::FRepositoryReadHandle InvalidHandle;
    const auto InvalidHandleResult = InvalidHandle.Require(
        GV2ContentCore::FDefinitionId::Require("core:screen.inventory"));
    if (InvalidHandleResult || !InvalidHandleResult.Error.has_value()
        || InvalidHandleResult.Error->Code
            != "core:diagnostic.repository.read.invalid_handle") return false;
    bool bInvalidHashThrows = false;
    try
    {
        (void)InvalidHandle.GetContentHash();
    }
    catch (const std::logic_error&)
    {
        bInvalidHashThrows = true;
    }
    if (!bInvalidHashThrows) return false;
    const GV2ContentCore::FValue* Location = Handle.Find(
        GV2ContentCore::FDefinitionId::Require("core:location.city.market"));
    if (Location == nullptr
        || Location->FindField("data")->FindField("screen_ids")->AsArray()[0].AsString()
            != "test_mod:screen.codex_lab") return false;

    const GV2ContentCore::FPackageDescriptor FilePermutedMod(
        "test_mod", "test_mod", 1,
        { "definitions/texts.json5", "definitions/screens.json5", "definitions/locations.json5" }, {}, {},
        TestMod.GetRedirects(), TestMod.GetTombstones());
    const GV2ContentCore::FBuildResult FilePermuted = GV2ContentCore::BuildRepository(
        { FilePermutedMod, Core }, Options);
    if (!FilePermuted.IsSuccess()
        || FilePermuted.GetCandidate() != ModBuild.GetCandidate()
        || FilePermuted.GetCandidate().GetReadHandle().GetContentHash()
            != Handle.GetContentHash()) return false;

    FFixtureProvider EntryPermutedProvider = Provider;
    EntryPermutedProvider.InlineSources["test_mod/definitions/screens.json5"] = R"json5(
    {
      schema_version: 1,
      type: "screen",
      definitions: [
        {
          id: "test_mod:screen.codex_lab",
          data: { title_text_id: "test_mod:text.screen.codex_lab.title" },
        },
        {
          id: "core:screen.inventory",
          data: { title_text_id: "core:text.screen.inventory.title" },
          tags: ["test_mod_override"],
        },
      ],
    })json5";
    GV2ContentCore::FBuildOptions EntryPermutedOptions;
    EntryPermutedOptions.SourceProvider = &EntryPermutedProvider;
    const GV2ContentCore::FBuildResult EntryPermuted = GV2ContentCore::BuildRepository(
        { Core, TestMod }, EntryPermutedOptions);
    if (!EntryPermuted.IsSuccess()
        || EntryPermuted.GetCandidate() != ModBuild.GetCandidate()) return false;

    FFixtureProvider NumericSpellingProvider = Provider;
    NumericSpellingProvider.InlineSources["core/definitions/items.json5"] = R"json5(
    { type: "item", definitions: [{
      extensions: {}, deprecated: false, tags: ["weapon", "melee"],
      data: { icon_resource_id: "core:resource.item.iron_sword.icon",
        price: 0xa, label_text_id: "core:text.item.iron_sword.name" },
      id: "core:item.weapon.iron_sword",
    }], schema_version: 1, extensions: {} })json5";
    GV2ContentCore::FBuildOptions NumericSpellingOptions;
    NumericSpellingOptions.SourceProvider = &NumericSpellingProvider;
    const auto NumericSpelling = GV2ContentCore::BuildRepository(
        { Core, TestMod }, NumericSpellingOptions);
    if (!NumericSpelling.IsSuccess()
        || NumericSpelling.GetCandidate().GetReadHandle().GetContentHash()
            != Handle.GetContentHash()) return false;
    if (Handle.GetContentHash()
        != "7e74328e50a01e9dc44982a86c37a63eaae751b40e288eecb6c364d4124121a1") return false;

    FFixtureProvider ChangedActiveProvider = Provider;
    ChangedActiveProvider.InlineSources["core/definitions/items.json5"] = R"json5(
    { schema_version: 1, type: "item", definitions: [{
      id: "core:item.weapon.iron_sword",
      data: { label_text_id: "core:text.item.iron_sword.name", price: 11,
        icon_resource_id: "core:resource.item.iron_sword.icon" },
      tags: ["weapon", "melee"], deprecated: false, extensions: {},
    }], extensions: {} })json5";
    GV2ContentCore::FBuildOptions ChangedActiveOptions;
    ChangedActiveOptions.SourceProvider = &ChangedActiveProvider;
    const auto ChangedActive = GV2ContentCore::BuildRepository(
        { Core, TestMod }, ChangedActiveOptions);
    if (!ChangedActive.IsSuccess()
        || ChangedActive.GetCandidate().GetReadHandle().GetContentHash()
            == Handle.GetContentHash()) return false;

    const GV2ContentCore::FPackageDescriptor ReindexedMod(
        "test_mod", "test_mod", 2, TestMod.GetRelativeSources(),
        TestMod.GetSchemaBindings(), TestMod.GetExtensionSchemaBindings(),
        TestMod.GetRedirects(), TestMod.GetTombstones());
    const auto Reindexed = GV2ContentCore::BuildRepository({ Core, ReindexedMod }, Options);
    if (!Reindexed.IsSuccess()
        || Reindexed.GetCandidate().GetReadHandle().GetContentHash()
            == Handle.GetContentHash()) return false;

    std::vector<std::string> ExtendedTombstones = TestMod.GetTombstones();
    ExtendedTombstones.push_back("test_mod:item.retired");
    const GV2ContentCore::FPackageDescriptor AdditionalRetirementMod(
        "test_mod", "test_mod", 1, TestMod.GetRelativeSources(),
        TestMod.GetSchemaBindings(), TestMod.GetExtensionSchemaBindings(),
        TestMod.GetRedirects(), std::move(ExtendedTombstones));
    const auto AdditionalRetirement = GV2ContentCore::BuildRepository(
        { Core, AdditionalRetirementMod }, Options);
    if (!AdditionalRetirement.IsSuccess()
        || AdditionalRetirement.GetCandidate().GetReadHandle().GetContentHash()
            == Handle.GetContentHash()) return false;
    const auto Repeated = GV2ContentCore::BuildRepository({ Core, TestMod }, Options);
    if (!Repeated.IsSuccess() || Repeated.GetCandidate() != ModBuild.GetCandidate()) return false;

    FDuplicateCoreValidator DuplicateCoreValidator;
    GV2ContentCore::FSemanticValidatorRegistry HostValidators;
    std::vector<GV2ContentCore::FDiagnostic> RegistryDiagnostics;
    if (!HostValidators.Register(DuplicateCoreValidator, RegistryDiagnostics)) return false;
    GV2ContentCore::FBuildOptions DuplicateValidatorOptions = Options;
    DuplicateValidatorOptions.SemanticValidatorRegistry = &HostValidators;
    const auto DuplicateValidatorBuild = GV2ContentCore::BuildRepository(
        { Core, TestMod }, DuplicateValidatorOptions);
    if (DuplicateValidatorBuild.IsSuccess()
        || DuplicateValidatorBuild.GetDiagnostics().front().Code
            != "core:diagnostic.semantic.validator.duplicate_id") return false;

    const auto RunInvalid = [&](const std::string& Group, const GV2ContentCore::FPackageDescriptor& Mod)
    {
        FFixtureProvider InvalidProvider;
        InvalidProvider.PackageRoots.emplace("core", FixtureRoot / "valid" / "core");
        InvalidProvider.PackageRoots.emplace("test_mod", FixtureRoot / "invalid" / Group / "test_mod");
        GV2ContentCore::FBuildOptions InvalidOptions;
        InvalidOptions.SourceProvider = &InvalidProvider;
        return GV2ContentCore::BuildRepository({ Core, Mod }, InvalidOptions);
    };
    const auto SingleSourceMod = [](const std::string& Source)
    {
        return GV2ContentCore::FPackageDescriptor("test_mod", "test_mod", 1, { Source });
    };
    const auto CheckFailure = [](const GV2ContentCore::FBuildResult& Result, const std::string& Code)
    {
        return Result.IsFailure() && !Result.GetDiagnostics().empty()
            && Result.GetDiagnostics().front().Code == Code;
    };
    if (!CheckFailure(
            RunInvalid("foreign_new_id", SingleSourceMod("definitions/screens.json5")),
            "core:diagnostic.repository.identity.foreign_new_id")) return false;
    if (!CheckFailure(
            RunInvalid("broken_override", SingleSourceMod("definitions/screens.json5")),
            "core:diagnostic.schema.value.missing_required_field")) return false;
    if (!CheckFailure(
            RunInvalid("missing_reference", SingleSourceMod("definitions/screens.json5")),
            "core:diagnostic.reference.target_missing")) return false;
    if (!CheckFailure(
            RunInvalid("semantic_failure", SingleSourceMod("definitions/items.json5")),
            "core:diagnostic.semantic.item.price_not_positive")) return false;
    const GV2ContentCore::FPackageDescriptor ClassMod(
        "test_mod", "test_mod", 1,
        { "definitions/items.json5", "definitions/resources.json5" },
        { GV2ContentCore::FSchemaBinding(
            "resource", 2, "test_mod:schema.definition.resource.v2",
            "schemas/resource_v2.schema.json5") });
    if (!CheckFailure(
            RunInvalid("resource_class_mismatch", ClassMod),
            "core:diagnostic.reference.resource_class_mismatch")) return false;
    const GV2ContentCore::FPackageDescriptor CycleMod(
        "test_mod", "test_mod", 1, { "definitions/screens.json5" }, {}, {},
        {
            GV2ContentCore::FRedirectDescriptor("test_mod:screen.a", "test_mod:screen.b"),
            GV2ContentCore::FRedirectDescriptor("test_mod:screen.b", "test_mod:screen.a"),
        });
    if (!CheckFailure(
            RunInvalid("redirect_cycle", CycleMod),
            "core:diagnostic.repository.redirect.cycle")) return false;
    const GV2ContentCore::FPackageDescriptor MissingRedirectTargetMod(
        "test_mod", "test_mod", 1, { "definitions/screens.json5" }, {}, {},
        { GV2ContentCore::FRedirectDescriptor(
            "test_mod:screen.old", "test_mod:screen.missing") });
    if (!CheckFailure(
            RunInvalid("redirect_cycle", MissingRedirectTargetMod),
            "core:diagnostic.repository.redirect.target_missing")) return false;
    const GV2ContentCore::FPackageDescriptor TombstonedRedirectTargetMod(
        "test_mod", "test_mod", 1, { "definitions/screens.json5" }, {}, {},
        { GV2ContentCore::FRedirectDescriptor(
            "test_mod:screen.old", "test_mod:screen.removed") },
        { "test_mod:screen.removed" });
    if (!CheckFailure(
            RunInvalid("redirect_cycle", TombstonedRedirectTargetMod),
            "core:diagnostic.repository.redirect.target_tombstoned")) return false;
    const GV2ContentCore::FPackageDescriptor ActiveRedirectMod(
        "test_mod", "test_mod", 1, { "definitions/screens.json5" }, {}, {},
        { GV2ContentCore::FRedirectDescriptor(
            "test_mod:screen.old_name", "core:screen.main") });
    if (!CheckFailure(
            RunInvalid("active_redirect_source", ActiveRedirectMod),
            "core:diagnostic.repository.redirect.active_source_conflict")) return false;
    const GV2ContentCore::FPackageDescriptor ActiveTombstoneMod(
        "test_mod", "test_mod", 1, { "definitions/screens.json5" }, {}, {}, {},
        { "test_mod:screen.old_name" });
    if (!CheckFailure(
            RunInvalid("active_redirect_source", ActiveTombstoneMod),
            "core:diagnostic.repository.tombstone.active_definition_conflict")) return false;
    return true;
}

int Run(
    const std::int64_t CommandCount,
    const std::int64_t Seed,
    const bool bSelfTest,
    const std::vector<GV2RuntimeCore::FRuntimeSource>& RuntimeSources,
    const std::optional<std::filesystem::path>& ContentRoot)
{
    if (!RunContentCoreValueSelfTest())
    {
        std::cerr << "pcc_value_model_self_test_failed\n";
        return 1;
    }
    if (!RunContentCoreDiagnosticSelfTest())
    {
        std::cerr << "pcc_diagnostic_model_self_test_failed\n";
        return 1;
    }
    if (!RunContentCoreBuildResultSelfTest())
    {
        std::cerr << "pcc_build_result_self_test_failed\n";
        return 1;
    }
    if (!RunContentCorePackageSelfTest())
    {
        std::cerr << "pcc_package_descriptor_self_test_failed\n";
        return 1;
    }
    if (!RunContentCoreParseLimitsSelfTest())
    {
        std::cerr << "pcc_parse_limits_self_test_failed\n";
        return 1;
    }
    if (!RunContentCoreJson5LexerSelfTest())
    {
        std::cerr << "pcc_json5_lexer_self_test_failed\n";
        return 1;
    }
    if (!RunContentCoreJson5ParserSelfTest())
    {
        std::cerr << "pcc_json5_parser_self_test_failed\n";
        return 1;
    }
    if (!RunContentCoreSchemaRegistrySelfTest())
    {
        std::cerr << "pcc_schema_registry_self_test_failed\n";
        return 1;
    }
    if (!RunContentCoreScalarValidationSelfTest())
    {
        std::cerr << "pcc_scalar_validation_self_test_failed\n";
        return 1;
    }
    if (!RunContentCoreContainerValidationSelfTest())
    {
        std::cerr << "pcc_container_validation_self_test_failed\n";
        return 1;
    }
    if (!RunContentCorePresenceDefaultSelfTest())
    {
        std::cerr << "pcc_presence_default_self_test_failed\n";
        return 1;
    }
    if (!RunContentCoreSpecialFieldSelfTest())
    {
        std::cerr << "pcc_special_field_self_test_failed\n";
        return 1;
    }
    if (!RunContentCoreDefinitionEnvelopeSelfTest())
    {
        std::cerr << "pcc_definition_envelope_self_test_failed\n";
        return 1;
    }
    if (!RunContentCoreExtensionSchemaSelfTest())
    {
        std::cerr << "pcc_extension_schema_self_test_failed\n";
        return 1;
    }
    if (!RunSharedJson5FixtureConformance())
    {
        std::cerr << "pcc_shared_json5_fixture_conformance_failed\n";
        return 1;
    }

    // PCC-35: pin a repository read handle before Lua bootstrap. Missing or
    // invalid repository content blocks session startup entirely.
    if (!ContentRoot)
    {
        std::cerr << "content_root_not_found\n";
        return 9;
    }
    const GV2ContentCore::FBuildResult RepositoryBuild = BuildRepositoryFromDirectory(*ContentRoot);
    if (RepositoryBuild.IsFailure())
    {
        PrintRepositoryDiagnostics(RepositoryBuild.GetDiagnostics());
        std::cerr << "repository_build_failed\n";
        return 9;
    }
    const GV2ContentCore::FRepositoryReadHandle RepositoryHandle =
        RepositoryBuild.GetCandidate().GetReadHandle();
    // Exercise the pinned handle with a portable repository query.
    const std::size_t RepositoryItemCount = RepositoryHandle.List("item").size();

    GV2RuntimeCore::FRuntimeSession Runtime;
    GV2RuntimeCore::FRuntimeFault Fault;
    if (!Runtime.Start(1, RuntimeSources, Fault))
    {
        std::cerr << "runtime_start_failed code=" << Fault.Code << " message=" << Fault.Message << '\n';
        return 2;
    }

    FMetadataOnlyResourceCatalog Resources;
    const auto Resource = Resources.FindMetadata("core:resource.image.headless_fixture");
    if (!Resource || !Resource->bAvailable)
    {
        std::cerr << "metadata_resource_catalog_failed\n";
        return 3;
    }

    FUnresolvedLocalizationAdapter Localization;
    GV2RuntimeCore::FTextSpec Text;
    Text.TextId = "core:text.headless.fixture";
    if (Localization.Resolve(Text, "").has_value())
    {
        std::cerr << "headless_localization_should_remain_unresolved\n";
        return 4;
    }

    const auto Started = std::chrono::steady_clock::now();
    for (std::int64_t Index = 1; Index <= CommandCount; ++Index)
    {
        GV2RuntimeCore::FCommandRequest Request;
        Request.CommandId = "core:command.test.headless_step";
        Request.Sequence = Index;
        Request.Args.emplace("seed", GV2RuntimeCore::FValue(Seed));
        Request.Args.emplace("step", GV2RuntimeCore::FValue(Index));
        if (!Runtime.DispatchCommand(Request, Fault))
        {
            std::cerr << "command_failed sequence=" << Index
                      << " code=" << Fault.Code
                      << " message=" << Fault.Message << '\n';
            return 5;
        }
    }
    const auto Finished = std::chrono::steady_clock::now();
    const double Seconds = std::chrono::duration<double>(Finished - Started).count();
    const double CommandsPerSecond = Seconds > 0.0
        ? static_cast<double>(CommandCount) / Seconds
        : 0.0;

    if (bSelfTest)
    {
        const std::string StableIdFailure =
            GV2RuntimeCore::Testing::RunStableIdConformance();
        if (!StableIdFailure.empty())
        {
            std::cerr << "stable_id_conformance_failed case=" << StableIdFailure << '\n';
            return 8;
        }

        GV2RuntimeCore::FSemanticInput Input;
        Input.SessionGeneration = 1;
        Input.UiInstanceId = "ui@1:1";
        Input.Revision = 1;
        Input.Sequence = CommandCount + 1;
        Input.NodeKeyPath = {"route", "button"};
        Input.CommandId = "core:command.test.semantic_step";
        if (!Runtime.DispatchSemanticInput(Input, Fault))
        {
            std::cerr << "semantic_input_failed code=" << Fault.Code
                      << " message=" << Fault.Message << '\n';
            return 6;
        }

        GV2RuntimeCore::FCommandRequest StartRequest;
        StartRequest.CommandId = "core:command.debug.start";
        StartRequest.Sequence = CommandCount + 2;
        std::optional<GV2RuntimeCore::FScreenRequest> PendingScreen;
        if (!Runtime.DispatchCommand(StartRequest, Fault)
            || !Runtime.TakePendingScreen(PendingScreen, Fault)
            || !PendingScreen
            || PendingScreen->ScreenId != "core:screen.test"
            || PendingScreen->Fields.size() != 5
            || PendingScreen->Fields[0].FieldId != "buttons"
            || PendingScreen->Fields[1].FieldId != "checkbox"
            || PendingScreen->Fields[2].FieldId != "class_select"
            || PendingScreen->Fields[3].FieldId != "description"
            || PendingScreen->Fields[4].FieldId != "player_name")
        {
            std::cerr << "debug_start_flow_failed code=" << Fault.Code
                      << " message=" << Fault.Message << '\n';
            return 7;
        }
    }

    std::cout << "{\"ok\":true,\"lua_release_num\":"
              << GV2RuntimeCore::FRuntimeSession::LuaReleaseNumber
              << ",\"commands\":" << CommandCount
              << ",\"seed\":" << Seed
              << ",\"commands_per_second\":" << CommandsPerSecond
              << ",\"repository_content_hash\":\"" << RepositoryHandle.GetContentHash() << "\""
              << ",\"repository_item_count\":" << RepositoryItemCount
              << ",\"media_payload_loaded\":false"
              << ",\"localization_resolved\":false}\n";
    return 0;
}
}

int main(int argc, char** argv)
{
    std::int64_t CommandCount = 1000;
    std::int64_t Seed = 1;
    bool bSelfTest = false;
    std::optional<std::string> ExplicitContentRoot;

    for (int Index = 1; Index < argc; ++Index)
    {
        const std::string Argument = argv[Index];
        if (Argument == "--self-test")
        {
            bSelfTest = true;
        }
        else if (Argument.rfind("--commands=", 0) == 0)
        {
            if (!TryParsePositive(Argument.substr(11), CommandCount))
            {
                std::cerr << "invalid --commands value\n";
                return 64;
            }
        }
        else if (Argument.rfind("--seed=", 0) == 0)
        {
            if (!TryParsePositive(Argument.substr(7), Seed))
            {
                std::cerr << "invalid --seed value\n";
                return 64;
            }
        }
        else if (Argument.rfind("--content-root=", 0) == 0)
        {
            ExplicitContentRoot = Argument.substr(15);
        }
        else
        {
            std::cerr << "unknown argument: " << Argument << '\n';
            return 64;
        }
    }

    std::vector<GV2RuntimeCore::FRuntimeSource> RuntimeSources;
    if (!LoadRuntimeSources(argv[0], RuntimeSources))
    {
        std::cerr << "unable to locate a non-empty Scripts module tree\n";
        return 66;
    }
    const std::optional<std::filesystem::path> ContentRoot = LoadContentRoot(argv[0], ExplicitContentRoot);
    return Run(CommandCount, Seed, bSelfTest, RuntimeSources, ContentRoot);
}
