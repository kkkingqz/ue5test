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
    std::vector<FDiagnostic>& OutDiagnostics)
{
    OutDiagnostics.clear();
    OutDocument = ParseJson5Document(Json, FParseLimits{}, OutDiagnostics);
    if (!OutDocument.has_value() || !OutDiagnostics.empty()) return nullptr;
    const FValidationDiagnosticContext Context;
    OutDiagnostics.clear();
    return CompileUiFieldSpec(OutDocument->GetRootValue(), &*OutDocument, "", Context, OutDiagnostics);
}
}

std::string RunUiSchemaConformance()
{
    std::optional<FParsedDocument> Document;
    std::vector<FDiagnostic> Diagnostics;

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

    // 12. unknown kind — `schema_ref` and `enum` are explicitly outside the standard UI
    // kind vocabulary at this stage; both must fail typed, not silently pass through.
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

    return "";
}
} // namespace GV2ContentCore::Testing
