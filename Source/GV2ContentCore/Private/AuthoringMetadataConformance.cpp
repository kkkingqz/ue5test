#include "GV2ContentCore/Testing/AuthoringMetadataConformance.h"
#include "GV2ContentCore/AuthoringMetadata.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"
#include "GV2ContentCore/SchemaRegistry.h"

#include <iostream>
#include <memory>

namespace GV2ContentCore::Testing
{

namespace
{

std::optional<FSchemaResource> CreateMockSchema()
{
    std::string SchemaJson =
        "{\n"
        "  id: \"test:schema.definition.item.v1\",\n"
        "  definition_type: \"item\",\n"
        "  schema_version: 1,\n"
        "  root: {\n"
        "    kind: \"object\",\n"
        "    fields: {\n"
        "      price: { kind: \"int64\", required: true, min: 0 },\n"
        "      name: { kind: \"string\", required: true },\n"
        "      icon: { kind: \"string\", required: false },\n"
        "    },\n"
        "  },\n"
        "  semantic_validators: [],\n"
        "  extensions: {},\n"
        "}\n";

    FParseLimits Limits;
    std::vector<FDiagnostic> Diags;
    auto Doc = ParseJson5Document(SchemaJson, Limits, Diags, "test", 0, "schemas/item_v1.schema.json5");
    if (!Doc) return std::nullopt;

    FSchemaBinding Binding("item", 1, "test:schema.definition.item.v1", "schemas/item_v1.schema.json5");
    return ParseSchemaResource(*Doc, Binding, "test", 0, "schemas/item_v1.schema.json5", Diags);
}

bool TestValidParsing()
{
    auto Schema = CreateMockSchema();
    if (!Schema) return false;

    std::string UiMetaJson =
        "// UI metadata for item schema\n"
        "{\n"
        "  schema_id: \"test:schema.definition.item.v1\",\n"
        "  fields: {\n"
        "    name: {\n"
        "      label: \"Item Name\",\n"
        "      description: \"The display name of the item\",\n"
        "      category: \"General\",\n"
        "      order: 1,\n"
        "      widget_hint: \"text\",\n"
        "    },\n"
        "    price: {\n"
        "      label: \"Price in Gold\",\n"
        "      description: \"Cost in standard currency\",\n"
        "      category: \"Economy\",\n"
        "      order: 2,\n"
        "      widget_hint: \"number\",\n"
        "    },\n"
        "  },\n"
        "}\n";

    std::vector<FDiagnostic> Diags;
    auto Result = ParseSchemaUiMetadata(UiMetaJson, *Schema, Diags, "test", "schemas/item_v1.ui.json5");
    if (!Result.has_value() || !Diags.empty()) return false;

    const auto* NameMeta = Result->FindField("name");
    if (!NameMeta || !NameMeta->Label.has_value() || *NameMeta->Label != "Item Name") return false;
    if (!NameMeta->Order.has_value() || *NameMeta->Order != 1) return false;
    if (!NameMeta->WidgetHint.has_value() || *NameMeta->WidgetHint != "text") return false;

    const auto* PriceMeta = Result->FindField("price");
    if (!PriceMeta || !PriceMeta->Label.has_value() || *PriceMeta->Label != "Price in Gold") return false;
    if (!PriceMeta->Order.has_value() || *PriceMeta->Order != 2) return false;
    if (!PriceMeta->Category.has_value() || *PriceMeta->Category != "Economy") return false;

    // Field not declared in metadata returns nullptr
    if (Result->FindField("icon") != nullptr) return false;

    return true;
}

bool TestUnresolvedField()
{
    auto Schema = CreateMockSchema();
    if (!Schema) return false;

    std::string UiMetaJson =
        "{\n"
        "  fields: {\n"
        "    stale_field: {\n"
        "      label: \"Stale\",\n"
        "    },\n"
        "  },\n"
        "}\n";

    std::vector<FDiagnostic> Diags;
    auto Result = ParseSchemaUiMetadata(UiMetaJson, *Schema, Diags, "test", "schemas/item_v1.ui.json5");
    if (Result.has_value() || Diags.empty()) return false;

    bool bHasUnresolvedCode = false;
    for (const auto& D : Diags)
    {
        if (D.Code == "core:diagnostic.schema.ui_metadata.unresolved_field")
        {
            bHasUnresolvedCode = true;
            break;
        }
    }
    return bHasUnresolvedCode;
}

bool TestUnknownRootProperty()
{
    auto Schema = CreateMockSchema();
    if (!Schema) return false;

    std::string UiMetaJson =
        "{\n"
        "  unknown_root_prop: true,\n"
        "  fields: {\n"
        "    name: { label: \"Name\" },\n"
        "  },\n"
        "}\n";

    std::vector<FDiagnostic> Diags;
    auto Result = ParseSchemaUiMetadata(UiMetaJson, *Schema, Diags, "test", "schemas/item_v1.ui.json5");
    if (Result.has_value() || Diags.empty()) return false;

    bool bHasUnknownCode = false;
    for (const auto& D : Diags)
    {
        if (D.Code == "core:diagnostic.schema.ui_metadata.unknown_field")
        {
            bHasUnknownCode = true;
            break;
        }
    }
    return bHasUnknownCode;
}

bool TestUnknownFieldProperty()
{
    auto Schema = CreateMockSchema();
    if (!Schema) return false;

    std::string UiMetaJson =
        "{\n"
        "  fields: {\n"
        "    name: {\n"
        "      label: \"Name\",\n"
        "      unknown_prop: 123,\n"
        "    },\n"
        "  },\n"
        "}\n";

    std::vector<FDiagnostic> Diags;
    auto Result = ParseSchemaUiMetadata(UiMetaJson, *Schema, Diags, "test", "schemas/item_v1.ui.json5");
    if (Result.has_value() || Diags.empty()) return false;

    bool bHasUnknownCode = false;
    for (const auto& D : Diags)
    {
        if (D.Code == "core:diagnostic.schema.ui_metadata.unknown_field")
        {
            bHasUnknownCode = true;
            break;
        }
    }
    return bHasUnknownCode;
}

bool TestInvalidTypes()
{
    auto Schema = CreateMockSchema();
    if (!Schema) return false;

    std::string UiMetaJson =
        "{\n"
        "  fields: {\n"
        "    name: {\n"
        "      order: \"not_a_number\",\n"
        "    },\n"
        "  },\n"
        "}\n";

    std::vector<FDiagnostic> Diags;
    auto Result = ParseSchemaUiMetadata(UiMetaJson, *Schema, Diags, "test", "schemas/item_v1.ui.json5");
    if (Result.has_value() || Diags.empty()) return false;

    bool bHasInvalidTypeCode = false;
    for (const auto& D : Diags)
    {
        if (D.Code == "core:diagnostic.schema.ui_metadata.invalid_type")
        {
            bHasInvalidTypeCode = true;
            break;
        }
    }
    return bHasInvalidTypeCode;
}

} // namespace

std::string RunAuthoringMetadataConformance()
{
    if (!TestValidParsing()) return "authoring_metadata.valid_parsing";
    if (!TestUnresolvedField()) return "authoring_metadata.unresolved_field";
    if (!TestUnknownRootProperty()) return "authoring_metadata.unknown_root_property";
    if (!TestUnknownFieldProperty()) return "authoring_metadata.unknown_field_property";
    if (!TestInvalidTypes()) return "authoring_metadata.invalid_types";

    return "";
}

} // namespace GV2ContentCore::Testing
