#include "GV2ContentCore/AuthoringMetadata.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"

#include <set>

namespace GV2ContentCore
{

namespace
{

FDiagnostic MakeUiMetaDiagnostic(
    const std::string& Code,
    std::string Message,
    const std::string& PackageId,
    const std::string& RelativeSource,
    std::optional<FSourceSpan> Span = std::nullopt)
{
    FDiagnostic Diag;
    Diag.Code = Code;
    Diag.Severity = EDiagnosticSeverity::Error;
    Diag.Message = std::move(Message);
    if (!PackageId.empty())
    {
        Diag.PackageId = PackageId;
    }
    if (!RelativeSource.empty())
    {
        Diag.RelativeSource = RelativeSource;
    }
    Diag.Span = Span;
    return Diag;
}

const std::set<std::string> AllowedRootProperties = {
    "fields",
    "schema_id",
    "schema_version",
    "definition_type"
};

const std::set<std::string> AllowedFieldProperties = {
    "label",
    "description",
    "category",
    "order",
    "widget_hint"
};

std::optional<FSourceSpan> GetValueSpan(const FParsedDocument& Doc, std::string_view Pointer)
{
    const FParsedLocation* Loc = Doc.FindLocation(Pointer);
    return Loc ? std::make_optional(Loc->ValueSpan) : std::nullopt;
}

std::optional<FSourceSpan> GetKeySpan(const FParsedDocument& Doc, std::string_view Pointer)
{
    const FParsedLocation* Loc = Doc.FindLocation(Pointer);
    return Loc ? Loc->KeySpan : std::nullopt;
}

} // namespace

std::optional<FSchemaUiMetadata> ParseSchemaUiMetadata(
    const std::string& Content,
    const FSchemaResource& Schema,
    std::vector<FDiagnostic>& OutDiagnostics,
    const std::string& PackageId,
    const std::string& RelativeSource)
{
    FParseLimits Limits;
    std::vector<FDiagnostic> ParseDiags;
    auto ParsedDoc = ParseJson5Document(Content, Limits, ParseDiags, PackageId, 0, RelativeSource);
    if (!ParsedDoc)
    {
        OutDiagnostics.insert(OutDiagnostics.end(), ParseDiags.begin(), ParseDiags.end());
        return std::nullopt;
    }

    const FValue& RootValue = ParsedDoc->GetRootValue();
    if (!RootValue.IsObject())
    {
        OutDiagnostics.push_back(MakeUiMetaDiagnostic(
            "core:diagnostic.schema.ui_metadata.invalid_shape",
            "UI metadata root must be an object",
            PackageId,
            RelativeSource,
            GetValueSpan(*ParsedDoc, "")));
        return std::nullopt;
    }

    const std::size_t InitialDiagCount = OutDiagnostics.size();

    // Check unknown root properties
    for (const auto& [Key, Val] : RootValue.AsObject())
    {
        if (AllowedRootProperties.find(Key) == AllowedRootProperties.end())
        {
            OutDiagnostics.push_back(MakeUiMetaDiagnostic(
                "core:diagnostic.schema.ui_metadata.unknown_field",
                "unknown property '" + Key + "' in UI metadata root",
                PackageId,
                RelativeSource,
                GetKeySpan(*ParsedDoc, "/" + Key)));
        }
    }

    const FValue* FieldsVal = RootValue.FindField("fields");
    if (FieldsVal == nullptr)
    {
        OutDiagnostics.push_back(MakeUiMetaDiagnostic(
            "core:diagnostic.schema.ui_metadata.missing_fields",
            "UI metadata root must declare a 'fields' object",
            PackageId,
            RelativeSource,
            GetValueSpan(*ParsedDoc, "")));
        return std::nullopt;
    }

    if (!FieldsVal->IsObject())
    {
        OutDiagnostics.push_back(MakeUiMetaDiagnostic(
            "core:diagnostic.schema.ui_metadata.invalid_shape",
            "'fields' must be an object",
            PackageId,
            RelativeSource,
            GetValueSpan(*ParsedDoc, "/fields")));
        return std::nullopt;
    }

    const FCompiledFieldSpec* RootSpec = Schema.GetCompiledRootSpec().get();
    if (RootSpec == nullptr || RootSpec->Kind != EFieldKind::Object)
    {
        OutDiagnostics.push_back(MakeUiMetaDiagnostic(
            "core:diagnostic.schema.ui_metadata.invalid_schema",
            "schema root spec is not an object",
            PackageId,
            RelativeSource,
            GetValueSpan(*ParsedDoc, "")));
        return std::nullopt;
    }

    FSchemaUiMetadata Result(RelativeSource);

    for (const auto& [FieldName, FieldVal] : FieldsVal->AsObject())
    {
        const std::string FieldPointer = "/fields/" + FieldName;

        // Verify that FieldName exists in Schema
        bool bFieldExists = false;
        for (const auto& SchemaField : RootSpec->Fields)
        {
            if (SchemaField.Name == FieldName)
            {
                bFieldExists = true;
                break;
            }
        }

        if (!bFieldExists)
        {
            OutDiagnostics.push_back(MakeUiMetaDiagnostic(
                "core:diagnostic.schema.ui_metadata.unresolved_field",
                "field '" + FieldName + "' declared in UI metadata does not exist in schema '" + Schema.GetSchemaId() + "'",
                PackageId,
                RelativeSource,
                GetKeySpan(*ParsedDoc, FieldPointer)));
            continue;
        }

        if (!FieldVal.IsObject())
        {
            OutDiagnostics.push_back(MakeUiMetaDiagnostic(
                "core:diagnostic.schema.ui_metadata.invalid_shape",
                "field metadata for '" + FieldName + "' must be an object",
                PackageId,
                RelativeSource,
                GetValueSpan(*ParsedDoc, FieldPointer)));
            continue;
        }

        FFieldUiMetadata FieldMeta;

        for (const auto& [PropName, PropVal] : FieldVal.AsObject())
        {
            const std::string PropPointer = FieldPointer + "/" + PropName;

            if (AllowedFieldProperties.find(PropName) == AllowedFieldProperties.end())
            {
                OutDiagnostics.push_back(MakeUiMetaDiagnostic(
                    "core:diagnostic.schema.ui_metadata.unknown_field",
                    "unknown property '" + PropName + "' in field metadata for '" + FieldName + "'",
                    PackageId,
                    RelativeSource,
                    GetKeySpan(*ParsedDoc, PropPointer)));
                continue;
            }

            if (PropName == "label")
            {
                if (!PropVal.IsString())
                {
                    OutDiagnostics.push_back(MakeUiMetaDiagnostic(
                        "core:diagnostic.schema.ui_metadata.invalid_type",
                        "'label' must be a string",
                        PackageId,
                        RelativeSource,
                        GetValueSpan(*ParsedDoc, PropPointer)));
                }
                else
                {
                    FieldMeta.Label = PropVal.AsString();
                }
            }
            else if (PropName == "description")
            {
                if (!PropVal.IsString())
                {
                    OutDiagnostics.push_back(MakeUiMetaDiagnostic(
                        "core:diagnostic.schema.ui_metadata.invalid_type",
                        "'description' must be a string",
                        PackageId,
                        RelativeSource,
                        GetValueSpan(*ParsedDoc, PropPointer)));
                }
                else
                {
                    FieldMeta.Description = PropVal.AsString();
                }
            }
            else if (PropName == "category")
            {
                if (!PropVal.IsString())
                {
                    OutDiagnostics.push_back(MakeUiMetaDiagnostic(
                        "core:diagnostic.schema.ui_metadata.invalid_type",
                        "'category' must be a string",
                        PackageId,
                        RelativeSource,
                        GetValueSpan(*ParsedDoc, PropPointer)));
                }
                else
                {
                    FieldMeta.Category = PropVal.AsString();
                }
            }
            else if (PropName == "order")
            {
                if (!PropVal.IsInteger())
                {
                    OutDiagnostics.push_back(MakeUiMetaDiagnostic(
                        "core:diagnostic.schema.ui_metadata.invalid_type",
                        "'order' must be an integer",
                        PackageId,
                        RelativeSource,
                        GetValueSpan(*ParsedDoc, PropPointer)));
                }
                else
                {
                    FieldMeta.Order = PropVal.AsInteger();
                }
            }
            else if (PropName == "widget_hint")
            {
                if (!PropVal.IsString())
                {
                    OutDiagnostics.push_back(MakeUiMetaDiagnostic(
                        "core:diagnostic.schema.ui_metadata.invalid_type",
                        "'widget_hint' must be a string",
                        PackageId,
                        RelativeSource,
                        GetValueSpan(*ParsedDoc, PropPointer)));
                }
                else
                {
                    FieldMeta.WidgetHint = PropVal.AsString();
                }
            }
        }

        Result.SetField(FieldName, std::move(FieldMeta));
    }

    if (OutDiagnostics.size() > InitialDiagCount)
    {
        return std::nullopt;
    }

    return Result;
}

} // namespace GV2ContentCore
