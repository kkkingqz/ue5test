#include "GV2ContentCore/FieldValidation.h"

#include "GV2ContentCore/StableId.h"

#include <algorithm>
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
        else
        {
            std::string ExistingPointer = JsonPointer;
            while (!ExistingPointer.empty())
            {
                const std::size_t Separator = ExistingPointer.rfind('/');
                ExistingPointer = Separator == std::string::npos
                    ? std::string()
                    : ExistingPointer.substr(0, Separator);
                if (const FParsedLocation* ParentLocation = Document->FindLocation(ExistingPointer))
                {
                    Diagnostic.Span = ParentLocation->ValueSpan;
                    break;
                }
            }
            if (!Diagnostic.Span.has_value())
            {
                if (const FParsedLocation* RootLocation = Document->FindLocation(""))
                {
                    Diagnostic.Span = RootLocation->ValueSpan;
                }
            }
        }
    }
    return Diagnostic;
}

bool IsCommonField(const std::string_view FieldName)
{
    return FieldName == "kind" || FieldName == "required" || FieldName == "nullable"
        || FieldName == "default" || FieldName == "description"
        || FieldName == "storage" || FieldName == "write_policy" || FieldName == "operations";
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

int CompareValues(const FValue& Left, const FValue& Right)
{
    if (Left.GetKind() != Right.GetKind())
    {
        return static_cast<int>(Left.GetKind()) < static_cast<int>(Right.GetKind()) ? -1 : 1;
    }
    switch (Left.GetKind())
    {
    case EValueKind::Null: return 0;
    case EValueKind::Boolean:
        return Left.AsBoolean() == Right.AsBoolean() ? 0 : (Left.AsBoolean() ? 1 : -1);
    case EValueKind::Integer:
        return Left.AsInteger() == Right.AsInteger() ? 0 : (Left.AsInteger() < Right.AsInteger() ? -1 : 1);
    case EValueKind::Number:
        return Left.AsNumber() == Right.AsNumber() ? 0 : (Left.AsNumber() < Right.AsNumber() ? -1 : 1);
    case EValueKind::String:
        return Left.AsString() == Right.AsString() ? 0 : (Left.AsString() < Right.AsString() ? -1 : 1);
    case EValueKind::Array:
    {
        const auto& LeftArray = Left.AsArray();
        const auto& RightArray = Right.AsArray();
        const std::size_t SharedSize = std::min(LeftArray.size(), RightArray.size());
        for (std::size_t Index = 0; Index < SharedSize; ++Index)
        {
            if (const int ItemResult = CompareValues(LeftArray[Index], RightArray[Index]); ItemResult != 0) return ItemResult;
        }
        return LeftArray.size() == RightArray.size() ? 0 : (LeftArray.size() < RightArray.size() ? -1 : 1);
    }
    case EValueKind::Object:
    {
        const auto& LeftObject = Left.AsObject();
        const auto& RightObject = Right.AsObject();
        std::vector<const FValue::FObjectField*> OrderedLeft;
        std::vector<const FValue::FObjectField*> OrderedRight;
        OrderedLeft.reserve(LeftObject.size());
        OrderedRight.reserve(RightObject.size());
        for (const FValue::FObjectField& Field : LeftObject) OrderedLeft.push_back(&Field);
        for (const FValue::FObjectField& Field : RightObject) OrderedRight.push_back(&Field);
        const auto ByName = [](const FValue::FObjectField* LeftField, const FValue::FObjectField* RightField)
        {
            return LeftField->first < RightField->first;
        };
        std::sort(OrderedLeft.begin(), OrderedLeft.end(), ByName);
        std::sort(OrderedRight.begin(), OrderedRight.end(), ByName);
        const std::size_t SharedSize = std::min(OrderedLeft.size(), OrderedRight.size());
        for (std::size_t Index = 0; Index < SharedSize; ++Index)
        {
            if (OrderedLeft[Index]->first != OrderedRight[Index]->first)
                return OrderedLeft[Index]->first < OrderedRight[Index]->first ? -1 : 1;
            if (const int ValueResult = CompareValues(OrderedLeft[Index]->second, OrderedRight[Index]->second); ValueResult != 0) return ValueResult;
        }
        return OrderedLeft.size() == OrderedRight.size() ? 0 : (OrderedLeft.size() < OrderedRight.size() ? -1 : 1);
    }
    }
    return 0;
}

struct FValuePointerLess final
{
    bool operator()(const FValue* Left, const FValue* Right) const
    {
        return CompareValues(*Left, *Right) < 0;
    }
};

bool ReadCommonFields(
    const FValue& Source,
    const bool bObjectField,
    bool& OutNullable,
    bool& OutRequired,
    const FParsedDocument* Document,
    const std::string& Pointer,
    const FValidationDiagnosticContext& Context,
    std::vector<FDiagnostic>& Diagnostics)
{
    const std::size_t InitialCount = Diagnostics.size();
    if (const FValue* Nullable = Source.FindField("nullable"))
    {
        if (!Nullable->IsBoolean()) Diagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.field_spec.invalid_constraint", "nullable must be boolean",
            Document, ChildPointer(Pointer, "nullable"), Context));
        else OutNullable = Nullable->AsBoolean();
    }
    if (const FValue* Required = Source.FindField("required"))
    {
        if (!bObjectField)
        {
            Diagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.field_spec.invalid_constraint",
                "required is allowed only on an object field",
                Document, ChildPointer(Pointer, "required"), Context));
        }
        else if (!Required->IsBoolean())
        {
            Diagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.field_spec.invalid_constraint", "required must be boolean",
                Document, ChildPointer(Pointer, "required"), Context));
        }
        else OutRequired = Required->AsBoolean();
    }
    if (const FValue* Description = Source.FindField("description"); Description != nullptr && !Description->IsString())
    {
        Diagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.field_spec.invalid_constraint", "description must be string",
            Document, ChildPointer(Pointer, "description"), Context));
    }
    return Diagnostics.size() == InitialCount;
}

bool ReadPropertyAttributes(
    const FValue& Source,
    EStoragePolicy& OutStorage,
    EWritePolicy& OutWritePolicy,
    std::vector<std::string>& OutOperations,
    const FParsedDocument* Document,
    const std::string& Pointer,
    const FValidationDiagnosticContext& Context,
    std::vector<FDiagnostic>& Diagnostics)
{
    const std::size_t InitialCount = Diagnostics.size();
    OutStorage = EStoragePolicy::Definition;
    bool bStorageExplicit = false;

    if (const FValue* StorageVal = Source.FindField("storage"))
    {
        bStorageExplicit = true;
        if (!StorageVal->IsString())
        {
            Diagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.field_spec.invalid_storage", "storage must be a string",
                Document, ChildPointer(Pointer, "storage"), Context));
        }
        else
        {
            const std::string& S = StorageVal->AsString();
            if (S == "definition" || S == "Definition")
            {
                OutStorage = EStoragePolicy::Definition;
            }
            else if (S == "runtime_state" || S == "RuntimeState")
            {
                OutStorage = EStoragePolicy::RuntimeState;
            }
            else
            {
                Diagnostics.push_back(MakeDiagnostic(
                    "core:diagnostic.schema.field_spec.invalid_storage", "unknown storage policy '" + S + "', expected 'definition' or 'runtime_state'",
                    Document, ChildPointer(Pointer, "storage"), Context));
            }
        }
    }

    // Default write policy depends on storage
    OutWritePolicy = (OutStorage == EStoragePolicy::Definition) ? EWritePolicy::ReadOnly : EWritePolicy::Plain;

    if (const FValue* WritePolicyVal = Source.FindField("write_policy"))
    {
        if (!WritePolicyVal->IsString())
        {
            Diagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.field_spec.invalid_write_policy", "write_policy must be a string",
                Document, ChildPointer(Pointer, "write_policy"), Context));
        }
        else
        {
            const std::string& WP = WritePolicyVal->AsString();
            if (WP == "read_only" || WP == "ReadOnly")
            {
                OutWritePolicy = EWritePolicy::ReadOnly;
            }
            else if (WP == "plain" || WP == "Plain")
            {
                OutWritePolicy = EWritePolicy::Plain;
            }
            else if (WP == "managed" || WP == "Managed")
            {
                OutWritePolicy = EWritePolicy::Managed;
            }
            else
            {
                Diagnostics.push_back(MakeDiagnostic(
                    "core:diagnostic.schema.field_spec.invalid_write_policy", "unknown write policy '" + WP + "', expected 'read_only', 'plain', or 'managed'",
                    Document, ChildPointer(Pointer, "write_policy"), Context));
            }
        }
    }

    if (const FValue* OpsVal = Source.FindField("operations"))
    {
        if (!OpsVal->IsArray())
        {
            Diagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.field_spec.invalid_operations", "operations must be an array of strings",
                Document, ChildPointer(Pointer, "operations"), Context));
        }
        else
        {
            for (std::size_t i = 0; i < OpsVal->AsArray().size(); ++i)
            {
                const auto& Item = OpsVal->AsArray()[i];
                if (!Item.IsString() || Item.AsString().empty() || !IsCanonicalFieldName(Item.AsString()))
                {
                    Diagnostics.push_back(MakeDiagnostic(
                        "core:diagnostic.schema.field_spec.invalid_operations", "operation name must be a canonical snake_case string",
                        Document, ChildPointer(ChildPointer(Pointer, "operations"), std::to_string(i)), Context));
                }
                else
                {
                    OutOperations.push_back(Item.AsString());
                }
            }
        }
    }

    if (OutWritePolicy == EWritePolicy::Managed && OutOperations.empty())
    {
        Diagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.field_spec.missing_operations", "managed write_policy requires at least one operation in operations array",
            Document, ChildPointer(Pointer, "operations"), Context));
    }

    return Diagnostics.size() == InitialCount;
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
            "core:diagnostic.schema.field_spec.invalid_constraint",
            std::string(Name) + " must be a non-negative int64",
            Document, ChildPointer(Pointer, Name), Context));
        return std::nullopt;
    }
    return static_cast<std::size_t>(Value->AsInteger());
}

bool CheckClosedSpec(
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
        if (!IsCommonField(Name) && !Specific.contains(Name))
        {
            Diagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.field_spec.unknown_field",
                "Unknown field for FieldSpec kind " + Kind + ": " + Name,
                Document, ChildPointer(Pointer, Name), Context));
        }
    }
    return Diagnostics.size() == InitialCount;
}

bool MaterializeNode(
    const FValue& Value,
    const FCompiledFieldSpec& Spec,
    FValue& OutMaterializedValue,
    const FParsedDocument* Document,
    const std::string& Pointer,
    const FValidationDiagnosticContext& Context,
    std::vector<FDiagnostic>& Diagnostics);

void CompileExplicitDefault(
    const FValue& Source,
    const bool bRequired,
    const std::shared_ptr<FCompiledFieldSpec>& Result,
    const FParsedDocument* Document,
    const std::string& Pointer,
    const FValidationDiagnosticContext& Context,
    std::vector<FDiagnostic>& Diagnostics)
{
    const FValue* Default = Source.FindField("default");
    if (Default == nullptr) return;
    if (bRequired)
    {
        Diagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.field_spec.conflicting_constraint",
            "required object field cannot declare a default",
            Document, ChildPointer(Pointer, "default"), Context));
        return;
    }

    FValue MaterializedDefault;
    if (MaterializeNode(
            *Default,
            *Result,
            MaterializedDefault,
            Document,
            ChildPointer(Pointer, "default"),
            Context,
            Diagnostics))
    {
        Result->DefaultValue = std::move(MaterializedDefault);
    }
}

FCompiledFieldSpecPtr CompileFieldSpecNode(
    const FValue& Source,
    const FParsedDocument* Document,
    const std::string& Pointer,
    const FValidationDiagnosticContext& Context,
    std::vector<FDiagnostic>& Diagnostics,
    const bool bObjectField,
    bool* OutRequired)
{
    const std::size_t InitialCount = Diagnostics.size();
    if (!Source.IsObject())
    {
        Diagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.field_spec.invalid_shape", "FieldSpec must be an object",
            Document, Pointer, Context));
        return nullptr;
    }
    const FValue* KindValue = Source.FindField("kind");
    if (KindValue == nullptr || !KindValue->IsString())
    {
        Diagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.field_spec.invalid_kind", "FieldSpec requires a string kind",
            Document, ChildPointer(Pointer, "kind"), Context));
        return nullptr;
    }

    const std::string& Kind = KindValue->AsString();
    bool bNullable = false;
    bool bRequired = false;
    auto Result = std::make_shared<FCompiledFieldSpec>();

    if (IsScalarFieldKind(Kind))
    {
        if (const FValue* Required = Source.FindField("required"); Required != nullptr && Required->IsBoolean())
        {
            if (!bObjectField)
            {
                Diagnostics.push_back(MakeDiagnostic(
                    "core:diagnostic.schema.field_spec.invalid_constraint",
                    "required is allowed only on an object field",
                    Document, ChildPointer(Pointer, "required"), Context));
            }
            else bRequired = Required->AsBoolean();
        }
        const auto Scalar = CompileScalarFieldSpec(Source, Document, Pointer, Context, Diagnostics);
        if (Scalar.has_value())
        {
            Result->Kind = EFieldKind::Scalar;
            Result->Scalar = *Scalar;
            Result->bNullable = Scalar->bNullable;
            ReadPropertyAttributes(Source, Result->Storage, Result->WritePolicy, Result->Operations, Document, Pointer, Context, Diagnostics);
            CompileExplicitDefault(
                Source, bRequired, Result, Document, Pointer, Context, Diagnostics);
        }
        if (OutRequired != nullptr) *OutRequired = bRequired;
        return Diagnostics.size() == InitialCount ? Result : nullptr;
    }
    ReadCommonFields(Source, bObjectField, bNullable, bRequired, Document, Pointer, Context, Diagnostics);
    ReadPropertyAttributes(Source, Result->Storage, Result->WritePolicy, Result->Operations, Document, Pointer, Context, Diagnostics);
    Result->bNullable = bNullable;
    if (OutRequired != nullptr) *OutRequired = bRequired;

    if (Kind == "array")
    {
        Result->Kind = EFieldKind::Array;
        CheckClosedSpec(Source, { "items", "min_items", "max_items", "unique" }, Kind, Document, Pointer, Context, Diagnostics);
        const FValue* Items = Source.FindField("items");
        if (Items == nullptr)
        {
            Diagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.field_spec.missing_constraint", "array requires items",
                Document, ChildPointer(Pointer, "items"), Context));
        }
        else Result->Items = CompileFieldSpecNode(*Items, Document, ChildPointer(Pointer, "items"), Context, Diagnostics, false, nullptr);
        Result->MinimumSize = ReadSize(Source, "min_items", Document, Pointer, Context, Diagnostics);
        Result->MaximumSize = ReadSize(Source, "max_items", Document, Pointer, Context, Diagnostics);
        if (const FValue* Unique = Source.FindField("unique"))
        {
            if (!Unique->IsBoolean()) Diagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.field_spec.invalid_constraint", "unique must be boolean",
                Document, ChildPointer(Pointer, "unique"), Context));
            else Result->bUnique = Unique->AsBoolean();
        }
    }
    else if (Kind == "map")
    {
        Result->Kind = EFieldKind::Map;
        CheckClosedSpec(Source, { "keys", "values", "min_entries", "max_entries" }, Kind, Document, Pointer, Context, Diagnostics);
        const FValue* Keys = Source.FindField("keys");
        const FValue* Values = Source.FindField("values");
        if (Keys == nullptr) Diagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.field_spec.missing_constraint", "map requires keys",
            Document, ChildPointer(Pointer, "keys"), Context));
        else Result->MapKeys = CompileFieldSpecNode(*Keys, Document, ChildPointer(Pointer, "keys"), Context, Diagnostics, false, nullptr);
        if (Values == nullptr) Diagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.field_spec.missing_constraint", "map requires values",
            Document, ChildPointer(Pointer, "values"), Context));
        else Result->MapValues = CompileFieldSpecNode(*Values, Document, ChildPointer(Pointer, "values"), Context, Diagnostics, false, nullptr);
        Result->MinimumSize = ReadSize(Source, "min_entries", Document, Pointer, Context, Diagnostics);
        Result->MaximumSize = ReadSize(Source, "max_entries", Document, Pointer, Context, Diagnostics);
        if (Result->MapKeys != nullptr && (Result->MapKeys->Kind != EFieldKind::Scalar
            || (Result->MapKeys->Scalar->Kind != EScalarFieldKind::String
                && Result->MapKeys->Scalar->Kind != EScalarFieldKind::Enum)))
        {
            Diagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.field_spec.invalid_map_key",
                "map keys must use string or string-only enum FieldSpec",
                Document, ChildPointer(Pointer, "keys"), Context));
        }
        if (Result->MapKeys != nullptr && Result->MapKeys->Kind == EFieldKind::Scalar
            && Result->MapKeys->Scalar->Kind == EScalarFieldKind::Enum
            && std::any_of(Result->MapKeys->Scalar->EnumValues.begin(), Result->MapKeys->Scalar->EnumValues.end(),
                [](const FValue& Value) { return !Value.IsString(); }))
        {
            Diagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.field_spec.invalid_map_key",
                "map enum keys must contain only strings",
                Document, ChildPointer(Pointer, "keys"), Context));
        }
    }
    else if (Kind == "object")
    {
        Result->Kind = EFieldKind::Object;
        CheckClosedSpec(Source, { "fields" }, Kind, Document, Pointer, Context, Diagnostics);
        const FValue* Fields = Source.FindField("fields");
        if (Fields == nullptr || !Fields->IsObject())
        {
            Diagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.field_spec.invalid_fields", "object requires a fields object",
                Document, ChildPointer(Pointer, "fields"), Context));
        }
        else
        {
            for (const auto& [Name, Child] : Fields->AsObject())
            {
                const std::string ChildSpecPointer = ChildPointer(ChildPointer(Pointer, "fields"), Name);
                if (!IsCanonicalFieldName(Name))
                {
                    Diagnostics.push_back(MakeDiagnostic(
                        "core:diagnostic.schema.field_spec.invalid_field_name",
                        "object field name must be canonical snake_case",
                        Document, ChildSpecPointer, Context, true));
                }
                bool bChildRequired = false;
                FCompiledFieldSpecPtr ChildSpec = CompileFieldSpecNode(
                    Child, Document, ChildSpecPointer, Context, Diagnostics, true, &bChildRequired);
                if (ChildSpec != nullptr) Result->Fields.push_back({ Name, bChildRequired, std::move(ChildSpec) });
            }
        }
    }
    else if (Kind == "union")
    {
        Result->Kind = EFieldKind::Union;
        CheckClosedSpec(Source, { "discriminator", "variants" }, Kind, Document, Pointer, Context, Diagnostics);
        const FValue* Discriminator = Source.FindField("discriminator");
        if (Discriminator == nullptr || !Discriminator->IsString() || !IsCanonicalFieldName(Discriminator->IsString() ? Discriminator->AsString() : ""))
        {
            Diagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.field_spec.invalid_discriminator",
                "union discriminator must be a canonical snake_case field name",
                Document, ChildPointer(Pointer, "discriminator"), Context));
        }
        else Result->Discriminator = Discriminator->AsString();
        const FValue* Variants = Source.FindField("variants");
        if (Variants == nullptr || !Variants->IsObject() || Variants->AsObject().empty())
        {
            Diagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.field_spec.invalid_variants",
                "union variants must be a non-empty object",
                Document, ChildPointer(Pointer, "variants"), Context));
        }
        else
        {
            for (const auto& [Tag, Variant] : Variants->AsObject())
            {
                const std::string VariantPointer = ChildPointer(ChildPointer(Pointer, "variants"), Tag);
                FCompiledFieldSpecPtr VariantSpec = CompileFieldSpecNode(
                    Variant, Document, VariantPointer, Context, Diagnostics, false, nullptr);
                if (VariantSpec != nullptr && VariantSpec->Kind != EFieldKind::Object)
                {
                    Diagnostics.push_back(MakeDiagnostic(
                        "core:diagnostic.schema.field_spec.invalid_variant",
                        "union variant must be an object FieldSpec",
                        Document, VariantPointer, Context));
                }
                else if (VariantSpec != nullptr) Result->Variants.push_back({ Tag, std::move(VariantSpec) });
            }
        }
    }
    else if (Kind == "ref" || Kind == "ref_definition")
    {
        Result->Kind = EFieldKind::Reference;
        Result->ReferenceKind = EReferenceKind::Definition;
        CheckClosedSpec(Source, { "target_kind" }, Kind, Document, Pointer, Context, Diagnostics);
        const FValue* TargetKind = Source.FindField("target_kind");
        if (TargetKind == nullptr || !TargetKind->IsString()
            || !FStableId::IsValidSegment(TargetKind->IsString() ? TargetKind->AsString() : ""))
        {
            Diagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.field_spec.invalid_target_kind",
                Kind + " requires target_kind as a canonical Stable ID segment",
                Document, ChildPointer(Pointer, "target_kind"), Context));
        }
        else Result->ExpectedStableIdKind = TargetKind->AsString();
    }
    else if (Kind == "ref_instance")
    {
        Result->Kind = EFieldKind::Reference;
        Result->ReferenceKind = EReferenceKind::Instance;
        if (Source.FindField("storage") == nullptr)
        {
            Result->Storage = EStoragePolicy::RuntimeState;
            if (Source.FindField("write_policy") == nullptr)
            {
                Result->WritePolicy = EWritePolicy::Plain;
            }
        }
        CheckClosedSpec(Source, { "target_kind" }, Kind, Document, Pointer, Context, Diagnostics);
        const FValue* TargetKind = Source.FindField("target_kind");
        if (TargetKind == nullptr || !TargetKind->IsString()
            || !FStableId::IsValidSegment(TargetKind->IsString() ? TargetKind->AsString() : ""))
        {
            Diagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.field_spec.invalid_target_kind",
                "ref_instance requires target_kind as a canonical Stable ID segment",
                Document, ChildPointer(Pointer, "target_kind"), Context));
        }
        else Result->ExpectedStableIdKind = TargetKind->AsString();
    }
    else if (Kind == "text_id")
    {
        Result->Kind = EFieldKind::TextId;
        Result->ExpectedStableIdKind = "text";
        CheckClosedSpec(Source, {}, Kind, Document, Pointer, Context, Diagnostics);
    }
    else if (Kind == "resource_ref")
    {
        Result->Kind = EFieldKind::ResourceReference;
        Result->ExpectedStableIdKind = "resource";
        CheckClosedSpec(
            Source, { "resource_class", "bootstrap_required" },
            Kind, Document, Pointer, Context, Diagnostics);
        const FValue* ResourceClass = Source.FindField("resource_class");
        if (ResourceClass == nullptr || !ResourceClass->IsString()
            || !FStableId::IsValidSegment(ResourceClass->IsString() ? ResourceClass->AsString() : ""))
        {
            Diagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.field_spec.invalid_resource_class",
                "resource_ref requires resource_class as a canonical segment",
                Document, ChildPointer(Pointer, "resource_class"), Context));
        }
        else Result->ResourceClass = ResourceClass->AsString();
        if (const FValue* BootstrapRequired = Source.FindField("bootstrap_required"))
        {
            if (!BootstrapRequired->IsBoolean())
            {
                Diagnostics.push_back(MakeDiagnostic(
                    "core:diagnostic.schema.field_spec.invalid_bootstrap_required",
                    "bootstrap_required must be boolean",
                    Document, ChildPointer(Pointer, "bootstrap_required"), Context));
            }
            else Result->bBootstrapRequired = BootstrapRequired->AsBoolean();
        }
    }
    else
    {
        Diagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.field_spec.invalid_kind", "Unknown FieldSpec kind",
            Document, ChildPointer(Pointer, "kind"), Context));
    }

    if ((Result->MinimumSize.has_value() && Result->MaximumSize.has_value()
            && *Result->MinimumSize > *Result->MaximumSize))
    {
        Diagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.field_spec.invalid_constraint_range",
            "minimum container size must not exceed maximum size",
            Document, Pointer, Context));
    }
    if (Diagnostics.size() == InitialCount)
    {
        CompileExplicitDefault(
            Source, bRequired, Result, Document, Pointer, Context, Diagnostics);
    }
    return Diagnostics.size() == InitialCount ? Result : nullptr;
}

bool MaterializeNode(
    const FValue& Value,
    const FCompiledFieldSpec& Spec,
    FValue& OutMaterializedValue,
    const FParsedDocument* Document,
    const std::string& Pointer,
    const FValidationDiagnosticContext& Context,
    std::vector<FDiagnostic>& Diagnostics)
{
    if (Value.IsNull())
    {
        if (Spec.bNullable)
        {
            OutMaterializedValue = Value;
            return true;
        }
        Diagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.value.null_not_allowed",
            "Explicit null is not allowed by this FieldSpec", Document, Pointer, Context));
        return false;
    }
    if (Spec.Kind == EFieldKind::Scalar)
    {
        if (!ValidateScalarValue(Value, *Spec.Scalar, Document, Pointer, Context, Diagnostics)) return false;
        OutMaterializedValue = Value;
        return true;
    }
    if (Spec.Kind == EFieldKind::Reference
        || Spec.Kind == EFieldKind::TextId
        || Spec.Kind == EFieldKind::ResourceReference)
    {
        if (!Value.IsString())
        {
            Diagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.value.type_mismatch",
                "Stable ID field requires a string; coercion is prohibited",
                Document, Pointer, Context));
            return false;
        }
        FStableIdView ParsedId;
        EStableIdError StableIdError = EStableIdError::None;
        if (!FStableId::Parse(Value.AsString(), ParsedId, &StableIdError))
        {
            Diagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.value.invalid_stable_id",
                "Value is not a canonical Stable ID",
                Document, Pointer, Context));
            return false;
        }
        if (ParsedId.Kind != Spec.ExpectedStableIdKind)
        {
            Diagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.value.stable_id_wrong_kind",
                "Stable ID kind must be " + Spec.ExpectedStableIdKind,
                Document, Pointer, Context));
            return false;
        }
        OutMaterializedValue = Value;
        return true;
    }

    const bool bRequiresArray = Spec.Kind == EFieldKind::Array;
    const bool bRequiresObject = Spec.Kind == EFieldKind::Map || Spec.Kind == EFieldKind::Object || Spec.Kind == EFieldKind::Union;
    if ((bRequiresArray && !Value.IsArray()) || (bRequiresObject && !Value.IsObject()))
    {
        Diagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.schema.value.type_mismatch",
            "Value kind does not match container FieldSpec; coercion is prohibited",
            Document, Pointer, Context));
        return false;
    }

    const std::size_t InitialCount = Diagnostics.size();
    if (Spec.Kind == EFieldKind::Array)
    {
        const auto& Items = Value.AsArray();
        FValue::FArray MaterializedItems;
        MaterializedItems.reserve(Items.size());
        if ((Spec.MinimumSize.has_value() && Items.size() < *Spec.MinimumSize)
            || (Spec.MaximumSize.has_value() && Items.size() > *Spec.MaximumSize))
        {
            Diagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.value.constraint_failed",
                "array size violates min_items/max_items", Document, Pointer, Context));
        }
        if (Spec.bUnique)
        {
            std::set<const FValue*, FValuePointerLess> SeenItems;
            for (std::size_t Index = 0; Index < Items.size(); ++Index)
            {
                if (!SeenItems.insert(&Items[Index]).second)
                {
                    Diagnostics.push_back(MakeDiagnostic(
                        "core:diagnostic.schema.value.duplicate_array_item",
                        "array items must be unique", Document,
                        ChildPointer(Pointer, std::to_string(Index)), Context));
                }
            }
        }
        for (std::size_t Index = 0; Index < Items.size(); ++Index)
        {
            FValue MaterializedItem;
            if (MaterializeNode(
                    Items[Index], *Spec.Items, MaterializedItem, Document,
                    ChildPointer(Pointer, std::to_string(Index)), Context, Diagnostics))
            {
                MaterializedItems.push_back(std::move(MaterializedItem));
            }
        }
        if (Diagnostics.size() == InitialCount)
            OutMaterializedValue = FValue::MakeArray(std::move(MaterializedItems));
    }
    else if (Spec.Kind == EFieldKind::Map)
    {
        const auto& Entries = Value.AsObject();
        FValue::FObject MaterializedEntries;
        MaterializedEntries.reserve(Entries.size());
        if ((Spec.MinimumSize.has_value() && Entries.size() < *Spec.MinimumSize)
            || (Spec.MaximumSize.has_value() && Entries.size() > *Spec.MaximumSize))
        {
            Diagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.value.constraint_failed",
                "map size violates min_entries/max_entries", Document, Pointer, Context));
        }
        for (const auto& [Name, EntryValue] : Entries)
        {
            const std::string EntryPointer = ChildPointer(Pointer, Name);
            const std::size_t BeforeKey = Diagnostics.size();
            FValue MaterializedKey;
            MaterializeNode(
                FValue(Name), *Spec.MapKeys, MaterializedKey,
                Document, EntryPointer, Context, Diagnostics);
            for (std::size_t Index = BeforeKey; Index < Diagnostics.size(); ++Index)
            {
                if (Document != nullptr)
                {
                    if (const FParsedLocation* Location = Document->FindLocation(EntryPointer);
                        Location != nullptr && Location->KeySpan.has_value()) Diagnostics[Index].Span = *Location->KeySpan;
                }
            }
            FValue MaterializedEntryValue;
            if (MaterializeNode(
                    EntryValue, *Spec.MapValues, MaterializedEntryValue,
                    Document, EntryPointer, Context, Diagnostics))
            {
                MaterializedEntries.emplace_back(Name, std::move(MaterializedEntryValue));
            }
        }
        if (Diagnostics.size() == InitialCount)
            OutMaterializedValue = FValue::MakeObject(std::move(MaterializedEntries));
    }
    else if (Spec.Kind == EFieldKind::Object)
    {
        FValue::FObject MaterializedFields;
        MaterializedFields.reserve(Value.AsObject().size() + Spec.Fields.size());
        for (const auto& [Name, FieldValue] : Value.AsObject())
        {
            const auto Found = std::find_if(Spec.Fields.begin(), Spec.Fields.end(),
                [&Name](const FCompiledObjectField& Field) { return Field.Name == Name; });
            const std::string FieldPointer = ChildPointer(Pointer, Name);
            if (Found == Spec.Fields.end())
            {
                Diagnostics.push_back(MakeDiagnostic(
                    "core:diagnostic.schema.value.unknown_field",
                    "Field is not declared by the closed object schema",
                    Document, FieldPointer, Context, true));
            }
            else
            {
                FValue MaterializedFieldValue;
                if (MaterializeNode(
                        FieldValue, *Found->Spec, MaterializedFieldValue,
                        Document, FieldPointer, Context, Diagnostics))
                {
                    MaterializedFields.emplace_back(Name, std::move(MaterializedFieldValue));
                }
            }
        }
        for (const FCompiledObjectField& Field : Spec.Fields)
        {
            if (Value.FindField(Field.Name) != nullptr) continue;
            const std::string FieldPointer = ChildPointer(Pointer, Field.Name);
            if (Field.bRequired)
            {
                Diagnostics.push_back(MakeDiagnostic(
                    "core:diagnostic.schema.value.missing_required_field",
                    "Required object field is absent",
                    Document, FieldPointer, Context));
            }
            else if (Field.Spec->DefaultValue.has_value())
            {
                MaterializedFields.emplace_back(Field.Name, *Field.Spec->DefaultValue);
            }
        }
        if (Diagnostics.size() == InitialCount)
            OutMaterializedValue = FValue::MakeObject(std::move(MaterializedFields));
    }
    else
    {
        const FValue* Tag = Value.FindField(Spec.Discriminator);
        if (Tag == nullptr || !Tag->IsString())
        {
            Diagnostics.push_back(MakeDiagnostic(
                "core:diagnostic.schema.value.invalid_union_discriminator",
                "union requires a present string discriminator",
                Document, ChildPointer(Pointer, Spec.Discriminator), Context));
        }
        else
        {
            const auto Variant = std::find_if(Spec.Variants.begin(), Spec.Variants.end(),
                [&Tag](const FCompiledUnionVariant& Candidate)
                { return Candidate.DiscriminatorValue == Tag->AsString(); });
            if (Variant == Spec.Variants.end())
            {
                Diagnostics.push_back(MakeDiagnostic(
                    "core:diagnostic.schema.value.invalid_union_variant",
                    "union discriminator does not name a declared variant",
                    Document, ChildPointer(Pointer, Spec.Discriminator), Context));
            }
            else
            {
                FValue MaterializedVariant;
                if (MaterializeNode(
                        Value, *Variant->Spec, MaterializedVariant,
                        Document, Pointer, Context, Diagnostics))
                {
                    OutMaterializedValue = std::move(MaterializedVariant);
                }
            }
        }
    }
    return Diagnostics.size() == InitialCount;
}
}

FCompiledFieldSpecPtr CompileFieldSpec(
    const FValue& FieldSpec,
    const FParsedDocument* SchemaDocument,
    std::string SchemaJsonPointer,
    const FValidationDiagnosticContext& Context,
    std::vector<FDiagnostic>& OutDiagnostics)
{
    return CompileFieldSpecNode(
        FieldSpec, SchemaDocument, SchemaJsonPointer, Context, OutDiagnostics, false, nullptr);
}

bool ValidateFieldValue(
    const FValue& Value,
    const FCompiledFieldSpec& FieldSpec,
    FValue& OutMaterializedValue,
    const FParsedDocument* ValueDocument,
    std::string ValueJsonPointer,
    const FValidationDiagnosticContext& Context,
    std::vector<FDiagnostic>& OutDiagnostics)
{
    FValue MaterializedValue;
    if (!MaterializeNode(
            Value,
            FieldSpec,
            MaterializedValue,
            ValueDocument,
            ValueJsonPointer,
            Context,
            OutDiagnostics))
    {
        return false;
    }
    OutMaterializedValue = std::move(MaterializedValue);
    return true;
}
}
