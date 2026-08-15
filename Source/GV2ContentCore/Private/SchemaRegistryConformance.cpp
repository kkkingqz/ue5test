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
        "root: { kind: 'object', fields: {} }, semantic_validators: [], extensions: {} }";
    std::vector<FDiagnostic> Diagnostics;
    auto Document = ParseJson5Document(SchemaSource, FParseLimits{}, Diagnostics);
    if (!Document.has_value() || !Diagnostics.empty())
    {
        return "schema_registry.parse_schema_document";
    }

    const FSchemaBinding Binding(
        "item", 1, "core:schema.definition.item.v1", "schemas/not_derived_from_name.json5");
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

    // 3. Missing schema resource in repository build
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

    // 4. In-memory duplicate validator registration
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
