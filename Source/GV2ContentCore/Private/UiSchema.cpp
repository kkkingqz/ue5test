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
bool IsAllowedSchemaRefNamespace(const std::string_view SourceNamespace, const std::string_view TargetNamespace)
{
    if (SourceNamespace.empty() || TargetNamespace.empty())
    {
        return true;
    }
    if (SourceNamespace == TargetNamespace)
    {
        return true;
    }
    if (TargetNamespace == "core")
    {
        return SourceNamespace != "core";
    }
    if (SourceNamespace == "textsystem")
    {
        return TargetNamespace == "core";
    }
    if (SourceNamespace == "rh")
    {
        return TargetNamespace == "core" || TargetNamespace == "textsystem";
    }
    if (SourceNamespace == "core")
    {
        return false;
    }
    if (TargetNamespace == "core" || TargetNamespace == "textsystem" || TargetNamespace == "rh")
    {
        return true;
    }
    return false;
}
}

void FInMemoryUiSchemaResolver::RegisterUiSchema(
    std::string SchemaId,
    FValue RootSpec,
    std::string PackageId,
    std::string RelativeSource)
{
    FEntry Entry;
    Entry.RootSpec = std::move(RootSpec);
    Entry.PackageId = std::move(PackageId);
    Entry.RelativeSource = std::move(RelativeSource);
    Entries.insert_or_assign(std::move(SchemaId), std::move(Entry));
}

void FInMemoryUiSchemaResolver::RegisterUiSchemaDocument(
    std::string SchemaId,
    std::shared_ptr<const FParsedDocument> Document,
    std::string PackageId,
    std::string RelativeSource)
{
    FEntry Entry;
    Entry.Document = std::move(Document);
    Entry.PackageId = std::move(PackageId);
    Entry.RelativeSource = std::move(RelativeSource);
    Entries.insert_or_assign(std::move(SchemaId), std::move(Entry));
}

std::optional<FResolvedUiSchema> FInMemoryUiSchemaResolver::FindUiSchema(const std::string_view SchemaId) const
{
    const auto It = Entries.find(SchemaId);
    if (It == Entries.end())
    {
        return std::nullopt;
    }

    FResolvedUiSchema Resolved;
    Resolved.SchemaId = It->first;
    Resolved.PackageId = It->second.PackageId;
    Resolved.RelativeSource = It->second.RelativeSource;

    if (It->second.Document != nullptr)
    {
        Resolved.Document = It->second.Document.get();
        const FValue& DocRoot = It->second.Document->GetRootValue();
        if (DocRoot.IsObject())
        {
            if (const FValue* RootField = DocRoot.FindField("root"))
            {
                Resolved.RootSpec = RootField;
            }
            else
            {
                Resolved.RootSpec = &DocRoot;
            }
        }
        else
        {
            Resolved.RootSpec = &DocRoot;
        }
    }
    else if (It->second.RootSpec.has_value())
    {
        if (It->second.RootSpec->IsObject())
        {
            if (const FValue* RootField = It->second.RootSpec->FindField("root"))
            {
                Resolved.RootSpec = RootField;
            }
            else
            {
                Resolved.RootSpec = &*It->second.RootSpec;
            }
        }
        else
        {
            Resolved.RootSpec = &*It->second.RootSpec;
        }
    }
    return Resolved;
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
    std::vector<FDiagnostic>& OutDiagnostics,
    const IUiSchemaResolver* Resolver,
    std::vector<std::string>* ActiveResolutionChain)
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
                    Child, SchemaDocument, ChildSpecPointer, Context, OutDiagnostics, Resolver, ActiveResolutionChain);
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
        else Result->Items = CompileUiFieldSpec(*Items, SchemaDocument, ChildPointer(Pointer, "items"), Context, OutDiagnostics, Resolver, ActiveResolutionChain);
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
    else if (Kind == "schema_ref")
    {
        CheckClosedUiSpec(FieldSpec, { "schema_id" }, Kind, SchemaDocument, Pointer, Context, OutDiagnostics);
        const FValue* TargetSchemaIdValue = FieldSpec.FindField("schema_id");
        if (TargetSchemaIdValue == nullptr || !TargetSchemaIdValue->IsString())
        {
            OutDiagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.ui_schema.schema_ref.invalid_schema_id",
                "schema_ref requires a string schema_id",
                SchemaDocument, ChildPointer(Pointer, "schema_id"), Context));
            return nullptr;
        }

        const std::string& TargetSchemaId = TargetSchemaIdValue->AsString();
        FStableIdView TargetView;
        if (!FStableId::Parse(TargetSchemaId, TargetView) || TargetView.Kind != "schema")
        {
            OutDiagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.ui_schema.schema_ref.invalid_schema_id",
                "schema_ref schema_id must be a valid Stable ID of kind 'schema': '" + TargetSchemaId + "'",
                SchemaDocument, ChildPointer(Pointer, "schema_id"), Context));
            return nullptr;
        }

        std::string_view SourceNamespace;
        if (Context.SchemaId.has_value())
        {
            FStableIdView SourceView;
            if (FStableId::Parse(*Context.SchemaId, SourceView))
            {
                SourceNamespace = SourceView.Namespace;
            }
        }
        if (SourceNamespace.empty() && Context.PackageId.has_value())
        {
            SourceNamespace = *Context.PackageId;
        }

        if (!IsAllowedSchemaRefNamespace(SourceNamespace, TargetView.Namespace))
        {
            OutDiagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.ui_schema.schema_ref.forbidden_namespace",
                "schema_ref in namespace '" + std::string(SourceNamespace) + "' cannot reference schema '" + TargetSchemaId + "' in namespace '" + std::string(TargetView.Namespace) + "'",
                SchemaDocument, ChildPointer(Pointer, "schema_id"), Context));
            return nullptr;
        }

        if (Resolver == nullptr)
        {
            OutDiagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.ui_schema.schema_ref.unresolved_schema",
                "No UI schema resolver provided to resolve schema_ref target '" + TargetSchemaId + "'",
                SchemaDocument, ChildPointer(Pointer, "schema_id"), Context));
            return nullptr;
        }

        const auto Resolved = Resolver->FindUiSchema(TargetSchemaId);
        if (!Resolved.has_value() || Resolved->RootSpec == nullptr)
        {
            OutDiagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.ui_schema.schema_ref.unresolved_schema",
                "Unresolved schema_ref target '" + TargetSchemaId + "'",
                SchemaDocument, ChildPointer(Pointer, "schema_id"), Context));
            return nullptr;
        }

        std::vector<std::string> LocalChain;
        std::vector<std::string>& Chain = (ActiveResolutionChain != nullptr) ? *ActiveResolutionChain : LocalChain;

        if (Chain.empty() && Context.SchemaId.has_value())
        {
            Chain.push_back(*Context.SchemaId);
        }

        const auto ExistingIt = std::find(Chain.begin(), Chain.end(), TargetSchemaId);
        if (ExistingIt != Chain.end())
        {
            std::string CyclePath;
            for (auto It = ExistingIt; It != Chain.end(); ++It)
            {
                CyclePath += *It + " -> ";
            }
            CyclePath += TargetSchemaId;

            OutDiagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.ui_schema.schema_ref.cycle_detected",
                "Cycle detected in schema_ref resolution: " + CyclePath,
                SchemaDocument, ChildPointer(Pointer, "schema_id"), Context));
            return nullptr;
        }

        Chain.push_back(TargetSchemaId);

        FValidationDiagnosticContext TargetContext;
        TargetContext.SchemaId = TargetSchemaId;
        if (!Resolved->PackageId.empty()) TargetContext.PackageId = Resolved->PackageId;
        else if (Context.PackageId.has_value()) TargetContext.PackageId = Context.PackageId;
        if (!Resolved->RelativeSource.empty()) TargetContext.RelativeSource = Resolved->RelativeSource;

        const std::string TargetPointer = (Resolved->RootSpec != nullptr && Resolved->Document != nullptr && Resolved->RootSpec == Resolved->Document->GetRootValue().FindField("root"))
            ? "/root"
            : "";

        FCompiledUiFieldSpecPtr CompiledTarget = CompileUiFieldSpec(
            *Resolved->RootSpec,
            Resolved->Document,
            TargetPointer,
            TargetContext,
            OutDiagnostics,
            Resolver,
            &Chain);

        Chain.pop_back();

        if (CompiledTarget == nullptr || OutDiagnostics.size() != InitialCount)
        {
            return nullptr;
        }

        if (bNullable && !CompiledTarget->bNullable)
        {
            auto NullableCopy = std::make_shared<FCompiledUiFieldSpec>(*CompiledTarget);
            NullableCopy->bNullable = true;
            return NullableCopy;
        }

        return CompiledTarget;
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
