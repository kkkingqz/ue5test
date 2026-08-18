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
        *Document, Binding, "test_mod", 1, Binding.GetRelativePath(), Diagnostics);
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

    return "";
}
} // namespace GV2ContentCore::Testing
