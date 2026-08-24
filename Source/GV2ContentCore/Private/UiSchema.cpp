#include "GV2ContentCore/UiSchema.h"

#include "GV2ContentCore/StableId.h"

#include <algorithm>
#include <set>

namespace GV2ContentCore
{
namespace
{
std::string EscapeJsonPointerToken(const std::string_view Token)
{
    std::string Escaped;
    for (const char Character : Token)
    {
        if (Character == '~') Escaped += "~0";
        else if (Character == '/') Escaped += "~1";
        else Escaped.push_back(Character);
    }
    return Escaped;
}

std::string ChildPointer(const std::string& Parent, const std::string_view Child)
{
    return Parent + "/" + EscapeJsonPointerToken(Child);
}

FDiagnostic MakeDiagnostic(
    std::string Code,
    std::string Message,
    const FParsedDocument* Document,
    const std::string& JsonPointer,
    const FValidationDiagnosticContext& Context,
    const bool bUseKeySpan = false)
{
    FDiagnostic Diagnostic;
    Diagnostic.Code = std::move(Code);
    Diagnostic.Message = std::move(Message);
    Diagnostic.PackageId = Context.PackageId;
    Diagnostic.PackageLoadIndex = Context.PackageLoadIndex;
    Diagnostic.RelativeSource = Context.RelativeSource;
    Diagnostic.DefinitionId = Context.DefinitionId;
    Diagnostic.SchemaId = Context.SchemaId;
    Diagnostic.SchemaVersion = Context.SchemaVersion;
    Diagnostic.JsonPointer = JsonPointer;
    if (Document != nullptr)
    {
        if (const FParsedLocation* Location = Document->FindLocation(JsonPointer))
        {
            Diagnostic.Span = bUseKeySpan && Location->KeySpan.has_value()
                ? *Location->KeySpan
                : Location->ValueSpan;
        }
        else if (const FParsedLocation* RootLocation = Document->FindLocation(""))
        {
            Diagnostic.Span = RootLocation->ValueSpan;
        }
    }
    return Diagnostic;
}

bool IsCommonUiField(const std::string_view FieldName)
{
    return FieldName == "kind" || FieldName == "required" || FieldName == "nullable"
        || FieldName == "default" || FieldName == "description";
}

bool IsCanonicalFieldName(const std::string_view Name)
{
    if (Name.empty() || Name.front() < 'a' || Name.front() > 'z' || Name.back() == '_') return false;
    bool bPreviousUnderscore = false;
    for (const char Character : Name)
    {
        const bool bUnderscore = Character == '_';
        if (!bUnderscore && !(Character >= 'a' && Character <= 'z') && !(Character >= '0' && Character <= '9')) return false;
        if (bUnderscore && bPreviousUnderscore) return false;
        bPreviousUnderscore = bUnderscore;
    }
    return true;
}

std::optional<std::size_t> ReadSize(
    const FValue& Source,
    const std::string_view Name,
    const FParsedDocument* Document,
    const std::string& Pointer,
    const FValidationDiagnosticContext& Context,
    std::vector<FDiagnostic>& Diagnostics)
{
    const FValue* Value = Source.FindField(Name);
    if (Value == nullptr) return std::nullopt;
    if (!Value->IsInteger() || Value->AsInteger() < 0)
    {
        Diagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.ui_schema.field_spec.invalid_constraint",
            std::string(Name) + " must be a non-negative int64",
            Document, ChildPointer(Pointer, Name), Context));
        return std::nullopt;
    }
    return static_cast<std::size_t>(Value->AsInteger());
}

bool CheckClosedUiSpec(
    const FValue& Source,
    const std::set<std::string_view>& Specific,
    const std::string& Kind,
    const FParsedDocument* Document,
    const std::string& Pointer,
    const FValidationDiagnosticContext& Context,
    std::vector<FDiagnostic>& Diagnostics)
{
    const std::size_t InitialCount = Diagnostics.size();
    for (const auto& [Name, Value] : Source.AsObject())
    {
        (void)Value;
        if (!IsCommonUiField(Name) && !Specific.contains(Name))
        {
            Diagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.ui_schema.field_spec.unknown_field",
                "Unknown field for UI FieldSpec kind " + Kind + ": " + Name,
                Document, ChildPointer(Pointer, Name), Context));
        }
    }
    return Diagnostics.size() == InitialCount;
}
}

std::optional<EUiSchemaDomain> ParseUiSchemaDomain(const std::string_view Value)
{
    if (Value == "ui_field") return EUiSchemaDomain::UiField;
    if (Value == "ui_value") return EUiSchemaDomain::UiValue;
    return std::nullopt;
}

std::string_view ToString(const EUiSchemaDomain Domain)
{
    switch (Domain)
    {
    case EUiSchemaDomain::UiField: return "ui_field";
    case EUiSchemaDomain::UiValue: return "ui_value";
    }
    return "";
}

FCompiledUiFieldSpecPtr CompileUiFieldSpec(
    const FValue& FieldSpec,
    const FParsedDocument* SchemaDocument,
    std::string SchemaJsonPointer,
    const FValidationDiagnosticContext& Context,
    std::vector<FDiagnostic>& OutDiagnostics)
{
    const std::size_t InitialCount = OutDiagnostics.size();
    const std::string& Pointer = SchemaJsonPointer;

    if (!FieldSpec.IsObject())
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.ui_schema.field_spec.invalid_shape", "UI FieldSpec must be an object",
            SchemaDocument, Pointer, Context));
        return nullptr;
    }

    const FValue* KindValue = FieldSpec.FindField("kind");
    if (KindValue == nullptr || !KindValue->IsString())
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.ui_schema.field_spec.invalid_kind", "UI FieldSpec requires a string kind",
            SchemaDocument, ChildPointer(Pointer, "kind"), Context));
        return nullptr;
    }
    const std::string& Kind = KindValue->AsString();

    bool bNullable = false;
    if (const FValue* Nullable = FieldSpec.FindField("nullable"))
    {
        if (!Nullable->IsBoolean())
        {
            OutDiagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.ui_schema.field_spec.invalid_constraint", "nullable must be boolean",
                SchemaDocument, ChildPointer(Pointer, "nullable"), Context));
        }
        else bNullable = Nullable->AsBoolean();
    }
    if (const FValue* Required = FieldSpec.FindField("required"); Required != nullptr && !Required->IsBoolean())
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.ui_schema.field_spec.invalid_constraint", "required must be boolean",
            SchemaDocument, ChildPointer(Pointer, "required"), Context));
    }
    if (const FValue* Description = FieldSpec.FindField("description"); Description != nullptr && !Description->IsString())
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.ui_schema.field_spec.invalid_constraint", "description must be string",
            SchemaDocument, ChildPointer(Pointer, "description"), Context));
    }

    auto Result = std::make_shared<FCompiledUiFieldSpec>();
    Result->bNullable = bNullable;

    static const std::set<std::string_view> ScalarKinds = { "bool", "integer", "number", "string" };
    if (ScalarKinds.contains(Kind))
    {
        // UI schemas spell the integer scalar kind "integer", not the
        // definition-schema "int64" — key is its own kind and is never a
        // shorthand for `string`, so no other UI kind name gets remapped
        // here.
        FValue Shadow = FieldSpec;
        if (Kind == "integer")
        {
            for (auto& Field : Shadow.AsObject())
            {
                if (Field.first == "kind")
                {
                    Field.second = FValue::MakeString("int64");
                    break;
                }
            }
        }
        const auto Scalar = CompileScalarFieldSpec(Shadow, SchemaDocument, Pointer, Context, OutDiagnostics);
        if (Scalar.has_value())
        {
            Result->Kind = EUiFieldKind::Scalar;
            Result->Scalar = *Scalar;
            Result->bNullable = Scalar->bNullable;
        }
        if (const FValue* Default = FieldSpec.FindField("default"); Default != nullptr && Result->Scalar.has_value())
        {
            std::vector<FDiagnostic> DefaultDiagnostics;
            if (ValidateScalarValue(*Default, *Result->Scalar, SchemaDocument, ChildPointer(Pointer, "default"), Context, DefaultDiagnostics))
            {
                Result->DefaultValue = *Default;
            }
            else
            {
                OutDiagnostics.insert(OutDiagnostics.end(), DefaultDiagnostics.begin(), DefaultDiagnostics.end());
            }
        }
        return OutDiagnostics.size() == InitialCount ? Result : nullptr;
    }

    // `default` is only meaningful for a scalar leaf; every structural or
    // semantic kind rejects it outright instead of silently ignoring it.
    if (FieldSpec.FindField("default") != nullptr)
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.ui_schema.field_spec.invalid_default",
            "default is only allowed on scalar kinds (bool, integer, number, string)",
            SchemaDocument, ChildPointer(Pointer, "default"), Context));
    }

    if (Kind == "key")
    {
        Result->Kind = EUiFieldKind::Key;
        CheckClosedUiSpec(FieldSpec, {}, Kind, SchemaDocument, Pointer, Context, OutDiagnostics);
    }
    else if (Kind == "text")
    {
        Result->Kind = EUiFieldKind::Text;
        CheckClosedUiSpec(FieldSpec, {}, Kind, SchemaDocument, Pointer, Context, OutDiagnostics);
    }
    else if (Kind == "ref")
    {
        Result->Kind = EUiFieldKind::Ref;
        CheckClosedUiSpec(FieldSpec, { "target_kind" }, Kind, SchemaDocument, Pointer, Context, OutDiagnostics);
        const FValue* TargetKind = FieldSpec.FindField("target_kind");
        if (TargetKind == nullptr || !TargetKind->IsString()
            || !FStableId::IsValidSegment(TargetKind->IsString() ? TargetKind->AsString() : ""))
        {
            OutDiagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.ui_schema.field_spec.invalid_target_kind",
                "ref requires target_kind as a canonical Stable ID segment",
                SchemaDocument, ChildPointer(Pointer, "target_kind"), Context));
        }
        else Result->RefTargetKind = TargetKind->AsString();
    }
    else if (Kind == "binding")
    {
        Result->Kind = EUiFieldKind::Binding;
        CheckClosedUiSpec(FieldSpec, { "input_schema_id" }, Kind, SchemaDocument, Pointer, Context, OutDiagnostics);
        if (const FValue* InputSchemaId = FieldSpec.FindField("input_schema_id"))
        {
            if (!InputSchemaId->IsString() || !FStableId::IsOfKind(InputSchemaId->AsString(), "schema"))
            {
                OutDiagnostics.push_back(MakeDiagnostic(
                    "core:diagnostic.ui_schema.field_spec.invalid_input_schema_id",
                    "binding input_schema_id must be a Stable ID of kind 'schema'",
                    SchemaDocument, ChildPointer(Pointer, "input_schema_id"), Context));
            }
            else Result->BindingInputSchemaId = InputSchemaId->AsString();
        }
    }
    else if (Kind == "object")
    {
        Result->Kind = EUiFieldKind::Object;
        CheckClosedUiSpec(FieldSpec, { "fields" }, Kind, SchemaDocument, Pointer, Context, OutDiagnostics);
        const FValue* Fields = FieldSpec.FindField("fields");
        if (Fields == nullptr || !Fields->IsObject())
        {
            OutDiagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.ui_schema.field_spec.invalid_fields", "object requires a fields object",
                SchemaDocument, ChildPointer(Pointer, "fields"), Context));
        }
        else
        {
            for (const auto& [Name, Child] : Fields->AsObject())
            {
                const std::string ChildSpecPointer = ChildPointer(ChildPointer(Pointer, "fields"), Name);
                if (!IsCanonicalFieldName(Name))
                {
                    OutDiagnostics.push_back(MakeDiagnostic(
                        "core:diagnostic.ui_schema.field_spec.invalid_field_name",
                        "object field name must be canonical snake_case",
                        SchemaDocument, ChildSpecPointer, Context, true));
                }
                bool bChildRequired = false;
                if (const FValue* ChildRequired = Child.IsObject() ? Child.FindField("required") : nullptr;
                    ChildRequired != nullptr && ChildRequired->IsBoolean())
                {
                    bChildRequired = ChildRequired->AsBoolean();
                }
                FCompiledUiFieldSpecPtr ChildSpec = CompileUiFieldSpec(
                    Child, SchemaDocument, ChildSpecPointer, Context, OutDiagnostics);
                if (ChildSpec != nullptr) Result->Fields.push_back({ Name, bChildRequired, std::move(ChildSpec) });
            }
        }
    }
    else if (Kind == "array")
    {
        Result->Kind = EUiFieldKind::Array;
        CheckClosedUiSpec(FieldSpec, { "items", "min_items", "max_items", "keyed_by" }, Kind, SchemaDocument, Pointer, Context, OutDiagnostics);
        const FValue* Items = FieldSpec.FindField("items");
        if (Items == nullptr)
        {
            OutDiagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.ui_schema.field_spec.missing_constraint", "array requires items",
                SchemaDocument, ChildPointer(Pointer, "items"), Context));
        }
        else Result->Items = CompileUiFieldSpec(*Items, SchemaDocument, ChildPointer(Pointer, "items"), Context, OutDiagnostics);
        Result->MinimumItems = ReadSize(FieldSpec, "min_items", SchemaDocument, Pointer, Context, OutDiagnostics);
        Result->MaximumItems = ReadSize(FieldSpec, "max_items", SchemaDocument, Pointer, Context, OutDiagnostics);
        if (const FValue* KeyedBy = FieldSpec.FindField("keyed_by"))
        {
            if (!KeyedBy->IsString() || KeyedBy->AsString().empty())
            {
                OutDiagnostics.push_back(MakeDiagnostic(
                    "core:diagnostic.ui_schema.field_spec.invalid_keyed_by",
                    "keyed_by must be a non-empty string naming an items object field",
                    SchemaDocument, ChildPointer(Pointer, "keyed_by"), Context));
            }
            else if (Result->Items != nullptr)
            {
                const std::string& KeyField = KeyedBy->AsString();
                const auto It = std::find_if(
                    Result->Items->Fields.begin(), Result->Items->Fields.end(),
                    [&KeyField](const FCompiledUiObjectField& Field) { return Field.Name == KeyField; });
                if (Result->Items->Kind != EUiFieldKind::Object
                    || It == Result->Items->Fields.end()
                    || It->Spec->Kind != EUiFieldKind::Key)
                {
                    OutDiagnostics.push_back(MakeDiagnostic(
                        "core:diagnostic.ui_schema.field_spec.invalid_keyed_by",
                        "keyed_by must name an items object field compiled as kind 'key'; 'string' does not qualify",
                        SchemaDocument, ChildPointer(Pointer, "keyed_by"), Context));
                }
                else Result->KeyedBy = KeyField;
            }
        }
    }
    else if (Kind == "screen_fields")
    {
        Result->Kind = EUiFieldKind::ScreenFields;
        CheckClosedUiSpec(FieldSpec, {}, Kind, SchemaDocument, Pointer, Context, OutDiagnostics);
    }
    else
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.ui_schema.field_spec.invalid_kind",
            "unknown UI FieldSpec kind '" + Kind + "'",
            SchemaDocument, ChildPointer(Pointer, "kind"), Context));
        return nullptr;
    }

    return OutDiagnostics.size() == InitialCount ? Result : nullptr;
}
}
