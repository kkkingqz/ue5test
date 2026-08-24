#include "GV2ContentCore/Testing/SchemaRegistryConformance.h"

#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"
#include "GV2ContentCore/RepositoryBuilder.h"
#include "GV2ContentCore/SchemaRegistry.h"
#include "GV2ContentCore/SemanticValidation.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace GV2ContentCore::Testing
{
namespace
{
class FMemoryContentSourceProvider final : public IContentSourceProvider
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

class FDuplicateValidator final : public ISemanticValidator
{
public:
    std::string_view GetId() const override
    {
        return "core:validator.item.positive_price";
    }

    void Validate(
        const FValue&,
        const FSemanticCandidateView&,
        const FSemanticValidationContext&,
        std::vector<FDiagnostic>&) const override
    {
    }
};
} // namespace

std::string RunSchemaRegistryConformance()
{
    // 1. Basic schema parse and register
    const std::string SchemaSource =
        "{ id: 'core:schema.definition.item.v1', definition_type: 'item', schema_version: 1, "
        "root: { kind: 'object', fields: { price: { kind: 'int64', required: true, min: 0 } } }, semantic_validators: [], extensions: {} }";
    std::vector<FDiagnostic> Diagnostics;
    auto Document = ParseJson5Document(SchemaSource, FParseLimits{}, Diagnostics);
    if (!Document.has_value() || !Diagnostics.empty())
    {
        return "schema_registry.parse_schema_document";
    }

    const FSchemaBinding Binding(
        "item", 1, "core:schema.definition.item.v1", "schemas/item_v1.schema.json5");
    auto Resource = ParseSchemaResource(
        *Document, Binding, "core", 0, Binding.GetRelativePath(), Diagnostics);
    if (!Resource.has_value() || !Diagnostics.empty())
    {
        return "schema_registry.parse_schema_resource";
    }

    FSchemaRegistry Registry;
    if (!Registry.Register(std::move(*Resource), Diagnostics)
        || Registry.Find("item", 1) == nullptr
        || Registry.Find("item", 2) != nullptr)
    {
        return "schema_registry.register_and_find";
    }

    // 1b. Multi-version schema coexistence on same definition_type
    const std::string SchemaV2Source =
        "{ id: 'core:schema.definition.item.v2', definition_type: 'item', schema_version: 2, "
        "root: { kind: 'object', fields: { price: { kind: 'int64', required: true, min: 0 }, "
        "weight: { kind: 'int64', required: false, default: 1, min: 0 } } }, semantic_validators: [], extensions: {} }";
    Diagnostics.clear();
    auto DocumentV2 = ParseJson5Document(SchemaV2Source, FParseLimits{}, Diagnostics);
    if (!DocumentV2.has_value() || !Diagnostics.empty())
    {
        return "schema_registry.parse_schema_v2_document";
    }

    const FSchemaBinding BindingV2(
        "item", 2, "core:schema.definition.item.v2", "schemas/item_v2.schema.json5");
    auto ResourceV2 = ParseSchemaResource(
        *DocumentV2, BindingV2, "core", 0, BindingV2.GetRelativePath(), Diagnostics);
    if (!ResourceV2.has_value()
        || !Registry.Register(std::move(*ResourceV2), Diagnostics)
        || Registry.Find("item", 1) == nullptr
        || Registry.Find("item", 2) == nullptr
        || Registry.Find("item", 1)->GetKey().SchemaVersion != 1
        || Registry.Find("item", 2)->GetKey().SchemaVersion != 2)
    {
        return "schema_registry.version_coexistence";
    }

    // 2. Duplicate binding rejected
    Diagnostics.clear();
    auto Duplicate = ParseSchemaResource(
        *Document, Binding, "core", 1, Binding.GetRelativePath(), Diagnostics);
    if (!Duplicate.has_value()
        || Registry.Register(std::move(*Duplicate), Diagnostics)
        || Diagnostics.empty()
        || Diagnostics.back().Code != "core:diagnostic.schema.binding.duplicate")
    {
        return "schema_registry.duplicate_binding_rejected";
    }

    // 3. Multi-version definitions in repository build
    FMemoryContentSourceProvider MultiVersionProvider;
    MultiVersionProvider.Sources.emplace("core/schemas/item_v1.schema.json5", SchemaSource);
    MultiVersionProvider.Sources.emplace("core/schemas/item_v2.schema.json5", SchemaV2Source);
    MultiVersionProvider.Sources.emplace(
        "core/definitions/items_v1.json5",
        "{ schema_version: 1, type: 'item', definitions: [{ id: 'core:item.sword', data: { price: 10 } }] }");
    MultiVersionProvider.Sources.emplace(
        "core/definitions/items_v2.json5",
        "{ schema_version: 2, type: 'item', definitions: [{ id: 'core:item.shield', data: { price: 20, weight: 5 } }] }");
    FBuildOptions MultiVersionOptions;
    MultiVersionOptions.SourceProvider = &MultiVersionProvider;
    const FBuildResult MultiVersionResult = BuildRepository(
        { FPackageDescriptor(
            "core", "core", 0,
            { "definitions/items_v1.json5", "definitions/items_v2.json5" },
            { Binding, BindingV2 }) },
        MultiVersionOptions);
    if (!MultiVersionResult.IsSuccess()
        || MultiVersionResult.GetCandidate().GetReadHandle().Find(FDefinitionId::Require("core:item.sword")) == nullptr
        || MultiVersionResult.GetCandidate().GetReadHandle().Find(FDefinitionId::Require("core:item.shield")) == nullptr)
    {
        return "schema_registry.multi_version_definitions_resolved";
    }

    // 4. Missing schema resource in repository build
    FMemoryContentSourceProvider MissingProvider;
    MissingProvider.Sources.emplace(
        "core/definitions/items.json5",
        "{ schema_version: 1, type: 'item', definitions: [] }");
    FBuildOptions Options;
    Options.SourceProvider = &MissingProvider;
    const FBuildResult MissingResult = BuildRepository(
        { FPackageDescriptor("core", "core", 0, { "definitions/items.json5" }) },
        Options);
    if (!MissingResult.IsFailure()
        || MissingResult.GetDiagnostics().empty()
        || MissingResult.GetDiagnostics().front().Code != "core:diagnostic.schema.binding.missing"
        || MissingResult.GetDiagnostics().front().JsonPointer != std::optional<std::string>("/schema_version"))
    {
        return "schema_registry.missing_schema_binding_rejected";
    }

    // 5. In-memory duplicate validator registration
    FSemanticValidatorRegistry ValidatorRegistry;
    FDuplicateValidator Validator1;
    FDuplicateValidator Validator2;
    std::vector<FDiagnostic> ValidatorDiagnostics;
    if (!ValidatorRegistry.Register(Validator1, ValidatorDiagnostics)
        || !ValidatorDiagnostics.empty())
    {
        return "schema_registry.register_first_validator";
    }
    if (ValidatorRegistry.Register(Validator2, ValidatorDiagnostics)
        || ValidatorDiagnostics.empty()
        || ValidatorDiagnostics.back().Code != "core:diagnostic.semantic.validator.duplicate_id")
    {
        return "schema_registry.duplicate_validator_rejected";
    }

    // --- UPP-05: UI Schema Publication & Namespace Ownership Conformance ---

    // 6. UI Schema parsing and registration in SchemaRegistry
    {
        const std::string UiSchemaSource =
            "{ id: 'core:schema.ui_field.progress_bar.v2', schema_domain: 'ui_field', schema_version: 2, "
            "root: { kind: 'object', fields: { percent: { kind: 'number', required: true, min: 0.0, max: 1.0 }, "
            "label: { kind: 'text', required: false } } } }";
        Diagnostics.clear();
        auto UiDoc = ParseJson5Document(UiSchemaSource, FParseLimits{}, Diagnostics);
        if (!UiDoc.has_value() || !Diagnostics.empty()) return "schema_registry.ui_schema.parse_doc";

        const FSchemaBinding UiBinding("progress_bar", 2, "core:schema.ui_field.progress_bar.v2", "schemas/progress_bar_v2.schema.json5");
        auto UiResource = ParseSchemaResource(*UiDoc, UiBinding, "core", 0, UiBinding.GetRelativePath(), Diagnostics);
        if (!UiResource.has_value() || !Diagnostics.empty()
            || !UiResource->IsUiSchema()
            || UiResource->GetSchemaDomain() != EUiSchemaDomain::UiField
            || UiResource->GetCompiledUiRootSpec() == nullptr
            || UiResource->GetCompiledRootSpec() != nullptr)
        {
            return "schema_registry.ui_schema.parse_resource";
        }

        FSchemaRegistry UiRegistry;
        if (!UiRegistry.Register(std::move(*UiResource), Diagnostics)
            || UiRegistry.FindById("core:schema.ui_field.progress_bar.v2") == nullptr
            || !UiRegistry.FindUiSchema("core:schema.ui_field.progress_bar.v2").has_value())
        {
            return "schema_registry.ui_schema.register_and_lookup";
        }
    }

    // 7. Foreign namespace schema declaration is rejected
    {
        const std::string ForeignSchemaSource =
            "{ id: 'core:schema.ui_field.bad.v1', schema_domain: 'ui_field', schema_version: 1, "
            "root: { kind: 'object', fields: { val: { kind: 'string' } } } }";
        Diagnostics.clear();
        auto ForeignDoc = ParseJson5Document(ForeignSchemaSource, FParseLimits{}, Diagnostics);
        if (!ForeignDoc.has_value()) return "schema_registry.foreign_namespace.parse_doc";

        const FSchemaBinding ForeignBinding("bad", 1, "core:schema.ui_field.bad.v1", "schemas/bad_v1.schema.json5");
        // mod_custom declaring a core schema is rejected
        auto ForeignResource = ParseSchemaResource(*ForeignDoc, ForeignBinding, "mod_custom", 1, ForeignBinding.GetRelativePath(), Diagnostics);
        if (ForeignResource.has_value() || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.schema.resource.foreign_namespace")
        {
            return "schema_registry.foreign_namespace_rejected";
        }
    }

    // 8. Mod declaring pure data-driven UI schema without C++ succeeds in repository build
    {
        const std::string ModSchemaSource =
            "{ id: 'weather_mod:schema.ui_field.weather_card.v1', schema_domain: 'ui_field', schema_version: 1, "
            "root: { kind: 'object', fields: { "
            "  station_id: { kind: 'key', required: true }, "
            "  title: { kind: 'text', required: true }, "
            "  temperature: { kind: 'integer', required: true, min: -100, max: 100 }, "
            "  icon: { kind: 'ref', target_kind: 'resource', required: false }, "
            "  on_click: { kind: 'binding', required: false } "
            "} } }";

        FMemoryContentSourceProvider ModSourceProvider;
        ModSourceProvider.Sources.emplace("weather_mod/schemas/weather_card_v1.schema.json5", ModSchemaSource);

        const FSchemaBinding ModUiBinding(
            "weather_card", 1, "weather_mod:schema.ui_field.weather_card.v1", "schemas/weather_card_v1.schema.json5");

        FBuildOptions ModBuildOptions;
        ModBuildOptions.SourceProvider = &ModSourceProvider;
        const FBuildResult ModBuildResult = BuildRepository(
            {
                FPackageDescriptor("core", "core", 0, {}),
                FPackageDescriptor("weather_mod", "weather_mod", 1, {}, { ModUiBinding })
            },
            ModBuildOptions);

        if (!ModBuildResult.IsSuccess())
        {
            return "schema_registry.mod_pure_data_ui_schema_build";
        }
    }

    // 9. Mod attempting to introduce a non-standard primitive kind into UI schema is rejected
    {
        const std::string InvalidModSchemaSource =
            "{ id: 'weather_mod:schema.ui_field.invalid.v1', schema_domain: 'ui_field', schema_version: 1, "
            "root: { kind: 'object', fields: { "
            "  custom_val: { kind: 'unsupported_primitive_kind', required: true } "
            "} } }";

        FMemoryContentSourceProvider InvalidModSourceProvider;
        InvalidModSourceProvider.Sources.emplace("weather_mod/schemas/invalid_v1.schema.json5", InvalidModSchemaSource);

        const FSchemaBinding InvalidModBinding(
            "invalid", 1, "weather_mod:schema.ui_field.invalid.v1", "schemas/invalid_v1.schema.json5");

        FBuildOptions InvalidModBuildOptions;
        InvalidModBuildOptions.SourceProvider = &InvalidModSourceProvider;
        const FBuildResult InvalidModBuildResult = BuildRepository(
            {
                FPackageDescriptor("core", "core", 0, {}),
                FPackageDescriptor("weather_mod", "weather_mod", 1, {}, { InvalidModBinding })
            },
            InvalidModBuildOptions);

        if (InvalidModBuildResult.IsSuccess()
            || InvalidModBuildResult.GetDiagnostics().empty()
            || InvalidModBuildResult.GetDiagnostics().front().Code != "core:diagnostic.ui_schema.field_spec.invalid_kind")
        {
            return "schema_registry.mod_nonstandard_kind_rejected";
        }
    }

    return "";
}
} // namespace GV2ContentCore::Testing

