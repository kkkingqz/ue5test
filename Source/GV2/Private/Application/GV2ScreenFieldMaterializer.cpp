#include "Application/GV2ScreenFieldMaterializer.h"

#include "Application/GV2SessionContentSnapshot.h"
#include "GV2ContentCore/UiSchema.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"
#include "GV2RuntimeCore/GV2StableId.h"
#include "UI/GV2PreparedUiValue.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2UiSchemaCache.h"

#include <algorithm>
#include <string>
#include <vector>

namespace
{
// PAH-04A: session-scoped, not process-lifetime -- rebuilt by
// RebuildSchemaCacheForSession (called once per StartSession, before Ready) and
// torn down by ReleaseSchemaCacheForSession (EndSession, and a failed
// StartSession). No GV2PackageClosure::DiscoverFromGameData() call here anymore:
// that function always re-derives the canonical mods.lock.json5 default and
// ignores whatever roots this session actually started with (Editor's
// EditorPackageRoots override, the test-only sample override), which is exactly
// PKG-R1 -- a second, independent discovery that can silently diverge from the
// session's own repository/Lua package set.
TOptional<FGV2UiSchemaCache> GSessionSchemaCache;

// PAH-08: phase=authority -- the session schema cache accessor itself.
FGV2UiSchemaCache& GetSchemaCache()
{
    check(GSessionSchemaCache.IsSet());
    return *GSessionSchemaCache;
}
} // anonymous namespace

namespace
{
bool ToControlValue(
    const std::string& Name,
    const GV2ContentCore::FValue& Value,
    FGV2UiControlValue& OutValue)
{
    OutValue = {};
    OutValue.Name = FName(UTF8_TO_TCHAR(Name.c_str()));
    if (OutValue.Name.IsNone()) return false;
    if (Value.IsBoolean())
    {
        OutValue.Type = EGV2UiControlValueType::Boolean;
        OutValue.BooleanValue = Value.AsBoolean();
        return true;
    }
    if (Value.IsInteger())
    {
        OutValue.Type = EGV2UiControlValueType::Integer;
        OutValue.IntegerValue = Value.AsInteger();
        return true;
    }
    if (Value.IsNumber())
    {
        const double Number = Value.AsNumber();
        if (!FMath::IsFinite(Number)) return false;
        OutValue.Type = EGV2UiControlValueType::Number;
        OutValue.NumberValue = Number;
        return true;
    }
    if (Value.IsString())
    {
        OutValue.Type = EGV2UiControlValueType::String;
        OutValue.StringValue = UTF8_TO_TCHAR(Value.AsString().c_str());
        return true;
    }
    return false;
}

bool ResolveText(const GV2ContentCore::FValue& Value, FGV2TextViewModel& OutText, const FGV2PresentationPrepareContext* PrepareContext)
{
    if (!Value.IsObject()) return false;
    const GV2ContentCore::FValue* TextIdVal = Value.FindField("text_id");
    if (TextIdVal == nullptr || !TextIdVal->IsString()) return false;
    const std::string& TextId = TextIdVal->AsString();

    std::string Style;
    if (const GV2ContentCore::FValue* StyleVal = Value.FindField("style"))
    {
        if (StyleVal->IsString()) Style = StyleVal->AsString();
    }

    TArray<FGV2UiControlValue> Args;
    if (const GV2ContentCore::FValue* ArgsVal = Value.FindField("args"))
    {
        if (ArgsVal->IsObject())
        {
            const auto& ArgsObj = ArgsVal->AsObject();
            Args.Reserve(static_cast<int32>(ArgsObj.size()));
            for (const auto& [Name, Argument] : ArgsObj)
            {
                FGV2UiControlValue& Converted = Args.AddDefaulted_GetRef();
                if (!ToControlValue(Name, Argument, Converted)) return false;
            }
        }
    }

    FString Error;
    const bool bResolved = UGV2TextPipeline::Resolve(
        UTF8_TO_TCHAR(TextId.c_str()),
        Args,
        FName(UTF8_TO_TCHAR(Style.c_str())),
        OutText,
        Error,
        PrepareContext);
    if (!bResolved)
    {
        UE_LOG(LogTemp, Error, TEXT("ResolveText: '%s' failed: %s"), UTF8_TO_TCHAR(TextId.c_str()), *Error);
    }
    return bResolved;
}

bool ReadBinding(
    const GV2ContentCore::FValue& Value,
    const TArray<FString>& NodePath,
    const FString& ElementId,
    FGV2UiBindingDefinition& OutDefinition)
{
    if (!Value.IsObject()) return false;
    const GV2ContentCore::FValue* CmdIdVal = Value.FindField("command_id");
    if (CmdIdVal == nullptr || !CmdIdVal->IsString()) return false;
    const std::string& CommandId = CmdIdVal->AsString();
    if (!GV2RuntimeCore::FStableId::IsOfKind(CommandId, "command")) return false;

    OutDefinition = {};
    OutDefinition.NodeKeyPath = NodePath;
    OutDefinition.ElementId = ElementId;
    OutDefinition.CommandId = UTF8_TO_TCHAR(CommandId.c_str());

    if (const GV2ContentCore::FValue* ArgsVal = Value.FindField("args"))
    {
        if (ArgsVal->IsObject())
        {
            for (const auto& [Name, Argument] : ArgsVal->AsObject())
            {
                FGV2UiControlValue& Converted = OutDefinition.BoundArgs.AddDefaulted_GetRef();
                if (!ToControlValue(Name, Argument, Converted)) return false;
            }
        }
    }
    return true;
}

FString FieldIdOf(const GV2RuntimeCore::FScreenField& Field)
{
    return UTF8_TO_TCHAR(Field.FieldId.c_str());
}

FString WidgetElementId(const std::string& ScreenId, const FString& DiagPath)
{
    return FString::Printf(TEXT("%s#widget.%s"), UTF8_TO_TCHAR(ScreenId.c_str()), *DiagPath);
}

bool ScalarKindToControlValueType(GV2ContentCore::EScalarFieldKind Kind, EGV2UiControlValueType& OutType)
{
    switch (Kind)
    {
    case GV2ContentCore::EScalarFieldKind::Boolean: OutType = EGV2UiControlValueType::Boolean; return true;
    case GV2ContentCore::EScalarFieldKind::Integer: OutType = EGV2UiControlValueType::Integer; return true;
    case GV2ContentCore::EScalarFieldKind::Number: OutType = EGV2UiControlValueType::Number; return true;
    case GV2ContentCore::EScalarFieldKind::String: OutType = EGV2UiControlValueType::String; return true;
    case GV2ContentCore::EScalarFieldKind::Enum: return false;
    }
    return false;
}

struct FCollectBindingsContext
{
    const FGV2UiSchemaCache* SchemaCache = nullptr;
    std::string ScreenId;
    TArray<FGV2UiBindingDefinition>* Definitions = nullptr;
};

// PAH-08: phase=prepare -- reached only from PrepareBindingDefinitions, which
// runs while a revision is being materialised, never during application.
bool CollectBindingDefinitions(
    FCollectBindingsContext& Ctx,
    const GV2ContentCore::FCompiledUiFieldSpec& Spec,
    const GV2ContentCore::FValue& MaterializedValue,
    const TArray<FString>& NodePath,
    const FString& ElementKey)
{
    using namespace GV2ContentCore;
    switch (Spec.Kind)
    {
    case EUiFieldKind::Binding:
    {
        FGV2UiBindingDefinition Definition;
        if (!ReadBinding(MaterializedValue, NodePath, WidgetElementId(Ctx.ScreenId, ElementKey), Definition))
        {
            return false;
        }
        if (Spec.BindingInputSchemaId.has_value())
        {
            FString SchemaError;
            GV2ContentCore::FCompiledUiFieldSpecPtr InputSchema =
                Ctx.SchemaCache->GetCompiledSchema(*Spec.BindingInputSchemaId, SchemaError);
            if (InputSchema == nullptr || InputSchema->Kind != EUiFieldKind::Object)
            {
                return false;
            }
            Definition.InputSchemaId = UTF8_TO_TCHAR(Spec.BindingInputSchemaId->c_str());
            for (const FCompiledUiObjectField& InputField : InputSchema->Fields)
            {
                EGV2UiControlValueType InputType;
                if (!InputField.Spec || InputField.Spec->Kind != EUiFieldKind::Scalar
                    || !InputField.Spec->Scalar.has_value()
                    || !ScalarKindToControlValueType(InputField.Spec->Scalar->Kind, InputType))
                {
                    return false;
                }
                FGV2UiInputFieldDefinition& InputDef = Definition.InputFields.AddDefaulted_GetRef();
                InputDef.Name = FName(UTF8_TO_TCHAR(InputField.Name.c_str()));
                InputDef.Type = InputType;
                InputDef.bRequired = InputField.bRequired;
            }
        }
        Ctx.Definitions->Add(MoveTemp(Definition));
        return true;
    }
    case EUiFieldKind::Object:
    {
        if (!MaterializedValue.IsObject()) return false;
        for (const FCompiledUiObjectField& FieldSpec : Spec.Fields)
        {
            const GV2ContentCore::FValue* ChildVal = MaterializedValue.FindField(FieldSpec.Name);
            if (ChildVal == nullptr) continue;
            if (!FieldSpec.Spec) return false;
            if (!CollectBindingDefinitions(
                    Ctx,
                    *FieldSpec.Spec,
                    *ChildVal,
                    NodePath,
                    ElementKey))
            {
                return false;
            }
        }
        return true;
    }
    case EUiFieldKind::Array:
    {
        if (!MaterializedValue.IsArray() || !Spec.Items) return false;
        int32 Index = 0;
        for (const GV2ContentCore::FValue& ItemVal : MaterializedValue.AsArray())
        {
            FString ItemKeyStr = FString::Printf(TEXT("%d"), Index);
            if (Spec.KeyedBy.has_value() && ItemVal.IsObject())
            {
                if (const GV2ContentCore::FValue* KeyVal = ItemVal.FindField(*Spec.KeyedBy))
                {
                    if (KeyVal->IsString())
                    {
                        ItemKeyStr = UTF8_TO_TCHAR(KeyVal->AsString().c_str());
                    }
                }
            }

            TArray<FString> ItemNodePath = NodePath;
            ItemNodePath.Add(ItemKeyStr);
            const FString ItemElementKey = Spec.KeyedBy.has_value() ? ItemKeyStr : ElementKey;

            if (!CollectBindingDefinitions(
                    Ctx,
                    *Spec.Items,
                    ItemVal,
                    ItemNodePath,
                    ItemElementKey))
            {
                return false;
            }
            ++Index;
        }
        return true;
    }
    case EUiFieldKind::Scalar:
    case EUiFieldKind::Key:
    case EUiFieldKind::Text:
    case EUiFieldKind::Ref:
        return true;
    case EUiFieldKind::ScreenFields:
    {
        // DUC-09: a nested screen's fields are an array of the same field_id/
        // schema_id/value envelopes a top-level screen request uses, so bindings
        // inside them are collected by resolving each envelope's own schema and
        // recursing -- the same two envelopes ProjectMaterializedValue resolves
        // below, kept in the same traversal order so binding handles line up.
        if (!MaterializedValue.IsArray())
        {
            return false;
        }
        for (const GV2ContentCore::FValue& EnvelopeVal : MaterializedValue.AsArray())
        {
            if (!EnvelopeVal.IsObject())
            {
                return false;
            }
            const GV2ContentCore::FValue* SchemaIdVal = EnvelopeVal.FindField("schema_id");
            const GV2ContentCore::FValue* FieldIdVal = EnvelopeVal.FindField("field_id");
            const GV2ContentCore::FValue* InnerValueVal = EnvelopeVal.FindField("value");
            if (SchemaIdVal == nullptr || !SchemaIdVal->IsString()
                || FieldIdVal == nullptr || !FieldIdVal->IsString()
                || InnerValueVal == nullptr)
            {
                return false;
            }

            FString SchemaError;
            GV2ContentCore::FCompiledUiFieldSpecPtr InnerSchema =
                Ctx.SchemaCache->GetCompiledSchema(SchemaIdVal->AsString(), SchemaError);
            if (InnerSchema == nullptr)
            {
                return false;
            }

            GV2ContentCore::FValue InnerMaterialized;
            std::vector<GV2ContentCore::FDiagnostic> InnerDiagnostics;
            GV2ContentCore::FValidationDiagnosticContext InnerCtx;
            InnerCtx.SchemaId = SchemaIdVal->AsString();
            if (!GV2ContentCore::ValidateUiFieldValue(
                    *InnerValueVal, *InnerSchema, InnerMaterialized, nullptr, "", InnerCtx, InnerDiagnostics))
            {
                return false;
            }

            const FString FieldIdStr = UTF8_TO_TCHAR(FieldIdVal->AsString().c_str());
            TArray<FString> InnerNodePath = NodePath;
            InnerNodePath.Add(FieldIdStr);
            if (!CollectBindingDefinitions(Ctx, *InnerSchema, InnerMaterialized, InnerNodePath, FieldIdStr))
            {
                return false;
            }
        }
        return true;
    }
    }
    return true;
}

} // anonymous namespace

// PCC-03 verification: exposed (not anonymous-namespace-local) so a test can drive
// the exact same raw-value -> FGV2PreparedUiValue projection BuildFields uses,
// end to end from a hand-built ValidateUiFieldValue() output, without needing a
// file-backed schema in GameData/ just to exercise this step.
namespace GV2ScreenFieldMaterializer
{
// PAH-08: phase=prepare -- materialises a candidate value; every caller is on
// the BuildFields/PrepareBindingDefinitions path.
bool ProjectMaterializedValue(
    FMaterializeContext& Ctx,
    const GV2ContentCore::FCompiledUiFieldSpec& Spec,
    const GV2ContentCore::FValue& MaterializedValue,
    FGV2PreparedUiValue& OutValue)
{
    using namespace GV2ContentCore;
    if (MaterializedValue.IsNull())
    {
        OutValue = FGV2PreparedUiValue::MakeNull();
        return true;
    }

    switch (Spec.Kind)
    {
    case EUiFieldKind::Scalar:
    {
        if (!Spec.Scalar.has_value()) return false;
        switch (Spec.Scalar->Kind)
        {
        case EScalarFieldKind::Boolean:
            OutValue = FGV2PreparedUiValue::MakeBoolean(MaterializedValue.AsBoolean());
            return true;
        case EScalarFieldKind::Integer:
            OutValue = FGV2PreparedUiValue::MakeInteger(MaterializedValue.AsInteger());
            return true;
        case EScalarFieldKind::Number:
            OutValue = FGV2PreparedUiValue::MakeNumber(MaterializedValue.AsNumber());
            return true;
        case EScalarFieldKind::String:
            OutValue = FGV2PreparedUiValue::MakeString(UTF8_TO_TCHAR(MaterializedValue.AsString().c_str()));
            return true;
        case EScalarFieldKind::Enum:
            return false;
        }
        return false;
    }
    case EUiFieldKind::Key:
    {
        OutValue = FGV2PreparedUiValue::MakeKey(UTF8_TO_TCHAR(MaterializedValue.AsString().c_str()));
        return true;
    }
    case EUiFieldKind::Text:
    {
        FGV2TextViewModel Resolved;
        if (!ResolveText(MaterializedValue, Resolved, Ctx.PrepareContext))
        {
            return false;
        }
        OutValue = FGV2PreparedUiValue::MakeText(MoveTemp(Resolved));
        return true;
    }
    case EUiFieldKind::Ref:
    {
        OutValue = FGV2PreparedUiValue::MakeStableId(
            UTF8_TO_TCHAR(MaterializedValue.AsString().c_str()),
            UTF8_TO_TCHAR(Spec.RefTargetKind.c_str()));
        return true;
    }
    case EUiFieldKind::Binding:
    {
        check(Ctx.Handles != nullptr && Ctx.HandleCursor != nullptr);
        if (!Ctx.Handles->IsValidIndex(*Ctx.HandleCursor))
        {
            return false;
        }
        OutValue = FGV2PreparedUiValue::MakeBinding((*Ctx.Handles)[*Ctx.HandleCursor]);
        ++(*Ctx.HandleCursor);
        return true;
    }
    case EUiFieldKind::Object:
    {
        if (!MaterializedValue.IsObject()) return false;
        TArray<TPair<FString, FGV2PreparedUiValue>> MaterializedFields;
        for (const FCompiledUiObjectField& FieldSpec : Spec.Fields)
        {
            const GV2ContentCore::FValue* ChildVal = MaterializedValue.FindField(FieldSpec.Name);
            if (ChildVal == nullptr)
            {
                continue;
            }
            if (!FieldSpec.Spec) return false;

            FGV2PreparedUiValue ChildPrepared;
            if (!ProjectMaterializedValue(Ctx, *FieldSpec.Spec, *ChildVal, ChildPrepared))
            {
                return false;
            }
            MaterializedFields.Emplace(UTF8_TO_TCHAR(FieldSpec.Name.c_str()), MoveTemp(ChildPrepared));
        }
        OutValue = FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(MaterializedFields)));
        return true;
    }
    case EUiFieldKind::Array:
    {
        if (!MaterializedValue.IsArray() || !Spec.Items) return false;
        TArray<FGV2PreparedUiValue> MaterializedItems;
        for (const GV2ContentCore::FValue& ItemVal : MaterializedValue.AsArray())
        {
            FGV2PreparedUiValue ItemPrepared;
            if (!ProjectMaterializedValue(Ctx, *Spec.Items, ItemVal, ItemPrepared))
            {
                return false;
            }
            MaterializedItems.Add(MoveTemp(ItemPrepared));
        }
        OutValue = FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(MoveTemp(MaterializedItems)));
        return true;
    }
    case EUiFieldKind::ScreenFields:
    {
        // DUC-09: a nested screen's fields are an array of the same field_id/
        // schema_id/value envelope a top-level GV2RuntimeCore::FScreenField uses --
        // no separate protocol. Each envelope's own schema_id is resolved and its
        // value independently validated + materialized through the exact same
        // ValidateUiFieldValue + ProjectMaterializedValue pair BuildFields uses for
        // a top-level field, recursively. The result is a plain Array<Object{
        // field_id: Key, schema_id: String, value: Object}> -- no new prepared-value
        // kind needed; the consumer re-resolves each envelope's compiled schema by
        // schema_id (a cheap cache hit) to build a real TArray<FGV2ScreenFieldValue>
        // and apply it through UGV2ScreenWidgetBase::PrepareScreenFields/CommitScreenFields,
        // the same public API a top-level screen uses.
        if (!MaterializedValue.IsArray())
        {
            return false;
        }
        TArray<FGV2PreparedUiValue> NestedEnvelopes;
        for (const GV2ContentCore::FValue& EnvelopeVal : MaterializedValue.AsArray())
        {
            if (!EnvelopeVal.IsObject())
            {
                return false;
            }
            const GV2ContentCore::FValue* FieldIdVal = EnvelopeVal.FindField("field_id");
            const GV2ContentCore::FValue* SchemaIdVal = EnvelopeVal.FindField("schema_id");
            const GV2ContentCore::FValue* InnerValueVal = EnvelopeVal.FindField("value");
            if (FieldIdVal == nullptr || !FieldIdVal->IsString()
                || SchemaIdVal == nullptr || !SchemaIdVal->IsString()
                || InnerValueVal == nullptr)
            {
                return false;
            }

            const FString SchemaIdStr = UTF8_TO_TCHAR(SchemaIdVal->AsString().c_str());
            FString SchemaError;
            GV2ContentCore::FCompiledUiFieldSpecPtr InnerSchema =
                GetSchemaCache().GetCompiledSchema(SchemaIdVal->AsString(), SchemaError);
            if (InnerSchema == nullptr)
            {
                return false;
            }

            GV2ContentCore::FValue InnerMaterialized;
            std::vector<GV2ContentCore::FDiagnostic> InnerDiagnostics;
            GV2ContentCore::FValidationDiagnosticContext InnerCtx;
            InnerCtx.SchemaId = SchemaIdVal->AsString();
            if (!GV2ContentCore::ValidateUiFieldValue(
                    *InnerValueVal, *InnerSchema, InnerMaterialized, nullptr, "", InnerCtx, InnerDiagnostics))
            {
                return false;
            }

            FGV2PreparedUiValue InnerPrepared;
            if (!ProjectMaterializedValue(Ctx, *InnerSchema, InnerMaterialized, InnerPrepared)
                || !InnerPrepared.IsObject())
            {
                return false;
            }

            TArray<TPair<FString, FGV2PreparedUiValue>> EnvelopeFields;
            EnvelopeFields.Emplace(TEXT("field_id"), FGV2PreparedUiValue::MakeKey(UTF8_TO_TCHAR(FieldIdVal->AsString().c_str())));
            EnvelopeFields.Emplace(TEXT("schema_id"), FGV2PreparedUiValue::MakeString(SchemaIdStr));
            EnvelopeFields.Emplace(TEXT("value"), MoveTemp(InnerPrepared));
            NestedEnvelopes.Add(FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(EnvelopeFields))));
        }
        OutValue = FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(MoveTemp(NestedEnvelopes)));
        return true;
    }
    }
    return false;
}
} // namespace GV2ScreenFieldMaterializer

namespace
{
static void NormalizeArraysInContentValue(
    const GV2ContentCore::FCompiledUiFieldSpec& Spec,
    GV2ContentCore::FValue& Value)
{
    using namespace GV2ContentCore;
    if (Spec.Kind == EUiFieldKind::Array)
    {
        if (Value.IsObject() && Value.AsObject().empty())
        {
            Value = FValue::MakeArray({});
            return;
        }
        if (Value.IsArray() && Spec.Items)
        {
            for (FValue& Item : Value.AsArray())
            {
                NormalizeArraysInContentValue(*Spec.Items, Item);
            }
        }
    }
    else if (Spec.Kind == EUiFieldKind::Object && Value.IsObject())
    {
        for (const FCompiledUiObjectField& Field : Spec.Fields)
        {
            if (Field.Spec)
            {
                if (FValue* Child = Value.FindField(Field.Name))
                {
                    NormalizeArraysInContentValue(*Field.Spec, *Child);
                }
            }
        }
    }
}
} // anonymous namespace

namespace GV2ScreenFieldMaterializer
{
void RebuildSchemaCacheForSession(TArray<FGV2SchemaPackageRoot> PackageRoots)
{
    GSessionSchemaCache.Emplace(MoveTemp(PackageRoots));
}

void ReleaseSchemaCacheForSession()
{
    GSessionSchemaCache.Reset();
}

// DUC-09: lets a nested-screen-fields consumer (FGV2TabContainerTabsPropertyConsumer)
// re-resolve one envelope's compiled schema by schema_id -- a cache hit against the
// same session-scoped cache ProjectMaterializedValue already resolved it through
// above, needed only to fill FGV2ScreenFieldValue::CompiledSchema for
// PrepareScreenFields.
// PAH-08: phase=authority -- free-function face of the same cache.
GV2ContentCore::FCompiledUiFieldSpecPtr GetCompiledSchema(const std::string& SchemaId, FString& OutError)
{
    return GetSchemaCache().GetCompiledSchema(SchemaId, OutError);
}

// PAH-08: phase=prepare -- named for its phase; called before any mutation.
bool PrepareBindingDefinitions(
    const GV2RuntimeCore::FScreenRequest& Request,
    TArray<FGV2UiBindingDefinition>& OutDefinitions)
{
    OutDefinitions.Reset();
    for (const GV2RuntimeCore::FScreenField& Field : Request.Fields)
    {
        FString SchemaError;
        GV2ContentCore::FCompiledUiFieldSpecPtr Schema = GetSchemaCache().GetCompiledSchema(Field.SchemaId, SchemaError);
        if (!Schema)
        {
            OutDefinitions.Reset();
            return false;
        }

        GV2ContentCore::FValue ContentValue = GV2RuntimeCore::RuntimeValueToContentValue(Field.Value);
        NormalizeArraysInContentValue(*Schema, ContentValue);

        GV2ContentCore::FValue Materialized;
        std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
        GV2ContentCore::FValidationDiagnosticContext Ctx;
        Ctx.SchemaId = Field.SchemaId;
        if (!GV2ContentCore::ValidateUiFieldValue(
                ContentValue,
                *Schema,
                Materialized,
                nullptr,
                "",
                Ctx,
                Diagnostics))
        {
            for (const GV2ContentCore::FDiagnostic& Diag : Diagnostics)
            {
                if (Diag.Code == "core:diagnostic.ui_schema.value.unknown_field")
                {
                    UE_LOG(LogTemp, Error, TEXT("ScreenField: '%s': %s rejected (closed schema)"),
                        *FieldIdOf(Field), UTF8_TO_TCHAR(Diag.Message.c_str()));
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("ScreenField: '%s': %s at '%s' [%s]"),
                        *FieldIdOf(Field), UTF8_TO_TCHAR(Diag.Message.c_str()), UTF8_TO_TCHAR(Diag.JsonPointer.value_or("").c_str()), UTF8_TO_TCHAR(Diag.Code.c_str()));
                }
            }
            OutDefinitions.Reset();
            return false;
        }

        FCollectBindingsContext BindContext;
        BindContext.SchemaCache = &GetSchemaCache();
        BindContext.ScreenId = Request.ScreenId;
        BindContext.Definitions = &OutDefinitions;
        if (!CollectBindingDefinitions(
                BindContext,
                *Schema,
                Materialized,
                {TEXT("route"), TEXT("main"), FieldIdOf(Field)},
                FieldIdOf(Field)))
        {
            UE_LOG(LogTemp, Warning, TEXT("ScreenField: '%s': CollectBindingDefinitions returned false"), *FieldIdOf(Field));
            OutDefinitions.Reset();
            return false;
        }
    }
    return true;
}

// PAH-08: phase=prepare -- builds the candidate field set from the document.
bool BuildFields(
    const GV2RuntimeCore::FScreenRequest& Request,
    const TArray<FGV2UiBindingHandle>& Handles,
    TArray<FGV2ScreenFieldValue>& OutFields,
    const FGV2PresentationPrepareContext* PrepareContext)
{
    OutFields.Reset();
    OutFields.Reserve(static_cast<int32>(Request.Fields.size()));
    int32 HandleCursor = 0;

    for (const GV2RuntimeCore::FScreenField& Field : Request.Fields)
    {
        FString SchemaError;
        GV2ContentCore::FCompiledUiFieldSpecPtr Schema = GetSchemaCache().GetCompiledSchema(Field.SchemaId, SchemaError);
        if (!Schema)
        {
            UE_LOG(LogTemp, Error, TEXT("BuildFields: schema '%s' could not be compiled: %s"), UTF8_TO_TCHAR(Field.SchemaId.c_str()), *SchemaError);
            OutFields.Reset();
            return false;
        }

        GV2ContentCore::FValue ContentValue = GV2RuntimeCore::RuntimeValueToContentValue(Field.Value);
        NormalizeArraysInContentValue(*Schema, ContentValue);

        GV2ContentCore::FValue Materialized;
        std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
        GV2ContentCore::FValidationDiagnosticContext Ctx;
        Ctx.SchemaId = Field.SchemaId;
        if (!GV2ContentCore::ValidateUiFieldValue(
                ContentValue,
                *Schema,
                Materialized,
                nullptr,
                "",
                Ctx,
                Diagnostics))
        {
            for (const GV2ContentCore::FDiagnostic& Diag : Diagnostics)
            {
                if (Diag.Code == "core:diagnostic.ui_schema.value.unknown_field")
                {
                    UE_LOG(LogTemp, Error, TEXT("ScreenField: '%s': %s rejected (closed schema)"),
                        *FieldIdOf(Field), UTF8_TO_TCHAR(Diag.Message.c_str()));
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("ScreenField: '%s': %s at '%s' [%s]"),
                        *FieldIdOf(Field), UTF8_TO_TCHAR(Diag.Message.c_str()), UTF8_TO_TCHAR(Diag.JsonPointer.value_or("").c_str()), UTF8_TO_TCHAR(Diag.Code.c_str()));
                }
            }
            OutFields.Reset();
            return false;
        }

        FMaterializeContext MatContext;
        MatContext.Handles = &Handles;
        MatContext.HandleCursor = &HandleCursor;
        MatContext.PrepareContext = PrepareContext;
        FGV2PreparedUiValue PreparedValue;
        if (!ProjectMaterializedValue(
                MatContext,
                *Schema,
                Materialized,
                PreparedValue)
            || !PreparedValue.IsObject())
        {
            UE_LOG(LogTemp, Error, TEXT("BuildFields: '%s' ProjectMaterializedValue failed or did not produce an object"), *FieldIdOf(Field));
            OutFields.Reset();
            return false;
        }

        FGV2ScreenFieldValue& OutField = OutFields.AddDefaulted_GetRef();
        OutField.FieldId = FName(FieldIdOf(Field));
        OutField.SchemaId = UTF8_TO_TCHAR(Field.SchemaId.c_str());
        OutField.PreparedValue = PreparedValue.AsObjectRef();
        OutField.CompiledSchema = Schema;
    }

    if (HandleCursor != Handles.Num())
    {
        UE_LOG(LogTemp, Error, TEXT("BuildFields consumed %d of %d prepared binding handles (internal consistency error)"), HandleCursor, Handles.Num());
        OutFields.Reset();
        return false;
    }
    return true;
}
} // namespace GV2ScreenFieldMaterializer
