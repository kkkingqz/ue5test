#pragma once

#include "GV2ContentEditor/GV2ContentEditor.h"
#include "GV2ContentEditor/FieldAdapterRegistry.h"
#include "GV2ContentCore/AuthoringMetadata.h"
#include "GV2ContentCore/ExtensionSchema.h"
#include "GV2ContentCore/FieldValidation.h"
#include "GV2ContentCore/SchemaRegistry.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace GV2ContentEditor
{

enum class EPropertyPresence : std::uint8_t
{
    RequiredMissing,
    Absent,
    ImplicitDefault,
    Explicit
};

/**
 * Descriptor of a single field in the schema-driven form.
 */
struct GV2_CONTENT_EDITOR_API FGV2FormFieldDescriptor final
{
    std::string FieldName;
    std::string JsonPointer; // e.g. "/data/name" or "/data/inventory"
    bool bRequired = false;
    std::string DisplayLabel;
    std::string Description;
    std::string Category = "General";
    std::int64_t Order = 0;
    EPropertyPresence Presence = EPropertyPresence::Explicit;
    std::optional<GV2ContentCore::FValue> DefaultValue;
    FFieldAdapterDescriptor AdapterDescriptor;
    GV2ContentCore::FCompiledFieldSpecPtr Spec;
};

/**
 * Group of fields under a shared category header.
 */
struct GV2_CONTENT_EDITOR_API FGV2FormCategorySection final
{
    std::string CategoryName;
    std::vector<FGV2FormFieldDescriptor> Fields;
};

/**
 * Complete form model built dynamically from a definition schema and presentation metadata (CED-11).
 */
struct GV2_CONTENT_EDITOR_API FGV2SchemaFormModel final
{
    std::string DefinitionType;
    std::string SchemaId;
    std::vector<FGV2FormCategorySection> Categories;
    std::vector<FGV2FormFieldDescriptor> AllFields;

    static FGV2SchemaFormModel BuildFromSchema(
        const GV2ContentCore::FSchemaResource& Schema,
        const GV2ContentCore::FSchemaUiMetadata* UiMetadata = nullptr,
        const std::vector<GV2ContentCore::FExtensionSchemaResource>& ExtensionSchemas = {});
};

} // namespace GV2ContentEditor
