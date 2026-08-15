#include "GV2ContentCore/Testing/ExtensionSchemaConformance.h"

#include "GV2ContentCore/ExtensionSchema.h"
#include "GV2ContentCore/FieldValidation.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"
#include "GV2ContentCore/RepositoryBuilder.h"

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
} // namespace

std::string RunExtensionSchemaConformance()
{
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
    if (!Document.has_value() || !Diagnostics.empty())
    {
        return "extension_schema.parse_document";
    }

    auto Resource = ParseExtensionSchemaResource(
        *Document, Binding, "weather_mod", 1, Binding.GetRelativePath(), Diagnostics);
    if (!Resource.has_value() || !Diagnostics.empty())
    {
        return "extension_schema.parse_resource";
    }

    FExtensionSchemaRegistry Registry;
    if (!Registry.Register(std::move(*Resource), Diagnostics) || !Diagnostics.empty())
    {
        return "extension_schema.register_resource";
    }

    // 1. Extension block validation and defaults injection
    FValidationDiagnosticContext Context;
    Context.PackageId = "weather_mod";
    Context.PackageLoadIndex = 1;
    Context.DefinitionId = "weather_mod:item.test";
    FValue MaterializedExtensions;
    if (!ValidateExtensionBlocks(
            FValue::MakeObject({ { "weather_mod", FValue::MakeObject() } }),
            MaterializedExtensions,
            Registry, "item", 1, EExtensionSite::DefinitionEntry, "weather_mod",
            nullptr, "/extensions", Context, Diagnostics)
        || !Diagnostics.empty())
    {
        return "extension_schema.validate_extension_blocks";
    }

    const FValue* MaterializedBlock = MaterializedExtensions.FindField("weather_mod");
    if (MaterializedBlock == nullptr
        || MaterializedBlock->FindField("enabled") == nullptr
        || MaterializedBlock->FindField("enabled")->AsBoolean())
    {
        return "extension_schema.defaults_injected";
    }

    // 2. Foreign namespace in extension block rejected
    Diagnostics.clear();
    if (ValidateExtensionBlocks(
            FValue::MakeObject({ { "core", FValue::MakeObject() } }),
            MaterializedExtensions,
            Registry, "item", 1, EExtensionSite::DefinitionEntry, "weather_mod",
            nullptr, "/extensions", Context, Diagnostics)
        || Diagnostics.empty()
        || Diagnostics.front().Code != "core:diagnostic.extension.block.foreign_namespace")
    {
        return "extension_schema.foreign_namespace_rejected";
    }

    // 3. Missing extension reference blocks repository publishing
    FMemoryContentSourceProvider Provider;
    Provider.Sources.emplace(
        "core/schemas/item.json5",
        "{id:'core:schema.item.v1',definition_type:'item',schema_version:1,"
        "root:{kind:'object',fields:{}},semantic_validators:[],extensions:{}}");
    Provider.Sources.emplace(
        "weather_mod/schemas/item_weather_entry_v1.schema.json5",
        "{ id: 'weather_mod:schema.extension.item.entry.v1', definition_type: 'item', "
        "schema_version: 1, extension_site: 'definition_entry', extension_namespace: 'weather_mod', "
        "root: { kind: 'object', fields: { linked_item: { kind: 'ref', "
        "target_kind: 'item', required: true } } } }");
    Provider.Sources.emplace(
        "weather_mod/definitions/items.json5",
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

    if (!MissingExtensionReference.IsFailure()
        || MissingExtensionReference.GetDiagnostics().empty()
        || MissingExtensionReference.GetDiagnostics().front().Code != "core:diagnostic.reference.target_missing"
        || MissingExtensionReference.GetDiagnostics().front().SchemaId != std::optional<std::string>(Binding.GetSchemaId()))
    {
        return "extension_schema.missing_extension_reference_rejected";
    }

    return "";
}
} // namespace GV2ContentCore::Testing
