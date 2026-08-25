#include "Application/GV2ScreenFieldAdapterRegistry.h"

#include "GV2RuntimeCore/GV2StableId.h"
#include "UI/GV2TextPipeline.h"

#include <set>

namespace
{
using FObject = GV2RuntimeCore::FValue::FObject;
using FArray = GV2RuntimeCore::FValue::FArray;

constexpr std::string_view LocationTopBarSchema = "textsystem:schema.ui_field.location_top_bar.v1";
constexpr std::string_view LocationPlayerStatusSchema = "textsystem:schema.ui_field.location_player_status.v1";
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

// Accepts both Lua float and Lua integer subtypes. `math.min(1, x)` yields an
// integer at full value, so rejecting integers here silently zeroes full meters.
bool ReadClampedPercent(const GV2RuntimeCore::FValue& Value, float& OutPercent)
{
    if (const double* Dbl = std::get_if<double>(&Value.Data))
    {
        if (!FMath::IsFinite(*Dbl)) return false;
        OutPercent = FMath::Clamp(static_cast<float>(*Dbl), 0.0f, 1.0f);
        return true;
    }
    if (const std::int64_t* Int = std::get_if<std::int64_t>(&Value.Data))
    {
        OutPercent = FMath::Clamp(static_cast<float>(*Int), 0.0f, 1.0f);
        return true;
    }
    return false;
}

bool ReadBinding(
    const GV2RuntimeCore::FValue& Value,
    const TArray<FString>& NodePath,
    const FString& ElementId,
    FGV2UiBindingDefinition& OutDefinition)
{
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





bool ReadOptionalResource(const FObject& Value, const std::string_view Name, FString& OutValue)
{
    if (const GV2RuntimeCore::FValue* Candidate = FindValue(Value, Name))
    {
        const std::string* ResourceId = std::get_if<std::string>(&Candidate->Data);
        if (ResourceId == nullptr || !GV2RuntimeCore::FStableId::IsOfKind(*ResourceId, "resource")) return false;
        OutValue = UTF8_TO_TCHAR(ResourceId->c_str());
    }
    return true;
}

bool PrepareLocationTopBar(const std::string&, const GV2RuntimeCore::FScreenField& Field, const FObject& Value, TArray<FGV2UiBindingDefinition>&)
{
    static constexpr std::initializer_list<std::string_view> ConsumedKeys = {"day", "location", "primary_resource"};
    if (!CheckClosedKeys(Field.FieldId, Value, ConsumedKeys)) return false;

    for (const char* Name : {"day", "location", "primary_resource"})
    {
        GV2RuntimeCore::FTextSpec Spec;
        const GV2RuntimeCore::FValue* Text = FindValue(Value, Name);
        if (Text == nullptr || !ReadTextSpec(*Text, Spec)) return false;
    }
    return true;
}

bool BuildLocationTopBar(const GV2RuntimeCore::FScreenField& Field, const FObject& Value, const TArray<FGV2UiBindingHandle>&, int32&, FGV2ScreenFieldValue& OutField)
{
    static constexpr std::initializer_list<std::string_view> ConsumedKeys = {"day", "location", "primary_resource"};
    if (!CheckClosedKeys(Field.FieldId, Value, ConsumedKeys)) return false;

    FGV2LocationTopBarViewModel Model;
    return ResolveText(*FindValue(Value, "day"), Model.Day)
        && ResolveText(*FindValue(Value, "location"), Model.Location)
        && ResolveText(*FindValue(Value, "primary_resource"), Model.PrimaryResource)
        && (OutField = FGV2ScreenFieldValue::MakeLocationTopBar(FName(*FieldId(Field)), Model), true);
}

// Keyed icon collections: identity comes from the owning entity, never from the
// array position, so reordering or swapping an icon does not reassign widgets.
bool PrepareLocationIconCollection(
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    const std::string& CollectionKey)
{
    const GV2RuntimeCore::FValue* CollectionVal = FindValue(Value, CollectionKey);
    if (CollectionVal == nullptr) return true;
    const FArray* CollectionArray = AsArray(*CollectionVal);
    if (CollectionArray == nullptr) return false;

    static constexpr std::initializer_list<std::string_view> EntryConsumedKeys = {"key", "resource_id"};
    const std::string SubContext = CollectionKey + " element";
    TSet<FName> SeenKeys;
    for (const GV2RuntimeCore::FValue& EntryVal : *CollectionArray)
    {
        const FObject* EntryObj = std::get_if<FObject>(&EntryVal.Data);
        if (EntryObj == nullptr || !CheckClosedKeys(Field.FieldId, *EntryObj, EntryConsumedKeys, SubContext)) return false;
        const GV2RuntimeCore::FValue* KeyVal = FindValue(*EntryObj, "key");
        if (KeyVal == nullptr) return false;
        if (!ValidateRepeatedElementKey(std::get_if<std::string>(&KeyVal->Data), SeenKeys)) return false;
        FString ResourceId;
        if (!ReadOptionalResource(*EntryObj, "resource_id", ResourceId)) return false;
    }
    return true;
}

bool BuildLocationIconCollection(
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    const std::string& CollectionKey,
    TArray<FGV2LocationIconEntry>& OutEntries)
{
    const GV2RuntimeCore::FValue* CollectionVal = FindValue(Value, CollectionKey);
    if (CollectionVal == nullptr) return true;
    const FArray* CollectionArray = AsArray(*CollectionVal);
    if (CollectionArray == nullptr) return false;

    static constexpr std::initializer_list<std::string_view> EntryConsumedKeys = {"key", "resource_id"};
    const std::string SubContext = CollectionKey + " element";
    TSet<FName> SeenKeys;
    for (const GV2RuntimeCore::FValue& EntryVal : *CollectionArray)
    {
        const FObject* EntryObj = std::get_if<FObject>(&EntryVal.Data);
        if (EntryObj == nullptr || !CheckClosedKeys(Field.FieldId, *EntryObj, EntryConsumedKeys, SubContext)) return false;
        const GV2RuntimeCore::FValue* KeyVal = FindValue(*EntryObj, "key");
        if (KeyVal == nullptr) return false;
        const std::string* KeyStr = std::get_if<std::string>(&KeyVal->Data);
        if (!ValidateRepeatedElementKey(KeyStr, SeenKeys)) return false;

        FGV2LocationIconEntry Entry;
        Entry.Key = FName(UTF8_TO_TCHAR(KeyStr->c_str()));
        if (!ReadOptionalResource(*EntryObj, "resource_id", Entry.ResourceId)) return false;
        OutEntries.Add(MoveTemp(Entry));
    }
    return true;
}

bool PrepareLocationPlayerStatus(const std::string&, const GV2RuntimeCore::FScreenField& Field, const FObject& Value, TArray<FGV2UiBindingDefinition>&)
{
    static constexpr std::initializer_list<std::string_view> ConsumedKeys = {
        "name", "portrait_resource_id", "meters", "items", "effects"
    };
    if (!CheckClosedKeys(Field.FieldId, Value, ConsumedKeys)) return false;

    const GV2RuntimeCore::FValue* Name = FindValue(Value, "name");
    GV2RuntimeCore::FTextSpec Spec;
    FString PortraitResourceId;
    if (Name == nullptr || !ReadTextSpec(*Name, Spec) || !ReadOptionalResource(Value, "portrait_resource_id", PortraitResourceId))
    {
        return false;
    }
    if (const GV2RuntimeCore::FValue* MetersVal = FindValue(Value, "meters"))
    {
        const FArray* MetersArray = AsArray(*MetersVal);
        if (MetersArray == nullptr) return false;
        static constexpr std::initializer_list<std::string_view> MeterConsumedKeys = {"key", "percent", "label"};
        TSet<FName> MeterKeys;
        for (const GV2RuntimeCore::FValue& EntryVal : *MetersArray)
        {
            const FObject* MeterObj = std::get_if<FObject>(&EntryVal.Data);
            if (MeterObj == nullptr || !CheckClosedKeys(Field.FieldId, *MeterObj, MeterConsumedKeys, "meters element")) return false;
            const GV2RuntimeCore::FValue* KeyVal = FindValue(*MeterObj, "key");
            if (KeyVal == nullptr) return false;
            if (!ValidateRepeatedElementKey(std::get_if<std::string>(&KeyVal->Data), MeterKeys)) return false;
            if (const GV2RuntimeCore::FValue* PercentVal = FindValue(*MeterObj, "percent"))
            {
                float Percent = 0.0f;
                if (!ReadClampedPercent(*PercentVal, Percent)) return false;
            }
            if (const GV2RuntimeCore::FValue* LabelVal = FindValue(*MeterObj, "label"))
            {
                GV2RuntimeCore::FTextSpec LabelSpec;
                if (!ReadTextSpec(*LabelVal, LabelSpec)) return false;
            }
        }
    }
    if (!PrepareLocationIconCollection(Field, Value, "items")) return false;
    if (!PrepareLocationIconCollection(Field, Value, "effects")) return false;
    return true;
}

bool BuildLocationPlayerStatus(const GV2RuntimeCore::FScreenField& Field, const FObject& Value, const TArray<FGV2UiBindingHandle>&, int32&, FGV2ScreenFieldValue& OutField)
{
    static constexpr std::initializer_list<std::string_view> ConsumedKeys = {
        "name", "portrait_resource_id", "meters", "items", "effects"
    };
    if (!CheckClosedKeys(Field.FieldId, Value, ConsumedKeys)) return false;

    FGV2LocationPlayerStatusViewModel Model;
    if (!ReadOptionalResource(Value, "portrait_resource_id", Model.PortraitResourceId)
        || !ResolveText(*FindValue(Value, "name"), Model.Name)) return false;

    if (const GV2RuntimeCore::FValue* MetersVal = FindValue(Value, "meters"))
    {
        const FArray* MetersArray = AsArray(*MetersVal);
        if (MetersArray == nullptr) return false;
        static constexpr std::initializer_list<std::string_view> MeterConsumedKeys = {"key", "percent", "label"};
        TSet<FName> MeterKeys;
        for (int32 Index = 0; Index < static_cast<int32>(MetersArray->size()); ++Index)
        {
            const GV2RuntimeCore::FValue& EntryVal = (*MetersArray)[Index];
            const FObject* MeterObj = std::get_if<FObject>(&EntryVal.Data);
            if (MeterObj == nullptr || !CheckClosedKeys(Field.FieldId, *MeterObj, MeterConsumedKeys, "meters element")) return false;

            FGV2LocationMeterEntry MeterEntry;
            const GV2RuntimeCore::FValue* KeyVal = FindValue(*MeterObj, "key");
            if (KeyVal == nullptr) return false;
            const std::string* KeyStr = std::get_if<std::string>(&KeyVal->Data);
            if (!ValidateRepeatedElementKey(KeyStr, MeterKeys)) return false;
            MeterEntry.Key = FName(UTF8_TO_TCHAR(KeyStr->c_str()));

            if (const GV2RuntimeCore::FValue* PercentVal = FindValue(*MeterObj, "percent"))
            {
                if (!ReadClampedPercent(*PercentVal, MeterEntry.Meter.Percent)) return false;
            }
            if (const GV2RuntimeCore::FValue* LabelVal = FindValue(*MeterObj, "label"))
            {
                if (!ResolveText(*LabelVal, MeterEntry.Meter.Label)) return false;
            }
            Model.Meters.Add(MeterEntry);
        }
    }

    if (!BuildLocationIconCollection(Field, Value, "items", Model.Items)) return false;
    if (!BuildLocationIconCollection(Field, Value, "effects", Model.Effects)) return false;

    OutField = FGV2ScreenFieldValue::MakeLocationPlayerStatus(FName(*FieldId(Field)), Model);
    return true;
}

bool PrepareLocationScene(const std::string&, const GV2RuntimeCore::FScreenField& Field, const FObject& Value, TArray<FGV2UiBindingDefinition>&)
{
    static constexpr std::initializer_list<std::string_view> ConsumedKeys = {"background_tile_resource_id", "background_resource_id", "context_text", "characters"};
    if (!CheckClosedKeys(Field.FieldId, Value, ConsumedKeys)) return false;

    FString Ignored;
    if (!ReadOptionalResource(Value, "background_tile_resource_id", Ignored)
        || !ReadOptionalResource(Value, "background_resource_id", Ignored)) return false;
    if (const GV2RuntimeCore::FValue* Context = FindValue(Value, "context_text")) { GV2RuntimeCore::FTextSpec Spec; if (!ReadTextSpec(*Context, Spec)) return false; }
    if (const GV2RuntimeCore::FValue* CharsVal = FindValue(Value, "characters"))
    {
        const FArray* CharsArray = AsArray(*CharsVal);
        if (CharsArray == nullptr) return false;
        static constexpr std::initializer_list<std::string_view> CharConsumedKeys = {"key", "resource_id"};
        TSet<FName> CharKeys;
        for (const GV2RuntimeCore::FValue& EntryVal : *CharsArray)
        {
            const FObject* CharObj = std::get_if<FObject>(&EntryVal.Data);
            if (CharObj == nullptr || !CheckClosedKeys(Field.FieldId, *CharObj, CharConsumedKeys, "characters element")) return false;
            const GV2RuntimeCore::FValue* KeyVal = FindValue(*CharObj, "key");
            if (KeyVal == nullptr) return false;
            if (!ValidateRepeatedElementKey(std::get_if<std::string>(&KeyVal->Data), CharKeys)) return false;
            if (!ReadOptionalResource(*CharObj, "resource_id", Ignored)) return false;
        }
    }
    return true;
}

bool BuildLocationScene(const GV2RuntimeCore::FScreenField& Field, const FObject& Value, const TArray<FGV2UiBindingHandle>&, int32&, FGV2ScreenFieldValue& OutField)
{
    static constexpr std::initializer_list<std::string_view> ConsumedKeys = {"background_tile_resource_id", "background_resource_id", "context_text", "characters"};
    if (!CheckClosedKeys(Field.FieldId, Value, ConsumedKeys)) return false;

    FGV2LocationSceneViewModel Model;
    if (!ReadOptionalResource(Value, "background_tile_resource_id", Model.BackgroundTileResourceId)
        || !ReadOptionalResource(Value, "background_resource_id", Model.BackgroundResourceId)) return false;
    if (const GV2RuntimeCore::FValue* Context = FindValue(Value, "context_text") ; Context != nullptr && !ResolveText(*Context, Model.ContextText)) return false;

    if (const GV2RuntimeCore::FValue* CharsVal = FindValue(Value, "characters"))
    {
        const FArray* CharsArray = AsArray(*CharsVal);
        if (CharsArray == nullptr) return false;
        static constexpr std::initializer_list<std::string_view> CharConsumedKeys = {"key", "resource_id"};
        TSet<FName> CharacterKeys;
        for (int32 Index = 0; Index < static_cast<int32>(CharsArray->size()); ++Index)
        {
            const GV2RuntimeCore::FValue& EntryVal = (*CharsArray)[Index];
            const FObject* CharObj = std::get_if<FObject>(&EntryVal.Data);
            if (CharObj == nullptr || !CheckClosedKeys(Field.FieldId, *CharObj, CharConsumedKeys, "characters element")) return false;

            FGV2LocationCharacterEntry CharEntry;
            const GV2RuntimeCore::FValue* KeyVal = FindValue(*CharObj, "key");
            if (KeyVal == nullptr) return false;
            const std::string* KeyStr = std::get_if<std::string>(&KeyVal->Data);
            if (!ValidateRepeatedElementKey(KeyStr, CharacterKeys)) return false;
            CharEntry.Key = FName(UTF8_TO_TCHAR(KeyStr->c_str()));

            if (!ReadOptionalResource(*CharObj, "resource_id", CharEntry.ResourceId)) return false;
            Model.Characters.Add(CharEntry);
        }
    }

    OutField = FGV2ScreenFieldValue::MakeLocationScene(FName(*FieldId(Field)), Model);
    return true;
}

bool PrepareLocationCommands(
    const std::string& ScreenId,
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    TArray<FGV2UiBindingDefinition>& Definitions)
{
    static constexpr std::initializer_list<std::string_view> ConsumedKeys = {"items"};
    if (!CheckClosedKeys(Field.FieldId, Value, ConsumedKeys)) return false;

    const GV2RuntimeCore::FValue* ItemsValue = FindValue(Value, "items");
    const FArray* Items = ItemsValue != nullptr ? AsArray(*ItemsValue) : nullptr;
    if (Items == nullptr) return false;
    static constexpr std::initializer_list<std::string_view> ItemConsumedKeys = {"key", "text", "binding"};
    TSet<FName> SeenKeys;
    for (const GV2RuntimeCore::FValue& ItemValue : *Items)
    {
        const FObject* Item = AsObject(ItemValue);
        if (Item == nullptr || !CheckClosedKeys(Field.FieldId, *Item, ItemConsumedKeys, "items element")) return false;
        const std::string* Key = FindString(*Item, "key");
        const GV2RuntimeCore::FValue* Binding = FindValue(*Item, "binding");
        if (!ValidateRepeatedElementKey(Key, SeenKeys) || Binding == nullptr) return false;
        FGV2UiBindingDefinition& Definition = Definitions.AddDefaulted_GetRef();
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
    return true;
}

bool BuildLocationCommands(
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    const TArray<FGV2UiBindingHandle>& Handles,
    int32& HandleIndex,
    FGV2ScreenFieldValue& OutField)
{
    static constexpr std::initializer_list<std::string_view> ConsumedKeys = {"items"};
    if (!CheckClosedKeys(Field.FieldId, Value, ConsumedKeys)) return false;

    const GV2RuntimeCore::FValue* ItemsVal = FindValue(Value, "items");
    const FArray* Items = ItemsVal != nullptr ? AsArray(*ItemsVal) : nullptr;
    if (Items == nullptr) return false;
    TArray<FGV2ButtonViewModel> Buttons;
    Buttons.Reserve(static_cast<int32>(Items->size()));
    static constexpr std::initializer_list<std::string_view> ItemConsumedKeys = {"key", "text", "binding"};
    for (const GV2RuntimeCore::FValue& ItemValue : *Items)
    {
        if (!Handles.IsValidIndex(HandleIndex)) return false;
        const FObject* Item = AsObject(ItemValue);
        if (Item == nullptr || !CheckClosedKeys(Field.FieldId, *Item, ItemConsumedKeys, "items element")) return false;
        const std::string* Key = FindString(*Item, "key");
        if (Key == nullptr) return false;
        const GV2RuntimeCore::FValue* Text = FindValue(*Item, "text");
        FGV2ButtonViewModel& Button = Buttons.AddDefaulted_GetRef();
        Button.Key = FName(UTF8_TO_TCHAR(Key->c_str()));
        if (Text == nullptr || !ResolveText(*Text, Button.Text)) return false;
        Button.Binding = Handles[HandleIndex++];
    }
    OutField = FGV2ScreenFieldValue::MakeLocationCommands(FName(*FieldId(Field)), Buttons);
    return true;
}
}

const FGV2ScreenFieldAdapterRegistry& FGV2ScreenFieldAdapterRegistry::Get()
{
    static const FGV2ScreenFieldAdapterRegistry Registry;
    return Registry;
}

FGV2ScreenFieldAdapterRegistry::FGV2ScreenFieldAdapterRegistry()
    : Adapters({
        {LocationTopBarSchema, &PrepareLocationTopBar, &BuildLocationTopBar},
        {LocationPlayerStatusSchema, &PrepareLocationPlayerStatus, &BuildLocationPlayerStatus},
        {LocationSceneSchema, &PrepareLocationScene, &BuildLocationScene},
        {LocationCommandsSchema, &PrepareLocationCommands, &BuildLocationCommands},
    })
{
    TSet<FString> SchemaIds;
    for (const FAdapter& Adapter : Adapters)
    {
        check(!Adapter.SchemaId.empty());
        check(Adapter.PrepareBindings != nullptr && Adapter.BuildField != nullptr);
        const FString SchemaId = UTF8_TO_TCHAR(Adapter.SchemaId.data());
        check(!SchemaIds.Contains(SchemaId));
        SchemaIds.Add(SchemaId);
    }
}

const FGV2ScreenFieldAdapterRegistry::FAdapter* FGV2ScreenFieldAdapterRegistry::Find(
    const std::string_view SchemaId) const
{
    return Adapters.FindByPredicate(
        [SchemaId](const FAdapter& Adapter) { return Adapter.SchemaId == SchemaId; });
}

bool FGV2ScreenFieldAdapterRegistry::PrepareBindingDefinitions(
    const GV2RuntimeCore::FScreenRequest& Request,
    TArray<FGV2UiBindingDefinition>& OutDefinitions) const
{
    OutDefinitions.Reset();
    for (const GV2RuntimeCore::FScreenField& Field : Request.Fields)
    {
        const FObject* Value = AsObject(Field.Value);
        const FAdapter* Adapter = Find(Field.SchemaId);
        if (Value == nullptr || Adapter == nullptr
            || !Adapter->PrepareBindings(Request.ScreenId, Field, *Value, OutDefinitions))
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
    int32 HandleIndex = 0;
    for (const GV2RuntimeCore::FScreenField& Field : Request.Fields)
    {
        const FObject* Value = AsObject(Field.Value);
        const FAdapter* Adapter = Find(Field.SchemaId);
        FGV2ScreenFieldValue BuiltField;
        if (Value == nullptr || Adapter == nullptr
            || !Adapter->BuildField(Field, *Value, Handles, HandleIndex, BuiltField))
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
    if (HandleIndex != Handles.Num())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("GV2 Screen Field build left unused handles: screen='%s' handles=%d index=%d"),
            UTF8_TO_TCHAR(Request.ScreenId.c_str()),
            Handles.Num(),
            HandleIndex);
        OutFields.Reset();
        return false;
    }
    return true;
}

int32 FGV2ScreenFieldAdapterRegistry::Num() const
{
    return Adapters.Num();
}
