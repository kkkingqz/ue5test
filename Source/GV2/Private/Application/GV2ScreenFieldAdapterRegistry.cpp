#include "Application/GV2ScreenFieldAdapterRegistry.h"

#include "GV2RuntimeCore/GV2StableId.h"
#include "UI/GV2PreparedUiValue.h"
#include "UI/GV2TextPipeline.h"

#include <algorithm>
#include <set>

namespace
{
using FObject = GV2RuntimeCore::FValue::FObject;
using FArray = GV2RuntimeCore::FValue::FArray;

const FObject* AsObject(const GV2RuntimeCore::FValue& Value)
{
    return std::get_if<FObject>(&Value.Data);
}

const FArray* AsArray(const GV2RuntimeCore::FValue& Value)
{
    if (const FArray* Array = std::get_if<FArray>(&Value.Data))
    {
        return Array;
    }
    static const FArray EmptyArray;
    const FObject* Object = std::get_if<FObject>(&Value.Data);
    return Object != nullptr && Object->empty() ? &EmptyArray : nullptr;
}

const GV2RuntimeCore::FValue* FindValue(const FObject& Object, const std::string_view Name)
{
    const auto It = Object.find(Name);
    return It != Object.end() ? &It->second : nullptr;
}

const std::string* FindString(const FObject& Object, const std::string_view Name)
{
    const GV2RuntimeCore::FValue* Value = FindValue(Object, Name);
    return Value != nullptr ? std::get_if<std::string>(&Value->Data) : nullptr;
}

bool ToControlValue(
    const std::string& Name,
    const GV2RuntimeCore::FValue& Value,
    FGV2UiControlValue& OutValue)
{
    OutValue = {};
    OutValue.Name = FName(UTF8_TO_TCHAR(Name.c_str()));
    if (OutValue.Name.IsNone()) return false;
    if (const bool* Boolean = std::get_if<bool>(&Value.Data))
    {
        OutValue.Type = EGV2UiControlValueType::Boolean;
        OutValue.BooleanValue = *Boolean;
        return true;
    }
    if (const std::int64_t* Integer = std::get_if<std::int64_t>(&Value.Data))
    {
        OutValue.Type = EGV2UiControlValueType::Integer;
        OutValue.IntegerValue = *Integer;
        return true;
    }
    if (const double* Number = std::get_if<double>(&Value.Data))
    {
        if (!FMath::IsFinite(*Number)) return false;
        OutValue.Type = EGV2UiControlValueType::Number;
        OutValue.NumberValue = *Number;
        return true;
    }
    if (const std::string* String = std::get_if<std::string>(&Value.Data))
    {
        OutValue.Type = EGV2UiControlValueType::String;
        OutValue.StringValue = UTF8_TO_TCHAR(String->c_str());
        return true;
    }
    return false;
}

bool ReadTextSpec(const GV2RuntimeCore::FValue& Value, GV2RuntimeCore::FTextSpec& OutSpec)
{
    const FObject* Object = AsObject(Value);
    if (Object == nullptr) return false;
    for (const auto& [Key, Val] : *Object)
    {
        if (Key != "text_id" && Key != "style" && Key != "args")
        {
            UE_LOG(LogTemp, Error, TEXT("ScreenField: TextSpec has unknown key '%s' rejected (closed schema)"), UTF8_TO_TCHAR(Key.c_str()));
            return false;
        }
    }
    const std::string* TextId = FindString(*Object, "text_id");
    if (TextId == nullptr || !GV2RuntimeCore::FStableId::IsOfKind(*TextId, "text")) return false;
    OutSpec = {};
    OutSpec.TextId = *TextId;
    if (const std::string* Style = FindString(*Object, "style")) OutSpec.Style = *Style;
    if (const GV2RuntimeCore::FValue* Args = FindValue(*Object, "args"))
    {
        const FObject* ArgsObject = AsObject(*Args);
        if (ArgsObject == nullptr) return false;
        OutSpec.Args = *ArgsObject;
    }
    return true;
}

bool ResolveText(const GV2RuntimeCore::FValue& Value, FGV2TextViewModel& OutText)
{
    GV2RuntimeCore::FTextSpec Spec;
    if (!ReadTextSpec(Value, Spec)) return false;
    TArray<FGV2UiControlValue> Args;
    Args.Reserve(static_cast<int32>(Spec.Args.size()));
    for (const auto& [Name, Argument] : Spec.Args)
    {
        FGV2UiControlValue& Converted = Args.AddDefaulted_GetRef();
        if (!ToControlValue(Name, Argument, Converted)) return false;
    }
    FString Error;
    return UGV2TextPipeline::Resolve(
        UTF8_TO_TCHAR(Spec.TextId.c_str()),
        Args,
        FName(UTF8_TO_TCHAR(Spec.Style.c_str())),
        OutText,
        Error);
}

bool IsValidKeyValue(const std::string& Key)
{
    if (Key.empty() || Key.length() > 192)
    {
        return false;
    }
    for (char C : Key)
    {
        if (!((C >= 'a' && C <= 'z') || (C >= '0' && C <= '9') || C == '_' || C == '-' || C == '.' || C == '@' || C == ':'))
        {
            return false;
        }
    }
    if (Key.rfind("text:", 0) == 0 || GV2RuntimeCore::FStableId::IsOfKind(Key, "text"))
    {
        return false;
    }
    return true;
}

bool ValidateRepeatedElementKey(
    const std::string* Key,
    TSet<FName>& InOutSeenKeys)
{
    if (Key == nullptr || !IsValidKeyValue(*Key))
    {
        return false;
    }
    const FName KeyName(UTF8_TO_TCHAR(Key->c_str()));
    if (InOutSeenKeys.Contains(KeyName))
    {
        return false;
    }
    InOutSeenKeys.Add(KeyName);
    return true;
}

bool ReadBinding(
    const GV2RuntimeCore::FValue& Value,
    const TArray<FString>& NodePath,
    const FString& ElementId,
    FGV2UiBindingDefinition& OutDefinition)
{
    if (const std::string* CommandIdStr = std::get_if<std::string>(&Value.Data))
    {
        if (!GV2RuntimeCore::FStableId::IsOfKind(*CommandIdStr, "command")) return false;
        OutDefinition = {};
        OutDefinition.NodeKeyPath = NodePath;
        OutDefinition.ElementId = ElementId;
        OutDefinition.CommandId = UTF8_TO_TCHAR(CommandIdStr->c_str());
        return true;
    }

    const FObject* Object = AsObject(Value);
    if (Object == nullptr) return false;
    for (const auto& [Key, Val] : *Object)
    {
        if (Key != "command_id" && Key != "args")
        {
            // BAI-03/REV3-03: matches the Object-kind closed-schema rejection in
            // WalkFieldValue -- Binding's {command_id, args} shape is fixed by the
            // Binding kind itself, not schema.Fields-driven, but an unknown key inside
            // it is exactly the same "property nobody consumes" violation.
            UE_LOG(LogTemp, Error, TEXT("ScreenField: Binding value has unknown key '%s' rejected (closed schema)"), UTF8_TO_TCHAR(Key.c_str()));
            return false;
        }
    }
    const std::string* CommandId = FindString(*Object, "command_id");
    if (CommandId == nullptr
        || !GV2RuntimeCore::FStableId::IsOfKind(*CommandId, "command")) return false;
    OutDefinition = {};
    OutDefinition.NodeKeyPath = NodePath;
    OutDefinition.ElementId = ElementId;
    OutDefinition.CommandId = UTF8_TO_TCHAR(CommandId->c_str());
    if (const GV2RuntimeCore::FValue* Args = FindValue(*Object, "args"))
    {
        const FObject* ArgsObject = AsObject(*Args);
        if (ArgsObject == nullptr) return false;
        for (const auto& [Name, Argument] : *ArgsObject)
        {
            FGV2UiControlValue& Converted = OutDefinition.BoundArgs.AddDefaulted_GetRef();
            if (!ToControlValue(Name, Argument, Converted)) return false;
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
    default: return false;
    }
}

// UPP-27: the single schema-driven walk that replaces every former per-schema
// adapter. It is called twice per field with the exact same (Spec, RawValue)
// tree, in the exact same deterministic order (schema field-declaration order
// for objects, array order for arrays):
//   1. Collect pass (bOutValue == nullptr, Ctx.CollectDefinitions != nullptr):
//      walks the tree validating it against Spec (closed objects, required
//      fields, key identity, scalar/text/ref shape) and appends one
//      FGV2UiBindingDefinition per Binding-kind node it finds.
//   2. Materialize pass (Ctx.Handles != nullptr, OutValue != nullptr): walks
//      the *same* tree again, re-validating it (defensive, not just trusting
//      the first pass), and this time builds the real FGV2PreparedUiValue,
//      consuming the next handle from Ctx.Handles (produced by
//      FGV2UiBindingRegistry::PrepareBindings against pass 1's definitions,
//      in the same order) for every Binding-kind node instead of reading one.
// A field or array item absent from the raw value but optional in the schema
// is simply omitted from the materialized Object -- PrepareUiHostProperties
// already treats "absent from Candidate" as a reset mutation.
struct FWalkContext
{
    const FGV2UiSchemaCache* SchemaCache = nullptr;
    std::string ScreenId;
    TArray<FGV2UiBindingDefinition>* CollectDefinitions = nullptr;
    const TArray<FGV2UiBindingHandle>* Handles = nullptr;
    int32* HandleCursor = nullptr;
};

bool WalkFieldValue(
    FWalkContext& Ctx,
    const GV2ContentCore::FCompiledUiFieldSpec& Spec,
    const GV2RuntimeCore::FValue& RawValue,
    const TArray<FString>& NodePath,
    const FString& ElementKey,
    const FString& DiagPath,
    FGV2PreparedUiValue* OutValue,
    FString& OutError)
{
    using namespace GV2ContentCore;

    switch (Spec.Kind)
    {
    case EUiFieldKind::Scalar:
    {
        if (!Spec.Scalar.has_value())
        {
            OutError = FString::Printf(TEXT("'%s': scalar field has no scalar spec"), *DiagPath);
            return false;
        }
        switch (Spec.Scalar->Kind)
        {
        case EScalarFieldKind::Boolean:
            if (const bool* B = std::get_if<bool>(&RawValue.Data))
            {
                if (OutValue) *OutValue = FGV2PreparedUiValue::MakeBoolean(*B);
                return true;
            }
            break;
        case EScalarFieldKind::Integer:
            if (const std::int64_t* I = std::get_if<std::int64_t>(&RawValue.Data))
            {
                if (OutValue) *OutValue = FGV2PreparedUiValue::MakeInteger(*I);
                return true;
            }
            break;
        case EScalarFieldKind::Number:
            if (const double* D = std::get_if<double>(&RawValue.Data))
            {
                if (OutValue) *OutValue = FGV2PreparedUiValue::MakeNumber(*D);
                return true;
            }
            if (const std::int64_t* I = std::get_if<std::int64_t>(&RawValue.Data))
            {
                if (OutValue) *OutValue = FGV2PreparedUiValue::MakeNumber(static_cast<double>(*I));
                return true;
            }
            break;
        case EScalarFieldKind::String:
            if (const std::string* S = std::get_if<std::string>(&RawValue.Data))
            {
                if (OutValue) *OutValue = FGV2PreparedUiValue::MakeString(UTF8_TO_TCHAR(S->c_str()));
                return true;
            }
            break;
        default:
            break;
        }
        OutError = FString::Printf(TEXT("'%s': value does not match its scalar schema kind"), *DiagPath);
        return false;
    }
    case EUiFieldKind::Key:
    {
        const std::string* S = std::get_if<std::string>(&RawValue.Data);
        if (S == nullptr || !IsValidKeyValue(*S))
        {
            OutError = FString::Printf(TEXT("'%s': invalid key value"), *DiagPath);
            return false;
        }
        if (OutValue) *OutValue = FGV2PreparedUiValue::MakeKey(UTF8_TO_TCHAR(S->c_str()));
        return true;
    }
    case EUiFieldKind::Text:
    {
        // Shape-only in the collect pass (binding discovery never needed localized
        // text, and requiring the text pipeline to resolve during collection would
        // make PrepareBindingDefinitions depend on catalog/theme setup it has no
        // business needing); full resolution only when actually materializing.
        if (OutValue == nullptr)
        {
            GV2RuntimeCore::FTextSpec ShapeOnly;
            if (!ReadTextSpec(RawValue, ShapeOnly))
            {
                OutError = FString::Printf(TEXT("'%s': malformed TextSpec"), *DiagPath);
                return false;
            }
            return true;
        }
        FGV2TextViewModel Resolved;
        if (!ResolveText(RawValue, Resolved))
        {
            OutError = FString::Printf(TEXT("'%s': invalid or unresolvable TextSpec"), *DiagPath);
            return false;
        }
        *OutValue = FGV2PreparedUiValue::MakeText(MoveTemp(Resolved));
        return true;
    }
    case EUiFieldKind::Ref:
    {
        const std::string* S = std::get_if<std::string>(&RawValue.Data);
        if (S == nullptr || !GV2RuntimeCore::FStableId::IsOfKind(*S, Spec.RefTargetKind))
        {
            OutError = FString::Printf(TEXT("'%s': value is not a Stable ID of target_kind '%s'"), *DiagPath, UTF8_TO_TCHAR(Spec.RefTargetKind.c_str()));
            return false;
        }
        if (OutValue) *OutValue = FGV2PreparedUiValue::MakeStableId(UTF8_TO_TCHAR(S->c_str()), UTF8_TO_TCHAR(Spec.RefTargetKind.c_str()));
        return true;
    }
    case EUiFieldKind::Binding:
    {
        if (Ctx.CollectDefinitions != nullptr)
        {
            FGV2UiBindingDefinition Definition;
            if (!ReadBinding(RawValue, NodePath, WidgetElementId(Ctx.ScreenId, ElementKey), Definition))
            {
                OutError = FString::Printf(TEXT("'%s': invalid Binding value"), *DiagPath);
                return false;
            }
            if (Spec.BindingInputSchemaId.has_value())
            {
                FString SchemaError;
                GV2ContentCore::FCompiledUiFieldSpecPtr InputSchema =
                    Ctx.SchemaCache->GetCompiledSchema(*Spec.BindingInputSchemaId, SchemaError);
                if (InputSchema == nullptr || InputSchema->Kind != EUiFieldKind::Object)
                {
                    OutError = FString::Printf(
                        TEXT("'%s': binding_input_schema_id '%s' did not resolve to a compiled Object schema (%s)"),
                        *DiagPath, UTF8_TO_TCHAR(Spec.BindingInputSchemaId->c_str()), *SchemaError);
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
                        OutError = FString::Printf(TEXT("'%s': binding input field '%s' is not a supported scalar kind"), *DiagPath, UTF8_TO_TCHAR(InputField.Name.c_str()));
                        return false;
                    }
                    FGV2UiInputFieldDefinition& InputDef = Definition.InputFields.AddDefaulted_GetRef();
                    InputDef.Name = FName(UTF8_TO_TCHAR(InputField.Name.c_str()));
                    InputDef.Type = InputType;
                    InputDef.bRequired = InputField.bRequired;
                }
            }
            Ctx.CollectDefinitions->Add(MoveTemp(Definition));
            return true;
        }

        check(Ctx.Handles != nullptr && Ctx.HandleCursor != nullptr);
        // Defensive re-validation: the materialize pass never trusts that the
        // collect pass already ran cleanly against this exact value.
        FGV2UiBindingDefinition Discard;
        if (!ReadBinding(RawValue, NodePath, WidgetElementId(Ctx.ScreenId, ElementKey), Discard))
        {
            OutError = FString::Printf(TEXT("'%s': invalid Binding value"), *DiagPath);
            return false;
        }
        if (!Ctx.Handles->IsValidIndex(*Ctx.HandleCursor))
        {
            OutError = FString::Printf(TEXT("'%s': binding handle set is shorter than the value tree (internal consistency error)"), *DiagPath);
            return false;
        }
        if (OutValue) *OutValue = FGV2PreparedUiValue::MakeBinding((*Ctx.Handles)[*Ctx.HandleCursor]);
        ++(*Ctx.HandleCursor);
        return true;
    }
    case EUiFieldKind::Object:
    {
        const FObject* Object = AsObject(RawValue);
        if (Object == nullptr)
        {
            OutError = FString::Printf(TEXT("'%s': value is not an object"), *DiagPath);
            return false;
        }
        for (const auto& [Key, Val] : *Object)
        {
            const bool bKnown = std::any_of(
                Spec.Fields.begin(), Spec.Fields.end(),
                [&Key](const FCompiledUiObjectField& F) { return F.Name == Key; });
            if (!bKnown)
            {
                OutError = FString::Printf(TEXT("'%s': unknown key '%s' rejected (closed schema)"), *DiagPath, UTF8_TO_TCHAR(Key.c_str()));
                // BAI-03/REV3-03: this is the one rejection reason worth an Error-severity
                // log on its own -- a property nobody consumes must be loud, not just
                // returned as a bool. Every other validation failure below (missing
                // required field, bad key grammar, type mismatch, unresolvable text,
                // malformed binding) is reported only through OutError/the bool return,
                // exactly like the pre-UPP-27 helpers this replaces.
                UE_LOG(LogTemp, Error, TEXT("ScreenField: %s"), *OutError);
                return false;
            }
        }

        TArray<TPair<FString, FGV2PreparedUiValue>> MaterializedFields;
        for (const FCompiledUiObjectField& FieldSpec : Spec.Fields)
        {
            const FString ChildName = UTF8_TO_TCHAR(FieldSpec.Name.c_str());
            const GV2RuntimeCore::FValue* ChildRaw = FindValue(*Object, FieldSpec.Name);
            if (ChildRaw == nullptr)
            {
                if (FieldSpec.bRequired)
                {
                    OutError = FString::Printf(TEXT("'%s': required field '%s' is absent"), *DiagPath, *ChildName);
                    return false;
                }
                continue;
            }
            if (!FieldSpec.Spec)
            {
                OutError = FString::Printf(TEXT("'%s.%s': field has no compiled spec"), *DiagPath, *ChildName);
                return false;
            }
            const FString ChildDiagPath = DiagPath.IsEmpty() ? ChildName : FString::Printf(TEXT("%s.%s"), *DiagPath, *ChildName);
            FGV2PreparedUiValue ChildValue;
            if (!WalkFieldValue(Ctx, *FieldSpec.Spec, *ChildRaw, NodePath, ElementKey, ChildDiagPath, OutValue ? &ChildValue : nullptr, OutError))
            {
                return false;
            }
            if (OutValue)
            {
                MaterializedFields.Emplace(ChildName, MoveTemp(ChildValue));
            }
        }
        if (OutValue) *OutValue = FGV2PreparedUiValue::MakeObject(FGV2PreparedUiObject::Create(MoveTemp(MaterializedFields)));
        return true;
    }
    case EUiFieldKind::Array:
    {
        const FArray* Items = AsArray(RawValue);
        if (Items == nullptr)
        {
            OutError = FString::Printf(TEXT("'%s': value is not an array"), *DiagPath);
            return false;
        }
        if (!Spec.Items)
        {
            OutError = FString::Printf(TEXT("'%s': array schema has no item spec"), *DiagPath);
            return false;
        }
        if (Spec.MinimumItems.has_value() && Items->size() < *Spec.MinimumItems)
        {
            OutError = FString::Printf(TEXT("'%s': has fewer than the minimum %llu item(s)"), *DiagPath, static_cast<unsigned long long>(*Spec.MinimumItems));
            return false;
        }
        if (Spec.MaximumItems.has_value() && Items->size() > *Spec.MaximumItems)
        {
            OutError = FString::Printf(TEXT("'%s': has more than the maximum %llu item(s)"), *DiagPath, static_cast<unsigned long long>(*Spec.MaximumItems));
            return false;
        }

        TSet<FName> SeenKeys;
        TArray<FGV2PreparedUiValue> MaterializedItems;
        int32 Index = 0;
        for (const GV2RuntimeCore::FValue& ItemRaw : *Items)
        {
            const FObject* ItemObject = AsObject(ItemRaw);
            FString ItemKeyStr = FString::Printf(TEXT("%d"), Index);
            if (Spec.KeyedBy.has_value())
            {
                if (ItemObject == nullptr)
                {
                    OutError = FString::Printf(TEXT("'%s[%d]': keyed array item is not an object"), *DiagPath, Index);
                    return false;
                }
                const std::string* KeyRaw = FindString(*ItemObject, *Spec.KeyedBy);
                if (!ValidateRepeatedElementKey(KeyRaw, SeenKeys))
                {
                    OutError = FString::Printf(TEXT("'%s[%d]': missing, invalid, or duplicate '%s'"), *DiagPath, Index, UTF8_TO_TCHAR(Spec.KeyedBy->c_str()));
                    return false;
                }
                ItemKeyStr = UTF8_TO_TCHAR(KeyRaw->c_str());
            }

            TArray<FString> ItemNodePath = NodePath;
            ItemNodePath.Add(ItemKeyStr);
            const FString ItemDiagPath = FString::Printf(TEXT("%s.%s"), *DiagPath, *ItemKeyStr);
            // Matches the pre-UPP-27 element id scheme: an array item's element id is
            // its own key, not the array's field path -- Spec.KeyedBy already guarantees
            // uniqueness within this one array, and every schema in production today has
            // at most one bindable keyed array per field, so this cannot collide.
            const FString ItemElementKey = Spec.KeyedBy.has_value() ? ItemKeyStr : ElementKey;

            FGV2PreparedUiValue ItemValue;
            if (!WalkFieldValue(Ctx, *Spec.Items, ItemRaw, ItemNodePath, ItemElementKey, ItemDiagPath, OutValue ? &ItemValue : nullptr, OutError))
            {
                return false;
            }
            if (OutValue)
            {
                MaterializedItems.Add(MoveTemp(ItemValue));
            }
            ++Index;
        }
        if (OutValue) *OutValue = FGV2PreparedUiValue::MakeArray(FGV2PreparedUiArray::Create(MoveTemp(MaterializedItems)));
        return true;
    }
    case EUiFieldKind::ScreenFields:
    default:
        OutError = FString::Printf(TEXT("'%s': field kind is not supported by the screen field materializer"), *DiagPath);
        return false;
    }
}

TArray<FString> DiscoverDefaultSchemaPackageRoots()
{
    const FString GameDataDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData"));

    TArray<FString> Roots = {
        FPaths::Combine(GameDataDir, TEXT("core")),
        FPaths::Combine(GameDataDir, TEXT("textsystem")),
        FPaths::Combine(GameDataDir, TEXT("rh")),
        FPaths::Combine(GameDataDir, TEXT("sample")),
    };
    // Schema discovery is purely additive lookup-by-id, unlike loading a
    // package's actual definitions/scripts: scanning both "rh" and "sample"
    // even though only one is ever an active gameplay package cannot collide
    // (schema ids are globally namespaced) and keeps this cache free of the
    // Editor-settings/test-override branching that ResolveRepositoryPackageRoots()
    // needs for choosing which package's *content* loads.
    return Roots;
}
}

const FGV2ScreenFieldAdapterRegistry& FGV2ScreenFieldAdapterRegistry::Get()
{
    static const FGV2ScreenFieldAdapterRegistry Registry;
    return Registry;
}

FGV2ScreenFieldAdapterRegistry::FGV2ScreenFieldAdapterRegistry()
    : SchemaCache(DiscoverDefaultSchemaPackageRoots())
{
}

bool FGV2ScreenFieldAdapterRegistry::IsKnownSchema(const std::string& SchemaId) const
{
    FString Error;
    return SchemaCache.GetCompiledSchema(SchemaId, Error) != nullptr;
}

bool FGV2ScreenFieldAdapterRegistry::PrepareBindingDefinitions(
    const GV2RuntimeCore::FScreenRequest& Request,
    TArray<FGV2UiBindingDefinition>& OutDefinitions) const
{
    OutDefinitions.Reset();
    for (const GV2RuntimeCore::FScreenField& Field : Request.Fields)
    {
        FString SchemaError;
        GV2ContentCore::FCompiledUiFieldSpecPtr Schema = SchemaCache.GetCompiledSchema(Field.SchemaId, SchemaError);
        if (!Schema)
        {
            OutDefinitions.Reset();
            return false;
        }

        FWalkContext Ctx;
        Ctx.SchemaCache = &SchemaCache;
        Ctx.ScreenId = Request.ScreenId;
        Ctx.CollectDefinitions = &OutDefinitions;

        FString Error;
        if (!WalkFieldValue(
                Ctx,
                *Schema,
                Field.Value,
                {TEXT("route"), TEXT("main"), FieldIdOf(Field)},
                FieldIdOf(Field),
                FieldIdOf(Field),
                nullptr,
                Error))
        {
            OutDefinitions.Reset();
            return false;
        }
    }
    return true;
}

bool FGV2ScreenFieldAdapterRegistry::BuildFields(
    const GV2RuntimeCore::FScreenRequest& Request,
    const TArray<FGV2UiBindingHandle>& Handles,
    TArray<FGV2ScreenFieldValue>& OutFields) const
{
    OutFields.Reset();
    OutFields.Reserve(static_cast<int32>(Request.Fields.size()));
    int32 HandleCursor = 0;

    for (const GV2RuntimeCore::FScreenField& Field : Request.Fields)
    {
        FString SchemaError;
        GV2ContentCore::FCompiledUiFieldSpecPtr Schema = SchemaCache.GetCompiledSchema(Field.SchemaId, SchemaError);
        if (!Schema)
        {
            OutFields.Reset();
            return false;
        }

        FWalkContext Ctx;
        Ctx.SchemaCache = &SchemaCache;
        Ctx.ScreenId = Request.ScreenId;
        Ctx.Handles = &Handles;
        Ctx.HandleCursor = &HandleCursor;

        FGV2PreparedUiValue Materialized;
        FString Error;
        if (!WalkFieldValue(
                Ctx,
                *Schema,
                Field.Value,
                {TEXT("route"), TEXT("main"), FieldIdOf(Field)},
                FieldIdOf(Field),
                FieldIdOf(Field),
                &Materialized,
                Error)
            || !Materialized.IsObject())
        {
            OutFields.Reset();
            return false;
        }

        FGV2ScreenFieldValue& OutField = OutFields.AddDefaulted_GetRef();
        OutField.FieldId = FName(FieldIdOf(Field));
        OutField.SchemaId = UTF8_TO_TCHAR(Field.SchemaId.c_str());
        OutField.PreparedValue = Materialized.AsObjectRef();
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
