#include "Application/GV2ScreenFieldAdapterRegistry.h"

#include "GV2RuntimeCore/GV2StableId.h"
#include "UI/GV2TextPipeline.h"

#include <set>

namespace
{
using FObject = GV2RuntimeCore::FValue::FObject;
using FArray = GV2RuntimeCore::FValue::FArray;

constexpr std::string_view LocationSceneSchema = "textsystem:schema.ui_field.location_scene.v1";
constexpr std::string_view LocationCommandsSchema = "textsystem:schema.ui_field.location_commands.v1";

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

const bool* FindBoolean(const FObject& Object, const std::string_view Name)
{
    const GV2RuntimeCore::FValue* Value = FindValue(Object, Name);
    return Value != nullptr ? std::get_if<bool>(&Value->Data) : nullptr;
}

bool CheckClosedKeys(
    const std::string& FieldId,
    const FObject& Value,
    const std::initializer_list<std::string_view>& ConsumedKeys,
    const std::string& SubContext = {})
{
    for (const auto& [Key, Val] : Value)
    {
        bool bKnown = false;
        for (const std::string_view ConsumedKey : ConsumedKeys)
        {
            if (Key == ConsumedKey)
            {
                bKnown = true;
                break;
            }
        }
        if (!bKnown)
        {
            if (SubContext.empty())
            {
                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT("ScreenField [%s]: unknown key '%s' rejected (closed schema)"),
                    UTF8_TO_TCHAR(FieldId.c_str()),
                    UTF8_TO_TCHAR(Key.c_str()));
            }
            else
            {
                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT("ScreenField [%s]: unknown key '%s' in %s rejected (closed schema)"),
                    UTF8_TO_TCHAR(FieldId.c_str()),
                    UTF8_TO_TCHAR(Key.c_str()),
                    UTF8_TO_TCHAR(SubContext.c_str()));
            }
            return false;
        }
    }
    return true;
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
    static constexpr std::initializer_list<std::string_view> ConsumedKeys = {"text_id", "style", "args"};
    if (!CheckClosedKeys("TextSpec", *Object, ConsumedKeys)) return false;
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

bool IsValidRepeatedElementKey(const std::string& Key)
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
    if (Key == nullptr || !IsValidRepeatedElementKey(*Key))
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
    static constexpr std::initializer_list<std::string_view> ConsumedKeys = {"command_id", "args"};
    if (!CheckClosedKeys("Binding", *Object, ConsumedKeys)) return false;
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

FString FieldId(const GV2RuntimeCore::FScreenField& Field)
{
    return UTF8_TO_TCHAR(Field.FieldId.c_str());
}

FString WidgetElementId(const std::string& ScreenId, const GV2RuntimeCore::FScreenField& Field)
{
    return FString::Printf(
        TEXT("%s#widget.%s"),
        UTF8_TO_TCHAR(ScreenId.c_str()),
        *FieldId(Field));
}

bool AddSingleBinding(
    const std::string& ScreenId,
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    const TCHAR* InputSchema,
    const FName InputName,
    const EGV2UiControlValueType InputType,
    TArray<FGV2UiBindingDefinition>& OutDefinitions)
{
    const GV2RuntimeCore::FValue* Binding = FindValue(Value, "binding");
    if (Binding == nullptr) return false;
    FGV2UiBindingDefinition& Definition = OutDefinitions.AddDefaulted_GetRef();
    const FString Id = FieldId(Field);
    if (!ReadBinding(
            *Binding,
            {TEXT("route"), TEXT("main"), Id},
            WidgetElementId(ScreenId, Field),
            Definition))
    {
        return false;
    }
    Definition.InputSchemaId = InputSchema;
    FGV2UiInputFieldDefinition& Input = Definition.InputFields.AddDefaulted_GetRef();
    Input.Name = InputName;
    Input.Type = InputType;
    Input.bRequired = true;
    return true;
}





// Schemas whose value is exactly {items: [{key, text, binding}], key?} -- a flat
// list of bindable buttons, the one shape simple enough to closed-validate here
// without a real schema lookup. Schemas without a legacy adapter that DON'T match
// this shape (location_player_status/scene/top_bar: several sibling collections
// with no uniform "items" meaning) are only scanned for bindings, not closed-
// validated -- there is no per-schema-ID knowledge of their shape to check
// against short of resolving the compiled schema, which is not wired into this
// path yet (UPP-27's job). This is a deliberate, narrow stopgap, not a general
// substitute for real schema-driven value validation.
bool IsFlatBindableItemsSchema(const std::string_view SchemaId)
{
    return SchemaId == "textsystem:schema.ui_field.location_commands.v1"
        || SchemaId == "core:schema.ui_field.button_list.v2";
}

// Collects binding definitions for a schema without a legacy adapter, by finding
// every 'binding' key at any depth of the field value: a top-level 'binding' (a
// single-binding field, e.g. Button-shaped) or a 'binding' key inside items of
// any keyed array (e.g. LocationCommands' items). For flat bindable-items schemas
// (see IsFlatBindableItemsSchema) it also rejects unknown keys at field-value and
// item level, preserving BAI-03 (REV3-03: a property nobody consumes must be
// rejected, not silently dropped) for the one shape this function actually knows.
// For every other schema it does not validate the value's overall shape or
// reject sibling keys it doesn't recognize -- location_player_status legitimately
// has meters/items/effects/name alongside each other, none of which carry a
// binding, and real schema/value validation for those happens later in the
// IGV2UiPropertyHost/consumer pipeline the widget itself runs during Apply.
bool ExtractGenericBindings(
    const std::string& ScreenId,
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    TArray<FGV2UiBindingDefinition>& OutDefinitions)
{
    const bool bClosedShape = IsFlatBindableItemsSchema(Field.SchemaId);
    if (bClosedShape)
    {
        static constexpr std::initializer_list<std::string_view> FieldConsumedKeys = {"items", "key"};
        if (!CheckClosedKeys(Field.FieldId, Value, FieldConsumedKeys, "field value")) return false;
    }

    if (const GV2RuntimeCore::FValue* BindingVal = FindValue(Value, "binding"))
    {
        FGV2UiBindingDefinition& Definition = OutDefinitions.AddDefaulted_GetRef();
        if (!ReadBinding(
                *BindingVal,
                {TEXT("route"), TEXT("main"), FieldId(Field)},
                WidgetElementId(ScreenId, Field),
                Definition))
        {
            return false;
        }
    }

    for (const auto& [ChildKey, ChildValue] : Value)
    {
        if (ChildKey == "binding") continue;
        const FArray* Items = AsArray(ChildValue);
        if (Items == nullptr) continue;

        static constexpr std::initializer_list<std::string_view> ItemConsumedKeys = {"key", "text", "binding"};
        TSet<FName> SeenKeys;
        for (const GV2RuntimeCore::FValue& ItemValue : *Items)
        {
            const FObject* Item = AsObject(ItemValue);
            if (Item == nullptr) continue;
            if (bClosedShape && !CheckClosedKeys(Field.FieldId, *Item, ItemConsumedKeys, "items element")) return false;

            const GV2RuntimeCore::FValue* Binding = FindValue(*Item, "binding");
            if (Binding == nullptr) continue;

            const std::string* Key = FindString(*Item, "key");
            if (!ValidateRepeatedElementKey(Key, SeenKeys)) return false;

            FGV2UiBindingDefinition& Definition = OutDefinitions.AddDefaulted_GetRef();
            if (!ReadBinding(
                    *Binding,
                    {TEXT("route"), TEXT("main"), FieldId(Field), UTF8_TO_TCHAR(Key->c_str())},
                    FString::Printf(
                        TEXT("%s#widget.%s"),
                        UTF8_TO_TCHAR(ScreenId.c_str()),
                        UTF8_TO_TCHAR(Key->c_str())),
                    Definition))
            {
                return false;
            }
        }
    }
    return true;
}
}

const FGV2ScreenFieldAdapterRegistry& FGV2ScreenFieldAdapterRegistry::Get()
{
    static const FGV2ScreenFieldAdapterRegistry Registry;
    return Registry;
}

FGV2ScreenFieldAdapterRegistry::FGV2ScreenFieldAdapterRegistry()
    : Adapters({})
{
}

const FGV2ScreenFieldAdapterRegistry::FAdapter* FGV2ScreenFieldAdapterRegistry::Find(
    const std::string_view SchemaId) const
{
    return Adapters.FindByPredicate(
        [SchemaId](const FAdapter& Adapter) { return Adapter.SchemaId == SchemaId; });
}

bool FGV2ScreenFieldAdapterRegistry::IsKnownSchema(const std::string_view SchemaId) const
{
    if (Find(SchemaId) != nullptr)
    {
        return true;
    }
    if (SchemaId == "textsystem:schema.ui_field.location_top_bar.v1"
        || SchemaId == "textsystem:schema.ui_field.location_player_status.v1"
        || SchemaId == "textsystem:schema.ui_field.location_scene.v1"
        || SchemaId == "textsystem:schema.ui_field.location_commands.v1"
        || SchemaId == "core:schema.ui_field.button_list.v2"
        || SchemaId == "core:schema.ui_field.button.v1"
        || SchemaId == "core:schema.ui_field.checkbox.v1"
        || SchemaId == "core:schema.ui_field.dropdown_select.v1"
        || SchemaId == "core:schema.ui_field.image.v1"
        || SchemaId == "core:schema.ui_field.input_field.v1"
        || SchemaId == "core:schema.ui_field.modal.v1"
        || SchemaId == "core:schema.ui_field.portrait.v1"
        || SchemaId == "core:schema.ui_field.progress_bar.v1"
        || SchemaId == "core:schema.ui_field.rich_text.v3"
        || SchemaId == "core:schema.ui_field.tab_container.v1"
        || SchemaId == "core:schema.ui_field.text.v1")
    {
        return true;
    }
    return false;
}

bool FGV2ScreenFieldAdapterRegistry::PrepareBindingDefinitions(
    const GV2RuntimeCore::FScreenRequest& Request,
    TArray<FGV2UiBindingDefinition>& OutDefinitions) const
{
    OutDefinitions.Reset();
    for (const GV2RuntimeCore::FScreenField& Field : Request.Fields)
    {
        const FObject* Value = AsObject(Field.Value);
        if (Value == nullptr || !IsKnownSchema(Field.SchemaId))
        {
            OutDefinitions.Reset();
            return false;
        }
        const FAdapter* Adapter = Find(Field.SchemaId);
        if (Adapter != nullptr)
        {
            if (!Adapter->PrepareBindings(Request.ScreenId, Field, *Value, OutDefinitions))
            {
                OutDefinitions.Reset();
                return false;
            }
        }
        else
        {
            if (!ExtractGenericBindings(Request.ScreenId, Field, *Value, OutDefinitions))
            {
                OutDefinitions.Reset();
                return false;
            }
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
    int32 HandleIndex = 0;
    for (const GV2RuntimeCore::FScreenField& Field : Request.Fields)
    {
        const FObject* Value = AsObject(Field.Value);
        if (Value == nullptr || !IsKnownSchema(Field.SchemaId))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("GV2 Screen Field build failed: screen='%s' field='%s' schema='%s' handles=%d index=%d"),
                UTF8_TO_TCHAR(Request.ScreenId.c_str()),
                UTF8_TO_TCHAR(Field.FieldId.c_str()),
                UTF8_TO_TCHAR(Field.SchemaId.c_str()),
                Handles.Num(),
                HandleIndex);
            OutFields.Reset();
            return false;
        }
        const FAdapter* Adapter = Find(Field.SchemaId);
        if (Adapter != nullptr)
        {
            FGV2ScreenFieldValue BuiltField;
            if (!Adapter->BuildField(Field, *Value, Handles, HandleIndex, BuiltField))
            {
                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT("GV2 Screen Field build failed: screen='%s' field='%s' schema='%s' handles=%d index=%d"),
                    UTF8_TO_TCHAR(Request.ScreenId.c_str()),
                    UTF8_TO_TCHAR(Field.FieldId.c_str()),
                    UTF8_TO_TCHAR(Field.SchemaId.c_str()),
                    Handles.Num(),
                    HandleIndex);
                OutFields.Reset();
                return false;
            }
            OutFields.Add(MoveTemp(BuiltField));
        }
    }
    return true;
}

int32 FGV2ScreenFieldAdapterRegistry::Num() const
{
    return Adapters.Num();
}
