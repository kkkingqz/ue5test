#include "GV2ContentCore/ScalarValidation.h"

#include "GV2ContentCore/StableId.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <locale>
#include <set>
#include <string_view>

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

FDiagnostic MakeDiagnostic(
    std::string Code,
    std::string Message,
    const FParsedDocument* Document,
    const std::string& JsonPointer,
    const FValidationDiagnosticContext& Context)
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
            Diagnostic.Span = Location->ValueSpan;
        }
        else if (const FParsedLocation* RootLocation = Document->FindLocation(""))
        {
            Diagnostic.Span = RootLocation->ValueSpan;
        }
    }
    return Diagnostic;
}

std::string ChildPointer(const std::string& Parent, const std::string_view Child)
{
    return Parent + "/" + EscapeJsonPointerToken(Child);
}

std::size_t Utf8CodePointCount(const std::string& Value)
{
    return static_cast<std::size_t>(std::count_if(
        Value.begin(),
        Value.end(),
        [](const char Character)
        {
            return (static_cast<unsigned char>(Character) & 0xC0) != 0x80;
        }));
}

bool ReadOptionalBoolean(
    const FValue& Object,
    const std::string_view FieldName,
    bool& OutValue,
    const FParsedDocument* Document,
    const std::string& Pointer,
    const FValidationDiagnosticContext& Context,
    std::vector<FDiagnostic>& OutDiagnostics)
{
    const FValue* Value = Object.FindField(FieldName);
    if (Value == nullptr) return true;
    if (!Value->IsBoolean())
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.field_spec.invalid_constraint",
            std::string(FieldName) + " must be boolean",
            Document, ChildPointer(Pointer, FieldName), Context));
        return false;
    }
    OutValue = Value->AsBoolean();
    return true;
}

std::optional<std::int64_t> ReadOptionalInteger(
    const FValue& Object,
    const std::string_view FieldName,
    const bool bNonNegative,
    const FParsedDocument* Document,
    const std::string& Pointer,
    const FValidationDiagnosticContext& Context,
    std::vector<FDiagnostic>& OutDiagnostics)
{
    const FValue* Value = Object.FindField(FieldName);
    if (Value == nullptr) return std::nullopt;
    if (!Value->IsInteger() || (bNonNegative && Value->AsInteger() < 0))
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.field_spec.invalid_constraint",
            std::string(FieldName) + (bNonNegative ? " must be a non-negative int64" : " must be int64"),
            Document, ChildPointer(Pointer, FieldName), Context));
        return std::nullopt;
    }
    return Value->AsInteger();
}

std::optional<double> ReadOptionalNumber(
    const FValue& Object,
    const std::string_view FieldName,
    const FParsedDocument* Document,
    const std::string& Pointer,
    const FValidationDiagnosticContext& Context,
    std::vector<FDiagnostic>& OutDiagnostics)
{
    const FValue* Value = Object.FindField(FieldName);
    if (Value == nullptr) return std::nullopt;
    if (!Value->IsInteger() && !Value->IsNumber())
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.field_spec.invalid_constraint",
            std::string(FieldName) + " must be an int64 or finite number bound",
            Document, ChildPointer(Pointer, FieldName), Context));
        return std::nullopt;
    }
    return Value->IsInteger() ? static_cast<double>(Value->AsInteger()) : Value->AsNumber();
}

bool IsAllowedCommonField(const std::string_view FieldName)
{
    return FieldName == "kind"
        || FieldName == "required"
        || FieldName == "nullable"
        || FieldName == "default"
        || FieldName == "description"
        || FieldName == "storage"
        || FieldName == "write_policy"
        || FieldName == "operations";
}

bool IsScalarEnumValue(const FValue& Value)
{
    return Value.IsBoolean() || Value.IsInteger() || Value.IsNumber() || Value.IsString();
}
}

bool IsScalarFieldKind(const std::string_view Kind)
{
    return Kind == "bool" || Kind == "int64" || Kind == "number" || Kind == "string" || Kind == "enum";
}

std::optional<FScalarFieldSpec> CompileScalarFieldSpec(
    const FValue& FieldSpec,
    const FParsedDocument* SchemaDocument,
    std::string SchemaJsonPointer,
    const FValidationDiagnosticContext& Context,
    std::vector<FDiagnostic>& OutDiagnostics)
{
    const std::size_t InitialDiagnosticCount = OutDiagnostics.size();
    if (!FieldSpec.IsObject())
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.field_spec.invalid_shape",
            "FieldSpec must be an object",
            SchemaDocument, SchemaJsonPointer, Context));
        return std::nullopt;
    }

    const FValue* KindValue = FieldSpec.FindField("kind");
    if (KindValue == nullptr || !KindValue->IsString() || !IsScalarFieldKind(KindValue->IsString() ? KindValue->AsString() : ""))
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.field_spec.invalid_kind",
            "Scalar FieldSpec requires kind bool, int64, number, string or enum",
            SchemaDocument, ChildPointer(SchemaJsonPointer, "kind"), Context));
        return std::nullopt;
    }

    const std::string& Kind = KindValue->AsString();
    std::set<std::string_view> AllowedSpecificFields;
    FScalarFieldSpec Compiled;
    if (Kind == "bool") Compiled.Kind = EScalarFieldKind::Boolean;
    else if (Kind == "int64")
    {
        Compiled.Kind = EScalarFieldKind::Integer;
        AllowedSpecificFields = { "min", "max" };
    }
    else if (Kind == "number")
    {
        Compiled.Kind = EScalarFieldKind::Number;
        AllowedSpecificFields = { "min", "max", "exclusive_min", "exclusive_max" };
    }
    else if (Kind == "string")
    {
        Compiled.Kind = EScalarFieldKind::String;
        AllowedSpecificFields = { "min_length", "max_length", "pattern", "format" };
    }
    else
    {
        Compiled.Kind = EScalarFieldKind::Enum;
        AllowedSpecificFields = { "values" };
    }

    for (const auto& [FieldName, FieldValue] : FieldSpec.AsObject())
    {
        if (!IsAllowedCommonField(FieldName) && !AllowedSpecificFields.contains(FieldName))
        {
            OutDiagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.field_spec.unknown_field",
                "Unknown field for scalar FieldSpec kind " + Kind + ": " + FieldName,
                SchemaDocument, ChildPointer(SchemaJsonPointer, FieldName), Context));
        }
    }

    ReadOptionalBoolean(
        FieldSpec, "nullable", Compiled.bNullable,
        SchemaDocument, SchemaJsonPointer, Context, OutDiagnostics);
    if (const FValue* Required = FieldSpec.FindField("required"); Required != nullptr && !Required->IsBoolean())
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.field_spec.invalid_constraint",
            "required must be boolean",
            SchemaDocument, ChildPointer(SchemaJsonPointer, "required"), Context));
    }
    if (const FValue* Description = FieldSpec.FindField("description"); Description != nullptr && !Description->IsString())
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.field_spec.invalid_constraint",
            "description must be string",
            SchemaDocument, ChildPointer(SchemaJsonPointer, "description"), Context));
    }
    // Recursive FieldSpec compilation validates and stores explicit defaults.

    if (Compiled.Kind == EScalarFieldKind::Integer)
    {
        Compiled.MinimumInteger = ReadOptionalInteger(
            FieldSpec, "min", false, SchemaDocument, SchemaJsonPointer, Context, OutDiagnostics);
        Compiled.MaximumInteger = ReadOptionalInteger(
            FieldSpec, "max", false, SchemaDocument, SchemaJsonPointer, Context, OutDiagnostics);
        if (Compiled.MinimumInteger.has_value()
            && Compiled.MaximumInteger.has_value()
            && *Compiled.MinimumInteger > *Compiled.MaximumInteger)
        {
            OutDiagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.field_spec.invalid_constraint_range",
                "int64 min must not exceed max",
                SchemaDocument, SchemaJsonPointer, Context));
        }
    }
    else if (Compiled.Kind == EScalarFieldKind::Number)
    {
        const bool bHasMin = FieldSpec.FindField("min") != nullptr;
        const bool bHasExclusiveMin = FieldSpec.FindField("exclusive_min") != nullptr;
        const bool bHasMax = FieldSpec.FindField("max") != nullptr;
        const bool bHasExclusiveMax = FieldSpec.FindField("exclusive_max") != nullptr;
        if (bHasMin && bHasExclusiveMin)
        {
            OutDiagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.field_spec.conflicting_constraint",
                "number min and exclusive_min are mutually exclusive",
                SchemaDocument, SchemaJsonPointer, Context));
        }
        if (bHasMax && bHasExclusiveMax)
        {
            OutDiagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.field_spec.conflicting_constraint",
                "number max and exclusive_max are mutually exclusive",
                SchemaDocument, SchemaJsonPointer, Context));
        }
        const std::string_view MinimumField = bHasExclusiveMin ? "exclusive_min" : "min";
        const std::string_view MaximumField = bHasExclusiveMax ? "exclusive_max" : "max";
        Compiled.MinimumNumber = ReadOptionalNumber(
            FieldSpec, MinimumField, SchemaDocument, SchemaJsonPointer, Context, OutDiagnostics);
        Compiled.MaximumNumber = ReadOptionalNumber(
            FieldSpec, MaximumField, SchemaDocument, SchemaJsonPointer, Context, OutDiagnostics);
        Compiled.bMinimumExclusive = bHasExclusiveMin;
        Compiled.bMaximumExclusive = bHasExclusiveMax;
        if (Compiled.MinimumNumber.has_value() && Compiled.MaximumNumber.has_value())
        {
            const bool bInvalidRange = *Compiled.MinimumNumber > *Compiled.MaximumNumber
                || (*Compiled.MinimumNumber == *Compiled.MaximumNumber
                    && (Compiled.bMinimumExclusive || Compiled.bMaximumExclusive));
            if (bInvalidRange)
            {
                OutDiagnostics.push_back(MakeDiagnostic(
                    "core:diagnostic.schema.field_spec.invalid_constraint_range",
                    "number lower bound must permit at least one value below upper bound",
                    SchemaDocument, SchemaJsonPointer, Context));
            }
        }
    }
    else if (Compiled.Kind == EScalarFieldKind::String)
    {
        const std::optional<std::int64_t> MinimumLength = ReadOptionalInteger(
            FieldSpec, "min_length", true, SchemaDocument, SchemaJsonPointer, Context, OutDiagnostics);
        const std::optional<std::int64_t> MaximumLength = ReadOptionalInteger(
            FieldSpec, "max_length", true, SchemaDocument, SchemaJsonPointer, Context, OutDiagnostics);
        if (MinimumLength.has_value()) Compiled.MinimumLength = static_cast<std::size_t>(*MinimumLength);
        if (MaximumLength.has_value()) Compiled.MaximumLength = static_cast<std::size_t>(*MaximumLength);
        if (Compiled.MinimumLength.has_value()
            && Compiled.MaximumLength.has_value()
            && *Compiled.MinimumLength > *Compiled.MaximumLength)
        {
            OutDiagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.field_spec.invalid_constraint_range",
                "string min_length must not exceed max_length",
                SchemaDocument, SchemaJsonPointer, Context));
        }

        if (const FValue* Pattern = FieldSpec.FindField("pattern"))
        {
            if (!Pattern->IsString())
            {
                OutDiagnostics.push_back(MakeDiagnostic(
                    "core:diagnostic.schema.field_spec.invalid_constraint",
                    "pattern must be string",
                    SchemaDocument, ChildPointer(SchemaJsonPointer, "pattern"), Context));
            }
            else
            {
                try
                {
                    Compiled.Pattern = Pattern->AsString();
                    std::regex PortablePattern;
                    PortablePattern.imbue(std::locale::classic());
                    PortablePattern.assign(*Compiled.Pattern, std::regex::ECMAScript | std::regex::optimize);
                    Compiled.CompiledPattern = std::move(PortablePattern);
                }
                catch (const std::regex_error&)
                {
                    OutDiagnostics.push_back(MakeDiagnostic(
                        "core:diagnostic.schema.field_spec.invalid_pattern",
                        "pattern is not a valid ECMAScript regular expression",
                        SchemaDocument, ChildPointer(SchemaJsonPointer, "pattern"), Context));
                }
            }
        }
        if (const FValue* Format = FieldSpec.FindField("format"))
        {
            if (!Format->IsString()
                || (Format->AsString() != "stable_id" && Format->AsString() != "stable_id_segment"))
            {
                OutDiagnostics.push_back(MakeDiagnostic(
                    "core:diagnostic.schema.field_spec.invalid_format",
                    "format must be stable_id or stable_id_segment",
                    SchemaDocument, ChildPointer(SchemaJsonPointer, "format"), Context));
            }
            else
            {
                Compiled.Format = Format->AsString();
            }
        }
    }
    else if (Compiled.Kind == EScalarFieldKind::Enum)
    {
        const FValue* Values = FieldSpec.FindField("values");
        if (Values == nullptr || !Values->IsArray() || Values->AsArray().empty())
        {
            OutDiagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.field_spec.invalid_enum_values",
                "enum values must be a non-empty array",
                SchemaDocument, ChildPointer(SchemaJsonPointer, "values"), Context));
        }
        else
        {
            for (std::size_t Index = 0; Index < Values->AsArray().size(); ++Index)
            {
                const FValue& EnumValue = Values->AsArray()[Index];
                const bool bDuplicate = std::find(
                    Compiled.EnumValues.begin(), Compiled.EnumValues.end(), EnumValue)
                    != Compiled.EnumValues.end();
                if (!IsScalarEnumValue(EnumValue) || bDuplicate)
                {
                    OutDiagnostics.push_back(MakeDiagnostic(
                        bDuplicate
                            ? "core:diagnostic.schema.field_spec.duplicate_enum_value"
                            : "core:diagnostic.schema.field_spec.invalid_enum_value",
                        bDuplicate ? "enum values must be unique" : "enum values must be non-null scalars",
                        SchemaDocument,
                        ChildPointer(ChildPointer(SchemaJsonPointer, "values"), std::to_string(Index)),
                        Context));
                }
                else
                {
                    Compiled.EnumValues.push_back(EnumValue);
                }
            }
        }
    }

    if (OutDiagnostics.size() != InitialDiagnosticCount)
    {
        return std::nullopt;
    }
    return Compiled;
}

bool ValidateScalarValue(
    const FValue& Value,
    const FScalarFieldSpec& FieldSpec,
    const FParsedDocument* ValueDocument,
    std::string ValueJsonPointer,
    const FValidationDiagnosticContext& Context,
    std::vector<FDiagnostic>& OutDiagnostics)
{
    if (Value.IsNull())
    {
        if (FieldSpec.bNullable) return true;
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.value.null_not_allowed",
            "Explicit null is not allowed by this FieldSpec",
            ValueDocument, ValueJsonPointer, Context));
        return false;
    }

    bool bCorrectType = false;
    switch (FieldSpec.Kind)
    {
    case EScalarFieldKind::Boolean: bCorrectType = Value.IsBoolean(); break;
    case EScalarFieldKind::Integer: bCorrectType = Value.IsInteger(); break;
    case EScalarFieldKind::Number: bCorrectType = Value.IsNumber(); break;
    case EScalarFieldKind::String: bCorrectType = Value.IsString(); break;
    case EScalarFieldKind::Enum:
        bCorrectType = std::find(FieldSpec.EnumValues.begin(), FieldSpec.EnumValues.end(), Value)
            != FieldSpec.EnumValues.end();
        break;
    }
    if (!bCorrectType)
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            FieldSpec.Kind == EScalarFieldKind::Enum
                ? "core:diagnostic.schema.value.enum_not_allowed"
                : "core:diagnostic.schema.value.type_mismatch",
            FieldSpec.Kind == EScalarFieldKind::Enum
                ? "Value is not one of the declared enum values"
                : "Value kind does not match scalar FieldSpec; coercion is prohibited",
            ValueDocument, ValueJsonPointer, Context));
        return false;
    }

    if (FieldSpec.Kind == EScalarFieldKind::Integer)
    {
        const std::int64_t Integer = Value.AsInteger();
        if ((FieldSpec.MinimumInteger.has_value() && Integer < *FieldSpec.MinimumInteger)
            || (FieldSpec.MaximumInteger.has_value() && Integer > *FieldSpec.MaximumInteger))
        {
            OutDiagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.value.constraint_failed",
                "int64 value violates min/max constraint",
                ValueDocument, ValueJsonPointer, Context));
            return false;
        }
    }
    else if (FieldSpec.Kind == EScalarFieldKind::Number)
    {
        const double Number = Value.AsNumber();
        const bool bBelowMinimum = FieldSpec.MinimumNumber.has_value()
            && (FieldSpec.bMinimumExclusive ? Number <= *FieldSpec.MinimumNumber : Number < *FieldSpec.MinimumNumber);
        const bool bAboveMaximum = FieldSpec.MaximumNumber.has_value()
            && (FieldSpec.bMaximumExclusive ? Number >= *FieldSpec.MaximumNumber : Number > *FieldSpec.MaximumNumber);
        if (bBelowMinimum || bAboveMaximum)
        {
            OutDiagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.value.constraint_failed",
                "number value violates declared bounds",
                ValueDocument, ValueJsonPointer, Context));
            return false;
        }
    }
    else if (FieldSpec.Kind == EScalarFieldKind::String)
    {
        const std::string& String = Value.AsString();
        const std::size_t Length = Utf8CodePointCount(String);
        bool bValid = (!FieldSpec.MinimumLength.has_value() || Length >= *FieldSpec.MinimumLength)
            && (!FieldSpec.MaximumLength.has_value() || Length <= *FieldSpec.MaximumLength);
        if (bValid && FieldSpec.CompiledPattern.has_value())
        {
            bValid = std::regex_match(String, *FieldSpec.CompiledPattern);
        }
        if (bValid && FieldSpec.Format.has_value())
        {
            bValid = *FieldSpec.Format == "stable_id"
                ? FStableId::IsValid(String)
                : FStableId::IsValidSegment(String);
        }
        if (!bValid)
        {
            OutDiagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.value.constraint_failed",
                "string value violates length, pattern or format constraint",
                ValueDocument, ValueJsonPointer, Context));
            return false;
        }
    }
    return true;
}
}
