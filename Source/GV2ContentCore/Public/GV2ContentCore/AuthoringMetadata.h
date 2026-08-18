#pragma once

#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/GV2ContentCore.h"
#include "GV2ContentCore/SchemaRegistry.h"
#include "GV2ContentCore/Value.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace GV2ContentCore
{

struct GV2_CONTENT_CORE_API FFieldUiMetadata
{
    std::optional<std::string> Label;
    std::optional<std::string> Description;
    std::optional<std::string> Category;
    std::optional<std::int64_t> Order;
    std::optional<std::string> WidgetHint;

    bool IsEmpty() const
    {
        return !Label.has_value() &&
               !Description.has_value() &&
               !Category.has_value() &&
               !Order.has_value() &&
               !WidgetHint.has_value();
    }
};

class GV2_CONTENT_CORE_API FSchemaUiMetadata
{
public:
    FSchemaUiMetadata() = default;
    explicit FSchemaUiMetadata(std::string InRelativeSource)
        : RelativeSource(std::move(InRelativeSource)) {}

    const std::string& GetRelativeSource() const { return RelativeSource; }
    void SetRelativeSource(std::string InRelativeSource) { RelativeSource = std::move(InRelativeSource); }

    const std::unordered_map<std::string, FFieldUiMetadata>& GetFields() const { return Fields; }
    std::unordered_map<std::string, FFieldUiMetadata>& GetFields() { return Fields; }

    const FFieldUiMetadata* FindField(const std::string& FieldName) const
    {
        auto It = Fields.find(FieldName);
        if (It != Fields.end())
        {
            return &It->second;
        }
        return nullptr;
    }

    void SetField(const std::string& FieldName, FFieldUiMetadata Metadata)
    {
        Fields[FieldName] = std::move(Metadata);
    }

private:
    std::string RelativeSource;
    std::unordered_map<std::string, FFieldUiMetadata> Fields;
};

/**
 * Parses and validates an authoring UI metadata document (schemas/<name>.ui.json5).
 * Validates that all metadata fields resolve to fields in the given schema.
 * Rejects unknown properties, unresolved fields, and invalid types.
 */
GV2_CONTENT_CORE_API std::optional<FSchemaUiMetadata> ParseSchemaUiMetadata(
    const std::string& Content,
    const FSchemaResource& Schema,
    std::vector<FDiagnostic>& OutDiagnostics,
    const std::string& PackageId = "",
    const std::string& RelativeSource = "");

} // namespace GV2ContentCore
