#include "GV2ContentCore/Testing/UiSchemaConformance.h"

#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"
#include "GV2ContentCore/ScalarValidation.h"
#include "GV2ContentCore/UiSchema.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace GV2ContentCore::Testing
{
namespace
{
FCompiledUiFieldSpecPtr CompileUiCase(
    const std::string_view Json,
    std::optional<FParsedDocument>& OutDocument,
    std::vector<FDiagnostic>& OutDiagnostics,
    const IUiSchemaResolver* Resolver = nullptr,
    std::optional<std::string> SchemaId = std::nullopt,
    std::optional<std::string> PackageId = std::nullopt)
{
    OutDiagnostics.clear();
    OutDocument = ParseJson5Document(Json, FParseLimits{}, OutDiagnostics);
    if (!OutDocument.has_value() || !OutDiagnostics.empty()) return nullptr;
    FValidationDiagnosticContext Context;
    if (SchemaId.has_value()) Context.SchemaId = *SchemaId;
    if (PackageId.has_value()) Context.PackageId = *PackageId;
    OutDiagnostics.clear();
    return CompileUiFieldSpec(OutDocument->GetRootValue(), &*OutDocument, "", Context, OutDiagnostics, Resolver);
}

bool ValidateUiValueCase(
    const FCompiledUiFieldSpec& FieldSpec,
    const std::string_view ValueJson,
    FValue& OutMaterialized,
    std::optional<FParsedDocument>& OutValueDoc,
    std::vector<FDiagnostic>& OutDiagnostics,
    std::optional<std::string> SchemaId = std::nullopt)
{
    OutDiagnostics.clear();
    OutValueDoc = ParseJson5Document(ValueJson, FParseLimits{}, OutDiagnostics);
    if (!OutValueDoc.has_value() || !OutDiagnostics.empty()) return false;
    FValidationDiagnosticContext Context;
    if (SchemaId.has_value()) Context.SchemaId = *SchemaId;
    OutDiagnostics.clear();
    return ValidateUiFieldValue(OutValueDoc->GetRootValue(), FieldSpec, OutMaterialized, &*OutValueDoc, "", Context, OutDiagnostics);
}
}

std::string RunUiSchemaConformance()
{
    std::optional<FParsedDocument> Document;
    std::optional<FParsedDocument> ValueDoc;
    std::vector<FDiagnostic> Diagnostics;
    FValue Materialized;

    // 0. schema_domain: round-trips valid values, rejects an unknown one.
    if (ParseUiSchemaDomain("ui_field") != std::optional(EUiSchemaDomain::UiField)
        || ParseUiSchemaDomain("ui_value") != std::optional(EUiSchemaDomain::UiValue)
        || ParseUiSchemaDomain("bogus").has_value()
        || ToString(EUiSchemaDomain::UiField) != "ui_field"
        || ToString(EUiSchemaDomain::UiValue) != "ui_value")
    {
        return "ui_schema.domain.round_trip";
    }

    // 1. bool: positive.
    {
        const auto Spec = CompileUiCase("{kind:'bool', nullable:true}", Document, Diagnostics);
        if (Spec == nullptr || !Diagnostics.empty() || Spec->Kind != EUiFieldKind::Scalar
            || !Spec->Scalar.has_value() || Spec->Scalar->Kind != EScalarFieldKind::Boolean
            || !Spec->bNullable)
        {
            return "ui_schema.scalar.bool.positive";
        }
    }
    // bool: negative — a constraint field foreign to bool is rejected, not silently ignored.
    {
        const auto Spec = CompileUiCase("{kind:'bool', min_length:3}", Document, Diagnostics);
        if (Spec != nullptr || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.schema.field_spec.unknown_field")
        {
            return "ui_schema.scalar.bool.negative";
        }
    }

    // 2. integer: positive, including a validated explicit default.
    {
        const auto Spec = CompileUiCase("{kind:'integer', min:0, max:10, default:5}", Document, Diagnostics);
        if (Spec == nullptr || !Diagnostics.empty() || Spec->Kind != EUiFieldKind::Scalar
            || !Spec->Scalar.has_value() || Spec->Scalar->Kind != EScalarFieldKind::Integer
            || !Spec->DefaultValue.has_value() || Spec->DefaultValue->AsInteger() != 5)
        {
            return "ui_schema.scalar.integer.positive";
        }
    }
    // integer: negative — inverted range is a typed constraint violation.
    {
        const auto Spec = CompileUiCase("{kind:'integer', min:10, max:0}", Document, Diagnostics);
        if (Spec != nullptr || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.schema.field_spec.invalid_constraint_range")
        {
            return "ui_schema.scalar.integer.negative";
        }
    }

    // 3. number: positive.
    {
        const auto Spec = CompileUiCase("{kind:'number', min:0.0, max:1.0}", Document, Diagnostics);
        if (Spec == nullptr || !Diagnostics.empty() || Spec->Kind != EUiFieldKind::Scalar
            || !Spec->Scalar.has_value() || Spec->Scalar->Kind != EScalarFieldKind::Number)
        {
            return "ui_schema.scalar.number.positive";
        }
    }
    // number: negative — min and exclusive_min are mutually exclusive.
    {
        const auto Spec = CompileUiCase("{kind:'number', min:0, exclusive_min:1}", Document, Diagnostics);
        if (Spec != nullptr || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.schema.field_spec.conflicting_constraint")
        {
            return "ui_schema.scalar.number.negative";
        }
    }

    // 4. string: positive. Also the direct proof that `key` is not inferred
    // from `string` — a string-kind FieldSpec compiles to Scalar, never Key.
    {
        const auto Spec = CompileUiCase("{kind:'string', min_length:1, max_length:10}", Document, Diagnostics);
        if (Spec == nullptr || !Diagnostics.empty() || Spec->Kind != EUiFieldKind::Scalar
            || !Spec->Scalar.has_value() || Spec->Scalar->Kind != EScalarFieldKind::String
            || Spec->Kind == EUiFieldKind::Key)
        {
            return "ui_schema.scalar.string.positive";
        }
    }
    // string: negative — a field valid only for integer/number is rejected for string.
    {
        const auto Spec = CompileUiCase("{kind:'string', min:0}", Document, Diagnostics);
        if (Spec != nullptr || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.schema.field_spec.unknown_field")
        {
            return "ui_schema.scalar.string.negative";
        }
    }

    // 5. key: positive.
    {
        const auto Spec = CompileUiCase("{kind:'key'}", Document, Diagnostics);
        if (Spec == nullptr || !Diagnostics.empty() || Spec->Kind != EUiFieldKind::Key)
        {
            return "ui_schema.key.positive";
        }
    }
    // key: negative — `key: {kind: "string"}` is not a valid shorthand; declaring
    // `key` with a string-only constraint field is a schema compile error.
    {
        const auto Spec = CompileUiCase("{kind:'key', min_length:5}", Document, Diagnostics);
        if (Spec != nullptr || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.ui_schema.field_spec.unknown_field")
        {
            return "ui_schema.key.negative";
        }
    }

    // 6. text: positive.
    {
        const auto Spec = CompileUiCase("{kind:'text'}", Document, Diagnostics);
        if (Spec == nullptr || !Diagnostics.empty() || Spec->Kind != EUiFieldKind::Text)
        {
            return "ui_schema.text.positive";
        }
    }
    // text: negative — closed spec rejects a foreign constraint field.
    {
        const auto Spec = CompileUiCase("{kind:'text', pattern:'x'}", Document, Diagnostics);
        if (Spec != nullptr || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.ui_schema.field_spec.unknown_field")
        {
            return "ui_schema.text.negative";
        }
    }

    // 7. ref: positive.
    {
        const auto Spec = CompileUiCase("{kind:'ref', target_kind:'resource'}", Document, Diagnostics);
        if (Spec == nullptr || !Diagnostics.empty() || Spec->Kind != EUiFieldKind::Ref
            || Spec->RefTargetKind != "resource")
        {
            return "ui_schema.ref.positive";
        }
    }
    // ref: negative — target_kind is required.
    {
        const auto Spec = CompileUiCase("{kind:'ref'}", Document, Diagnostics);
        if (Spec != nullptr || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.ui_schema.field_spec.invalid_target_kind")
        {
            return "ui_schema.ref.negative";
        }
    }

    // 8. binding: positive.
    {
        const auto Spec = CompileUiCase(
            "{kind:'binding', input_schema_id:'core:schema.button_click'}", Document, Diagnostics);
        if (Spec == nullptr || !Diagnostics.empty() || Spec->Kind != EUiFieldKind::Binding
            || Spec->BindingInputSchemaId != std::optional<std::string>("core:schema.button_click"))
        {
            return "ui_schema.binding.positive";
        }
    }
    // binding: negative — input_schema_id must be a valid Stable ID of kind "schema".
    {
        const auto Spec = CompileUiCase("{kind:'binding', input_schema_id:'not-a-stable-id'}", Document, Diagnostics);
        if (Spec != nullptr || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.ui_schema.field_spec.invalid_input_schema_id")
        {
            return "ui_schema.binding.negative";
        }
    }

    // 9. object: positive, including the required flag on a child field.
    {
        const auto Spec = CompileUiCase(
            "{kind:'object', fields:{id:{kind:'key', required:true}, label:{kind:'string'}}}",
            Document, Diagnostics);
        if (Spec == nullptr || !Diagnostics.empty() || Spec->Kind != EUiFieldKind::Object
            || Spec->Fields.size() != 2
            || Spec->Fields[0].Name != "id" || !Spec->Fields[0].bRequired
            || Spec->Fields[0].Spec->Kind != EUiFieldKind::Key
            || Spec->Fields[1].Name != "label" || Spec->Fields[1].bRequired)
        {
            return "ui_schema.object.positive";
        }
    }
    // object: negative — fields is required.
    {
        const auto Spec = CompileUiCase("{kind:'object'}", Document, Diagnostics);
        if (Spec != nullptr || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.ui_schema.field_spec.invalid_fields")
        {
            return "ui_schema.object.negative";
        }
    }

    // 10. array: positive — keyed_by names an items field compiled as kind key.
    {
        const auto Spec = CompileUiCase(
            "{kind:'array', items:{kind:'object', fields:{id:{kind:'key'}, label:{kind:'string'}}}, keyed_by:'id'}",
            Document, Diagnostics);
        if (Spec == nullptr || !Diagnostics.empty() || Spec->Kind != EUiFieldKind::Array
            || Spec->KeyedBy != std::optional<std::string>("id"))
        {
            return "ui_schema.array.positive";
        }
    }
    // array: negative — keyed_by naming a string-kind field is rejected: `key`
    // is not satisfied by a field that merely looks like an identifier.
    {
        const auto Spec = CompileUiCase(
            "{kind:'array', items:{kind:'object', fields:{id:{kind:'string'}, label:{kind:'string'}}}, keyed_by:'id'}",
            Document, Diagnostics);
        if (Spec != nullptr || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.ui_schema.field_spec.invalid_keyed_by")
        {
            return "ui_schema.array.negative";
        }
    }
    // array: interactive array positive — array with binding and keyed_by compiles cleanly.
    {
        const auto Spec = CompileUiCase(
            "{kind:'array', items:{kind:'object', fields:{id:{kind:'key'}, action:{kind:'binding'}}}, keyed_by:'id'}",
            Document, Diagnostics);
        if (Spec == nullptr || !Diagnostics.empty() || Spec->Kind != EUiFieldKind::Array
            || Spec->KeyedBy != std::optional<std::string>("id"))
        {
            return "ui_schema.array.interactive_keyed.positive";
        }
    }
    // array: interactive array negative — array containing binding without keyed_by is rejected.
    {
        const auto Spec = CompileUiCase(
            "{kind:'array', items:{kind:'object', fields:{action:{kind:'binding'}}}}",
            Document, Diagnostics);
        if (Spec != nullptr || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.ui_schema.field_spec.missing_keyed_by")
        {
            return "ui_schema.array.interactive_unkeyed.negative";
        }
    }
    // array: nested interactive array negative — array with nested binding without keyed_by is rejected.
    {
        const auto Spec = CompileUiCase(
            "{kind:'array', items:{kind:'object', fields:{sub:{kind:'object', fields:{action:{kind:'binding'}}}}}}",
            Document, Diagnostics);
        if (Spec != nullptr || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.ui_schema.field_spec.missing_keyed_by")
        {
            return "ui_schema.array.interactive_nested_unkeyed.negative";
        }
    }

    // 11. screen_fields: positive — a trivial closed leaf marker at this compile stage.
    {
        const auto Spec = CompileUiCase("{kind:'screen_fields'}", Document, Diagnostics);
        if (Spec == nullptr || !Diagnostics.empty() || Spec->Kind != EUiFieldKind::ScreenFields)
        {
            return "ui_schema.screen_fields.positive";
        }
    }
    // screen_fields: negative — recursive per-entry resolution is out of UPP-02 scope,
    // so declaring it here is an unknown field, not a silently accepted extension.
    {
        const auto Spec = CompileUiCase("{kind:'screen_fields', items:{kind:'string'}}", Document, Diagnostics);
        if (Spec != nullptr || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.ui_schema.field_spec.unknown_field")
        {
            return "ui_schema.screen_fields.negative";
        }
    }

    // 12. unknown kind — `enum` is explicitly outside the standard UI kind vocabulary.
    {
        const auto Spec = CompileUiCase("{kind:'enum'}", Document, Diagnostics);
        if (Spec != nullptr || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.ui_schema.field_spec.invalid_kind")
        {
            return "ui_schema.unknown_kind";
        }
    }

    // 13. missing kind.
    {
        const auto Spec = CompileUiCase("{}", Document, Diagnostics);
        if (Spec != nullptr || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.ui_schema.field_spec.invalid_kind")
        {
            return "ui_schema.missing_kind";
        }
    }

    // 14. default is scalar-only — every non-scalar kind rejects it outright.
    {
        const auto Spec = CompileUiCase("{kind:'key', default:'x'}", Document, Diagnostics);
        if (Spec != nullptr || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.ui_schema.field_spec.invalid_default")
        {
            return "ui_schema.default_on_non_scalar";
        }
    }

    // 15. schema_ref: positive — inlines referenced schema without producing a runtime schema_ref kind.
    {
        FInMemoryUiSchemaResolver Resolver;
        std::optional<FParsedDocument> TargetDoc = ParseJson5Document(
            "{id:'core:schema.ui_value.button_item.v1', schema_domain:'ui_value', schema_version:1, "
            "root:{kind:'object', fields:{id:{kind:'key', required:true}, label:{kind:'text', required:true}}}}",
            FParseLimits{}, Diagnostics);
        if (!TargetDoc.has_value()) return "ui_schema.schema_ref.target_parse_failed";
        Resolver.RegisterUiSchemaDocument("core:schema.ui_value.button_item.v1", std::make_shared<FParsedDocument>(std::move(*TargetDoc)));

        const auto Spec = CompileUiCase(
            "{kind:'array', keyed_by:'id', items:{kind:'schema_ref', schema_id:'core:schema.ui_value.button_item.v1'}}",
            Document, Diagnostics, &Resolver, "core:schema.ui_field.button_list.v1", "core");
        if (Spec == nullptr || !Diagnostics.empty() || Spec->Kind != EUiFieldKind::Array
            || Spec->KeyedBy != std::optional<std::string>("id")
            || Spec->Items == nullptr || Spec->Items->Kind != EUiFieldKind::Object
            || Spec->Items->Fields.size() != 2
            || Spec->Items->Fields[0].Name != "id" || Spec->Items->Fields[0].Spec->Kind != EUiFieldKind::Key
            || Spec->Items->Fields[1].Name != "label" || Spec->Items->Fields[1].Spec->Kind != EUiFieldKind::Text)
        {
            return "ui_schema.schema_ref.positive";
        }
    }

    // 16. schema_ref: nullable propagation.
    {
        FInMemoryUiSchemaResolver Resolver;
        std::optional<FParsedDocument> TargetDoc = ParseJson5Document(
            "{id:'core:schema.ui_value.item.v1', root:{kind:'text'}}",
            FParseLimits{}, Diagnostics);
        if (!TargetDoc.has_value()) return "ui_schema.schema_ref.target_parse_failed";
        Resolver.RegisterUiSchemaDocument("core:schema.ui_value.item.v1", std::make_shared<FParsedDocument>(std::move(*TargetDoc)));

        const auto Spec = CompileUiCase(
            "{kind:'schema_ref', schema_id:'core:schema.ui_value.item.v1', nullable:true}",
            Document, Diagnostics, &Resolver, "core:schema.ui_field.test.v1", "core");
        if (Spec == nullptr || !Diagnostics.empty() || Spec->Kind != EUiFieldKind::Text || !Spec->bNullable)
        {
            return "ui_schema.schema_ref.nullable";
        }
    }

    // 17. schema_ref: invalid schema_id.
    {
        const auto Spec = CompileUiCase(
            "{kind:'schema_ref', schema_id:'not-a-valid-stable-id'}",
            Document, Diagnostics);
        if (Spec != nullptr || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.ui_schema.schema_ref.invalid_schema_id")
        {
            return "ui_schema.schema_ref.invalid_schema_id";
        }
    }

    // 18. schema_ref: unresolved schema.
    {
        FInMemoryUiSchemaResolver Resolver;
        const auto Spec = CompileUiCase(
            "{kind:'schema_ref', schema_id:'core:schema.ui_value.nonexistent.v1'}",
            Document, Diagnostics, &Resolver, "core:schema.ui_field.test.v1", "core");
        if (Spec != nullptr || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.ui_schema.schema_ref.unresolved_schema")
        {
            return "ui_schema.schema_ref.unresolved";
        }
    }

    // 19. schema_ref: forbidden namespace boundary (core referencing mod).
    {
        FInMemoryUiSchemaResolver Resolver;
        std::optional<FParsedDocument> TargetDoc = ParseJson5Document(
            "{id:'weather_mod:schema.ui_value.item.v1', root:{kind:'text'}}",
            FParseLimits{}, Diagnostics);
        if (!TargetDoc.has_value()) return "ui_schema.schema_ref.target_parse_failed";
        Resolver.RegisterUiSchemaDocument("weather_mod:schema.ui_value.item.v1", std::make_shared<FParsedDocument>(std::move(*TargetDoc)));

        const auto Spec = CompileUiCase(
            "{kind:'schema_ref', schema_id:'weather_mod:schema.ui_value.item.v1'}",
            Document, Diagnostics, &Resolver, "core:schema.ui_field.test.v1", "core");
        if (Spec != nullptr || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.ui_schema.schema_ref.forbidden_namespace")
        {
            return "ui_schema.schema_ref.forbidden_namespace";
        }
    }

    // 20. schema_ref: direct cycle (length 1: A -> A).
    {
        FInMemoryUiSchemaResolver Resolver;
        std::optional<FParsedDocument> SelfCycleDoc = ParseJson5Document(
            "{id:'core:schema.ui_value.self_cycle.v1', root:{kind:'object', fields:{child:{kind:'schema_ref', schema_id:'core:schema.ui_value.self_cycle.v1'}}}}",
            FParseLimits{}, Diagnostics);
        if (!SelfCycleDoc.has_value()) return "ui_schema.schema_ref.target_parse_failed";
        Resolver.RegisterUiSchemaDocument("core:schema.ui_value.self_cycle.v1", std::make_shared<FParsedDocument>(std::move(*SelfCycleDoc)));

        const auto Spec = CompileUiCase(
            "{kind:'schema_ref', schema_id:'core:schema.ui_value.self_cycle.v1'}",
            Document, Diagnostics, &Resolver, "core:schema.ui_field.root.v1", "core");
        if (Spec != nullptr || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.ui_schema.schema_ref.cycle_detected"
            || Diagnostics.front().Message.find("core:schema.ui_value.self_cycle.v1 -> core:schema.ui_value.self_cycle.v1") == std::string::npos)
        {
            return "ui_schema.schema_ref.cycle.length_1";
        }
    }

    // 21. schema_ref: indirect cycle length 2 (A -> B -> A).
    {
        FInMemoryUiSchemaResolver Resolver;
        std::optional<FParsedDocument> DocA = ParseJson5Document(
            "{id:'core:schema.ui_value.node_a.v1', root:{kind:'object', fields:{next:{kind:'schema_ref', schema_id:'core:schema.ui_value.node_b.v1'}}}}",
            FParseLimits{}, Diagnostics);
        std::optional<FParsedDocument> DocB = ParseJson5Document(
            "{id:'core:schema.ui_value.node_b.v1', root:{kind:'object', fields:{next:{kind:'schema_ref', schema_id:'core:schema.ui_value.node_a.v1'}}}}",
            FParseLimits{}, Diagnostics);
        if (!DocA.has_value() || !DocB.has_value()) return "ui_schema.schema_ref.target_parse_failed";
        Resolver.RegisterUiSchemaDocument("core:schema.ui_value.node_a.v1", std::make_shared<FParsedDocument>(std::move(*DocA)));
        Resolver.RegisterUiSchemaDocument("core:schema.ui_value.node_b.v1", std::make_shared<FParsedDocument>(std::move(*DocB)));

        const auto Spec = CompileUiCase(
            "{kind:'schema_ref', schema_id:'core:schema.ui_value.node_a.v1'}",
            Document, Diagnostics, &Resolver, "core:schema.ui_field.root.v1", "core");
        if (Spec != nullptr || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.ui_schema.schema_ref.cycle_detected"
            || Diagnostics.front().Message.find("core:schema.ui_value.node_a.v1 -> core:schema.ui_value.node_b.v1 -> core:schema.ui_value.node_a.v1") == std::string::npos)
        {
            return "ui_schema.schema_ref.cycle.length_2";
        }
    }

    // 22. schema_ref: indirect cycle length > 2 (length 3: C1 -> C2 -> C3 -> C1).
    {
        FInMemoryUiSchemaResolver Resolver;
        std::optional<FParsedDocument> Doc1 = ParseJson5Document(
            "{id:'core:schema.ui_value.c1.v1', root:{kind:'object', fields:{next:{kind:'schema_ref', schema_id:'core:schema.ui_value.c2.v1'}}}}",
            FParseLimits{}, Diagnostics);
        std::optional<FParsedDocument> Doc2 = ParseJson5Document(
            "{id:'core:schema.ui_value.c2.v1', root:{kind:'object', fields:{next:{kind:'schema_ref', schema_id:'core:schema.ui_value.c3.v1'}}}}",
            FParseLimits{}, Diagnostics);
        std::optional<FParsedDocument> Doc3 = ParseJson5Document(
            "{id:'core:schema.ui_value.c3.v1', root:{kind:'object', fields:{next:{kind:'schema_ref', schema_id:'core:schema.ui_value.c1.v1'}}}}",
            FParseLimits{}, Diagnostics);
        if (!Doc1.has_value() || !Doc2.has_value() || !Doc3.has_value()) return "ui_schema.schema_ref.target_parse_failed";
        Resolver.RegisterUiSchemaDocument("core:schema.ui_value.c1.v1", std::make_shared<FParsedDocument>(std::move(*Doc1)));
        Resolver.RegisterUiSchemaDocument("core:schema.ui_value.c2.v1", std::make_shared<FParsedDocument>(std::move(*Doc2)));
        Resolver.RegisterUiSchemaDocument("core:schema.ui_value.c3.v1", std::make_shared<FParsedDocument>(std::move(*Doc3)));

        const auto Spec = CompileUiCase(
            "{kind:'schema_ref', schema_id:'core:schema.ui_value.c1.v1'}",
            Document, Diagnostics, &Resolver, "core:schema.ui_field.root.v1", "core");
        if (Spec != nullptr || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.ui_schema.schema_ref.cycle_detected"
            || Diagnostics.front().Message.find("core:schema.ui_value.c1.v1 -> core:schema.ui_value.c2.v1 -> core:schema.ui_value.c3.v1 -> core:schema.ui_value.c1.v1") == std::string::npos)
        {
            return "ui_schema.schema_ref.cycle.length_3";
        }
    }

    // 23. schema_ref: closed spec rejects foreign fields.
    {
        FInMemoryUiSchemaResolver Resolver;
        const auto Spec = CompileUiCase(
            "{kind:'schema_ref', schema_id:'core:schema.ui_value.item.v1', min_items:5}",
            Document, Diagnostics, &Resolver, "core:schema.ui_field.test.v1", "core");
        if (Spec != nullptr || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.ui_schema.field_spec.unknown_field")
        {
            return "ui_schema.schema_ref.unknown_field";
        }
    }

    // 24. schema_ref: default rejected.
    {
        FInMemoryUiSchemaResolver Resolver;
        const auto Spec = CompileUiCase(
            "{kind:'schema_ref', schema_id:'core:schema.ui_value.item.v1', default:42}",
            Document, Diagnostics, &Resolver, "core:schema.ui_field.test.v1", "core");
        if (Spec != nullptr || Diagnostics.empty()
            || Diagnostics.front().Code != "core:diagnostic.ui_schema.field_spec.invalid_default")
        {
            return "ui_schema.schema_ref.invalid_default";
        }
    }

    // --- UPP-04: Portable UI Value Validation Conformance ---

    // 25. value.scalar: positive and negative range/type.
    {
        const auto Spec = CompileUiCase("{kind:'integer', min:1, max:100}", Document, Diagnostics);
        if (Spec == nullptr) return "ui_value.scalar.spec_failed";

        // Positive
        if (!ValidateUiValueCase(*Spec, "42", Materialized, ValueDoc, Diagnostics)
            || !Diagnostics.empty() || Materialized.AsInteger() != 42)
        {
            return "ui_value.scalar.integer.positive";
        }

        // Negative: out of range
        if (ValidateUiValueCase(*Spec, "200", Materialized, ValueDoc, Diagnostics)
            || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.schema.value.constraint_failed")
        {
            return "ui_value.scalar.integer.range_negative";
        }

        // Negative: type mismatch
        if (ValidateUiValueCase(*Spec, "'not_an_int'", Materialized, ValueDoc, Diagnostics)
            || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.schema.value.type_mismatch")
        {
            return "ui_value.scalar.integer.type_negative";
        }
    }

    // 26. value.nullability: positive and negative.
    {
        const auto NonNullSpec = CompileUiCase("{kind:'integer'}", Document, Diagnostics);
        const auto NullableSpec = CompileUiCase("{kind:'integer', nullable:true}", Document, Diagnostics);
        if (NonNullSpec == nullptr || NullableSpec == nullptr) return "ui_value.nullability.spec_failed";

        // Positive nullable
        if (!ValidateUiValueCase(*NullableSpec, "null", Materialized, ValueDoc, Diagnostics)
            || !Diagnostics.empty() || !Materialized.IsNull())
        {
            return "ui_value.nullability.positive";
        }

        // Negative non-nullable
        if (ValidateUiValueCase(*NonNullSpec, "null", Materialized, ValueDoc, Diagnostics)
            || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.ui_schema.value.null_not_allowed")
        {
            return "ui_value.nullability.negative";
        }
    }

    // 27. value.key: positive grammar and negative (invalid chars, text-derived id, non-string).
    {
        const auto Spec = CompileUiCase("{kind:'key'}", Document, Diagnostics);
        if (Spec == nullptr) return "ui_value.key.spec_failed";

        // Positive key grammar
        if (!ValidateUiValueCase(*Spec, "'tab_main.btn-1@alpha:sub'", Materialized, ValueDoc, Diagnostics)
            || !Diagnostics.empty() || Materialized.AsString() != "tab_main.btn-1@alpha:sub")
        {
            return "ui_value.key.positive";
        }

        // Negative: non-string
        if (ValidateUiValueCase(*Spec, "123", Materialized, ValueDoc, Diagnostics)
            || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.ui_schema.value.invalid_type")
        {
            return "ui_value.key.invalid_type";
        }

        // Negative: invalid characters (uppercase, spaces)
        if (ValidateUiValueCase(*Spec, "'Invalid Key!'", Materialized, ValueDoc, Diagnostics)
            || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.ui_schema.value.invalid_key")
        {
            return "ui_value.key.invalid_grammar";
        }

        // Negative: derived from text Stable ID
        if (ValidateUiValueCase(*Spec, "'core:text.button.ok'", Materialized, ValueDoc, Diagnostics)
            || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.ui_schema.value.invalid_key")
        {
            return "ui_value.key.text_derived_rejected";
        }
    }

    // 28. value.ref: positive and negative target kind / invalid Stable ID.
    {
        const auto Spec = CompileUiCase("{kind:'ref', target_kind:'resource'}", Document, Diagnostics);
        if (Spec == nullptr) return "ui_value.ref.spec_failed";

        // Positive
        if (!ValidateUiValueCase(*Spec, "'core:resource.icon.sword'", Materialized, ValueDoc, Diagnostics)
            || !Diagnostics.empty() || Materialized.AsString() != "core:resource.icon.sword")
        {
            return "ui_value.ref.positive";
        }

        // Negative: wrong kind (command instead of resource)
        if (ValidateUiValueCase(*Spec, "'core:command.button.click'", Materialized, ValueDoc, Diagnostics)
            || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.ui_schema.value.invalid_stable_id")
        {
            return "ui_value.ref.wrong_kind";
        }

        // Negative: malformed stable ID
        if (ValidateUiValueCase(*Spec, "'not_a_stable_id'", Materialized, ValueDoc, Diagnostics)
            || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.ui_schema.value.invalid_stable_id")
        {
            return "ui_value.ref.invalid_id";
        }
    }

    // 29. value.text (TextSpec): positive and negative (missing text_id, raw string, unknown field).
    {
        const auto Spec = CompileUiCase("{kind:'text'}", Document, Diagnostics);
        if (Spec == nullptr) return "ui_value.text.spec_failed";

        // Positive TextSpec
        if (!ValidateUiValueCase(*Spec, "{text_id:'core:text.button.ok', style:'primary', args:{count:5}}", Materialized, ValueDoc, Diagnostics)
            || !Diagnostics.empty() || !Materialized.IsObject())
        {
            return "ui_value.text.positive";
        }

        // Negative: raw string where TextSpec expected
        if (ValidateUiValueCase(*Spec, "'Raw String'", Materialized, ValueDoc, Diagnostics)
            || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.ui_schema.value.invalid_text_spec")
        {
            return "ui_value.text.raw_string_rejected";
        }

        // Negative: missing text_id
        if (ValidateUiValueCase(*Spec, "{style:'primary'}", Materialized, ValueDoc, Diagnostics)
            || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.ui_schema.value.invalid_text_spec")
        {
            return "ui_value.text.missing_text_id";
        }

        // Negative: text_id with non-text kind
        if (ValidateUiValueCase(*Spec, "{text_id:'core:command.ok'}", Materialized, ValueDoc, Diagnostics)
            || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.ui_schema.value.invalid_text_spec")
        {
            return "ui_value.text.invalid_text_id_kind";
        }

        // Negative: unknown field in TextSpec (closed schema)
        if (ValidateUiValueCase(*Spec, "{text_id:'core:text.button.ok', unknown_prop:'bad'}", Materialized, ValueDoc, Diagnostics)
            || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.ui_schema.value.unknown_field")
        {
            return "ui_value.text.unknown_field";
        }
    }

    // 30. value.binding (BindingSpec): positive and negative (missing command_id, unknown field).
    {
        const auto Spec = CompileUiCase("{kind:'binding', input_schema_id:'core:schema.click'}", Document, Diagnostics);
        if (Spec == nullptr) return "ui_value.binding.spec_failed";

        // Positive BindingSpec
        if (!ValidateUiValueCase(*Spec, "{command_id:'core:command.location.proceed', args:{target:'gate'}}", Materialized, ValueDoc, Diagnostics)
            || !Diagnostics.empty() || !Materialized.IsObject())
        {
            return "ui_value.binding.positive";
        }

        // Negative: missing command_id
        if (ValidateUiValueCase(*Spec, "{args:{}}", Materialized, ValueDoc, Diagnostics)
            || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.ui_schema.value.invalid_binding_spec")
        {
            return "ui_value.binding.missing_command_id";
        }

        // Negative: command_id with wrong kind
        if (ValidateUiValueCase(*Spec, "{command_id:'core:text.button.ok'}", Materialized, ValueDoc, Diagnostics)
            || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.ui_schema.value.invalid_binding_spec")
        {
            return "ui_value.binding.invalid_command_id_kind";
        }

        // Negative: unknown field in BindingSpec (closed schema)
        if (ValidateUiValueCase(*Spec, "{command_id:'core:command.location.proceed', callback:'func'}", Materialized, ValueDoc, Diagnostics)
            || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.ui_schema.value.unknown_field")
        {
            return "ui_value.binding.unknown_field";
        }
    }

    // 31. value.object: positive (with default application), missing required field, unknown field.
    {
        const auto Spec = CompileUiCase(
            "{kind:'object', fields:{"
            "  id:{kind:'key', required:true},"
            "  count:{kind:'integer', min:0, default:10},"
            "  title:{kind:'text', required:true}"
            "}}", Document, Diagnostics);
        if (Spec == nullptr) return "ui_value.object.spec_failed";

        // Positive with default materialized for absent optional 'count'
        if (!ValidateUiValueCase(*Spec, "{id:'btn_main', title:{text_id:'core:text.btn.ok'}}", Materialized, ValueDoc, Diagnostics)
            || !Diagnostics.empty()
            || !Materialized.IsObject()
            || Materialized.FindField("count") == nullptr
            || Materialized.FindField("count")->AsInteger() != 10)
        {
            return "ui_value.object.positive_with_default";
        }

        // Negative: missing required field 'id'
        if (ValidateUiValueCase(*Spec, "{title:{text_id:'core:text.btn.ok'}}", Materialized, ValueDoc, Diagnostics)
            || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.ui_schema.value.missing_field")
        {
            return "ui_value.object.missing_required";
        }

        // Negative: unknown field in object (closed schema)
        if (ValidateUiValueCase(*Spec, "{id:'btn_main', title:{text_id:'core:text.btn.ok'}, extra_field:123}", Materialized, ValueDoc, Diagnostics)
            || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.ui_schema.value.unknown_field")
        {
            return "ui_value.object.unknown_field";
        }
    }

    // 32. value.array and keyed collection: positive, duplicate key rejection, min/max item limits, nested unknown field.
    {
        const auto Spec = CompileUiCase(
            "{kind:'array', min_items:1, max_items:3, keyed_by:'key', items:{"
            "  kind:'object', fields:{"
            "    key:{kind:'key', required:true},"
            "    label:{kind:'text', required:true}"
            "  }"
            "}}", Document, Diagnostics);
        if (Spec == nullptr) return "ui_value.array.spec_failed";

        // Positive keyed collection
        if (!ValidateUiValueCase(*Spec,
            "[{key:'tab_1', label:{text_id:'core:text.tab1'}}, {key:'tab_2', label:{text_id:'core:text.tab2'}}]",
            Materialized, ValueDoc, Diagnostics)
            || !Diagnostics.empty()
            || !Materialized.IsArray()
            || Materialized.AsArray().size() != 2)
        {
            return "ui_value.array.positive";
        }

        // Negative: duplicate key in collection
        if (ValidateUiValueCase(*Spec,
            "[{key:'tab_dup', label:{text_id:'core:text.tab1'}}, {key:'tab_dup', label:{text_id:'core:text.tab2'}}]",
            Materialized, ValueDoc, Diagnostics)
            || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.ui_schema.value.duplicate_key")
        {
            return "ui_value.array.duplicate_key";
        }

        // Negative: too few items (0 < min_items:1)
        if (ValidateUiValueCase(*Spec, "[]", Materialized, ValueDoc, Diagnostics)
            || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.ui_schema.value.invalid_item_count")
        {
            return "ui_value.array.min_items";
        }

        // Negative: too many items (4 > max_items:3)
        if (ValidateUiValueCase(*Spec,
            "[{key:'k1', label:{text_id:'core:text.t'}}, {key:'k2', label:{text_id:'core:text.t'}}, "
            " {key:'k3', label:{text_id:'core:text.t'}}, {key:'k4', label:{text_id:'core:text.t'}}]",
            Materialized, ValueDoc, Diagnostics)
            || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.ui_schema.value.invalid_item_count")
        {
            return "ui_value.array.max_items";
        }

        // Negative: nested unknown field inside array item
        if (ValidateUiValueCase(*Spec,
            "[{key:'tab_1', label:{text_id:'core:text.tab1'}, extra_item_field:true}]",
            Materialized, ValueDoc, Diagnostics)
            || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.ui_schema.value.unknown_field")
        {
            return "ui_value.array.nested_unknown_field";
        }
    }

    // 33. value.schema_ref: nested schema validation with inlining and rejection of nested unknown field.
    {
        FInMemoryUiSchemaResolver Resolver;
        std::optional<FParsedDocument> TargetDoc = ParseJson5Document(
            "{id:'core:schema.ui_value.action_button.v1', schema_domain:'ui_value', schema_version:1, "
            "root:{kind:'object', fields:{"
            "  key:{kind:'key', required:true},"
            "  title:{kind:'text', required:true},"
            "  action:{kind:'binding', required:true}"
            "}}}",
            FParseLimits{}, Diagnostics);
        if (!TargetDoc.has_value()) return "ui_value.schema_ref.target_parse_failed";
        Resolver.RegisterUiSchemaDocument("core:schema.ui_value.action_button.v1", std::make_shared<FParsedDocument>(std::move(*TargetDoc)));

        const auto Spec = CompileUiCase(
            "{kind:'array', keyed_by:'key', items:{kind:'schema_ref', schema_id:'core:schema.ui_value.action_button.v1'}}",
            Document, Diagnostics, &Resolver, "core:schema.ui_field.button_list.v1", "core");
        if (Spec == nullptr) return "ui_value.schema_ref.compile_failed";

        // Positive
        if (!ValidateUiValueCase(*Spec,
            "[{key:'btn_action', title:{text_id:'core:text.btn.act'}, action:{command_id:'core:command.btn.execute'}}]",
            Materialized, ValueDoc, Diagnostics)
            || !Diagnostics.empty()
            || !Materialized.IsArray()
            || Materialized.AsArray().size() != 1)
        {
            return "ui_value.schema_ref.positive";
        }

        // Negative: unknown field inside referenced schema item
        if (ValidateUiValueCase(*Spec,
            "[{key:'btn_action', title:{text_id:'core:text.btn.act'}, action:{command_id:'core:command.btn.execute'}, unknown_ref_field:123}]",
            Materialized, ValueDoc, Diagnostics)
            || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.ui_schema.value.unknown_field")
        {
            return "ui_value.schema_ref.nested_unknown_field";
        }
    }

    return "";
}
} // namespace GV2ContentCore::Testing
