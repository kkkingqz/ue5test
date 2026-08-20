#include "Application/GV2ScreenFieldAdapterRegistry.h"

#include "GV2RuntimeCore/GV2StableId.h"
#include "UI/GV2TextPipeline.h"

namespace
{
using FObject = GV2RuntimeCore::FValue::FObject;
using FArray = GV2RuntimeCore::FValue::FArray;

constexpr std::string_view ButtonListSchema = "core:schema.ui_field.button_list.v2";
constexpr std::string_view RichTextSchema = "core:schema.ui_field.rich_text.v3";
constexpr std::string_view CheckboxSchema = "core:schema.ui_field.checkbox.v1";
constexpr std::string_view InputFieldSchema = "core:schema.ui_field.input_field.v1";
constexpr std::string_view DropdownSelectSchema = "core:schema.ui_field.dropdown_select.v1";
constexpr TCHAR CheckboxInputSchema[] = TEXT("core:schema.ui_input.checkbox_changed.v1");
constexpr TCHAR InputFieldInputSchema[] = TEXT("core:schema.ui_input.text_changed.v1");
constexpr TCHAR DropdownSelectInputSchema[] = TEXT("core:schema.ui_input.dropdown_selected.v1");

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
    const std::string* TextId = Object != nullptr ? FindString(*Object, "text_id") : nullptr;
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
    const FObject* Object = AsObject(Value);
    const std::string* CommandId = Object != nullptr ? FindString(*Object, "command_id") : nullptr;
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

bool PrepareButtonList(
    const std::string& ScreenId,
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    TArray<FGV2UiBindingDefinition>& OutDefinitions)
{
    const GV2RuntimeCore::FValue* ItemsValue = FindValue(Value, "items");
    const FArray* Items = ItemsValue != nullptr ? AsArray(*ItemsValue) : nullptr;
    if (Items == nullptr) return false;
    TSet<FName> SeenKeys;
    for (const GV2RuntimeCore::FValue& ItemValue : *Items)
    {
        const FObject* Item = AsObject(ItemValue);
        const std::string* Key = Item != nullptr ? FindString(*Item, "key") : nullptr;
        const GV2RuntimeCore::FValue* Binding = Item != nullptr ? FindValue(*Item, "binding") : nullptr;
        if (!ValidateRepeatedElementKey(Key, SeenKeys) || Binding == nullptr) return false;
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
    return true;
}

bool BuildButtonList(
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    const TArray<FGV2UiBindingHandle>& Handles,
    int32& HandleIndex,
    FGV2ScreenFieldValue& OutField)
{
    const FArray* Items = AsArray(*FindValue(Value, "items"));
    TArray<FGV2ButtonViewModel> Buttons;
    Buttons.Reserve(static_cast<int32>(Items->size()));
    for (const GV2RuntimeCore::FValue& ItemValue : *Items)
    {
        if (!Handles.IsValidIndex(HandleIndex)) return false;
        const FObject* Item = AsObject(ItemValue);
        if (Item == nullptr) return false;
        const std::string* Key = FindString(*Item, "key");
        if (Key == nullptr) return false;
        const GV2RuntimeCore::FValue* Text = FindValue(*Item, "text");
        FGV2ButtonViewModel& Button = Buttons.AddDefaulted_GetRef();
        Button.Key = FName(UTF8_TO_TCHAR(Key->c_str()));
        if (Text == nullptr || !ResolveText(*Text, Button.Text)) return false;
        Button.Binding = Handles[HandleIndex++];
    }
    OutField = FGV2ScreenFieldValue::MakeButtonList(FName(*FieldId(Field)), Buttons);
    return true;
}

bool PrepareRichText(
    const std::string& ScreenId,
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    TArray<FGV2UiBindingDefinition>& OutDefinitions)
{
    const GV2RuntimeCore::FValue* SpansValue = FindValue(Value, "spans");
    const FArray* Spans = SpansValue != nullptr ? AsArray(*SpansValue) : nullptr;
    if (Spans == nullptr) return false;
    TSet<FName> SeenKeys;
    for (const GV2RuntimeCore::FValue& SpanValue : *Spans)
    {
        const FObject* Span = AsObject(SpanValue);
        const std::string* Key = Span != nullptr ? FindString(*Span, "key") : nullptr;
        const GV2RuntimeCore::FValue* Binding = Span != nullptr ? FindValue(*Span, "binding") : nullptr;
        if (!ValidateRepeatedElementKey(Key, SeenKeys)) return false;
        if (Binding != nullptr)
        {
            FGV2UiBindingDefinition& Definition = OutDefinitions.AddDefaulted_GetRef();
            if (!ReadBinding(
                    *Binding,
                    {TEXT("route"), TEXT("main"), FieldId(Field), UTF8_TO_TCHAR(Key->c_str())},
                    FString::Printf(
                        TEXT("%s#span.%s"),
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

bool BuildRichText(
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    const TArray<FGV2UiBindingHandle>& Handles,
    int32& HandleIndex,
    FGV2ScreenFieldValue& OutField)
{
    const GV2RuntimeCore::FValue* Text = FindValue(Value, "text");
    const FArray* Spans = AsArray(*FindValue(Value, "spans"));
    FGV2InteractiveRichTextViewModel Model;
    if (Text == nullptr || !ResolveText(*Text, Model.Text)) return false;
    for (const GV2RuntimeCore::FValue& SpanValue : *Spans)
    {
        const FObject& Span = *AsObject(SpanValue);
        const std::string& Key = *FindString(Span, "key");
        const std::string* SpanId = FindString(Span, "span_id");
        FGV2RichTextSpanViewModel& SpanModel = Model.Spans.AddDefaulted_GetRef();
        SpanModel.Key = FName(UTF8_TO_TCHAR(Key.c_str()));
        SpanModel.SpanId = FName(UTF8_TO_TCHAR((SpanId != nullptr ? *SpanId : Key).c_str()));
        if (const GV2RuntimeCore::FValue* HoverValue = FindValue(Span, "hover"))
        {
            const FObject* Hover = AsObject(*HoverValue);
            if (Hover == nullptr) return false;
            if (const GV2RuntimeCore::FValue* Title = FindValue(*Hover, "title"))
            {
                if (!ResolveText(*Title, SpanModel.Hover.Title)) return false;
            }
            if (const GV2RuntimeCore::FValue* Description = FindValue(*Hover, "description"))
            {
                if (!ResolveText(*Description, SpanModel.Hover.Description)) return false;
            }
            if (const std::string* Image = FindString(*Hover, "image_resource_id"))
            {
                if (!GV2RuntimeCore::FStableId::IsOfKind(*Image, "resource")) return false;
                SpanModel.Hover.ImageResourceId = UTF8_TO_TCHAR(Image->c_str());
            }
        }
        if (FindValue(Span, "binding") != nullptr)
        {
            if (!Handles.IsValidIndex(HandleIndex)) return false;
            SpanModel.Binding = Handles[HandleIndex++];
        }
    }
    OutField = FGV2ScreenFieldValue::MakeInteractiveRichText(FName(*FieldId(Field)), Model);
    return true;
}

bool PrepareCheckbox(
    const std::string& ScreenId,
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    TArray<FGV2UiBindingDefinition>& OutDefinitions)
{
    GV2RuntimeCore::FTextSpec TextSpec;
    const GV2RuntimeCore::FValue* Text = FindValue(Value, "text");
    if (Text == nullptr || !ReadTextSpec(*Text, TextSpec)
        || FindBoolean(Value, "is_checked") == nullptr)
    {
        return false;
    }
    return AddSingleBinding(
        ScreenId,
        Field,
        Value,
        CheckboxInputSchema,
        TEXT("is_checked"),
        EGV2UiControlValueType::Boolean,
        OutDefinitions);
}

bool BuildCheckbox(
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    const TArray<FGV2UiBindingHandle>& Handles,
    int32& HandleIndex,
    FGV2ScreenFieldValue& OutField)
{
    if (!Handles.IsValidIndex(HandleIndex)) return false;
    const GV2RuntimeCore::FValue* Text = FindValue(Value, "text");
    const bool* IsChecked = FindBoolean(Value, "is_checked");
    FGV2CheckboxViewModel Model;
    Model.Key = FName(*FieldId(Field));
    if (Text == nullptr || IsChecked == nullptr || !ResolveText(*Text, Model.Text)) return false;
    Model.bIsChecked = *IsChecked;
    Model.Binding = Handles[HandleIndex++];
    OutField = FGV2ScreenFieldValue::MakeCheckbox(Model.Key, Model);
    return true;
}

bool PrepareInputField(
    const std::string& ScreenId,
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    TArray<FGV2UiBindingDefinition>& OutDefinitions)
{
    if (const GV2RuntimeCore::FValue* Text = FindValue(Value, "text"))
    {
        GV2RuntimeCore::FTextSpec Spec;
        if (!ReadTextSpec(*Text, Spec)) return false;
    }
    if (const GV2RuntimeCore::FValue* Placeholder = FindValue(Value, "placeholder_text"))
    {
        GV2RuntimeCore::FTextSpec Spec;
        if (!ReadTextSpec(*Placeholder, Spec)) return false;
    }
    if (const GV2RuntimeCore::FValue* TextValue = FindValue(Value, "value"))
    {
        if (std::get_if<std::string>(&TextValue->Data) == nullptr) return false;
    }
    return AddSingleBinding(
        ScreenId,
        Field,
        Value,
        InputFieldInputSchema,
        TEXT("value"),
        EGV2UiControlValueType::String,
        OutDefinitions);
}

bool BuildInputField(
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    const TArray<FGV2UiBindingHandle>& Handles,
    int32& HandleIndex,
    FGV2ScreenFieldValue& OutField)
{
    if (!Handles.IsValidIndex(HandleIndex)) return false;
    FGV2InputFieldViewModel Model;
    Model.Key = FName(*FieldId(Field));
    if (const GV2RuntimeCore::FValue* TextValue = FindValue(Value, "value"))
    {
        Model.TextValue = UTF8_TO_TCHAR(std::get<std::string>(TextValue->Data).c_str());
    }
    if (const GV2RuntimeCore::FValue* Placeholder = FindValue(Value, "placeholder_text"))
    {
        if (!ResolveText(*Placeholder, Model.PlaceholderText)) return false;
    }
    if (const GV2RuntimeCore::FValue* Text = FindValue(Value, "text"))
    {
        if (!ResolveText(*Text, Model.Text)) return false;
    }
    Model.Binding = Handles[HandleIndex++];
    OutField = FGV2ScreenFieldValue::MakeInputField(Model.Key, Model);
    return true;
}

bool ValidateDropdownOptions(const FObject& Value)
{
    const GV2RuntimeCore::FValue* SelectedValue = FindValue(Value, "selected_key");
    const std::string* SelectedKey = SelectedValue != nullptr
        ? std::get_if<std::string>(&SelectedValue->Data)
        : nullptr;
    if (SelectedValue != nullptr
        && (SelectedKey == nullptr || !IsValidRepeatedElementKey(*SelectedKey)))
    {
        return false;
    }
    const GV2RuntimeCore::FValue* ItemsValue = FindValue(Value, "items");
    const FArray* Items = ItemsValue != nullptr ? AsArray(*ItemsValue) : nullptr;
    if (Items == nullptr) return false;
    TSet<FName> Keys;
    bool SelectedFound = SelectedKey == nullptr;
    for (const GV2RuntimeCore::FValue& ItemValue : *Items)
    {
        const FObject* Item = AsObject(ItemValue);
        const std::string* Key = Item != nullptr ? FindString(*Item, "key") : nullptr;
        const GV2RuntimeCore::FValue* Text = Item != nullptr ? FindValue(*Item, "text") : nullptr;
        GV2RuntimeCore::FTextSpec TextSpec;
        if (!ValidateRepeatedElementKey(Key, Keys) || Text == nullptr
            || !ReadTextSpec(*Text, TextSpec))
        {
            return false;
        }
        SelectedFound = SelectedFound || (SelectedKey != nullptr && *SelectedKey == *Key);
    }
    return SelectedFound;
}

bool PrepareDropdown(
    const std::string& ScreenId,
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    TArray<FGV2UiBindingDefinition>& OutDefinitions)
{
    if (const GV2RuntimeCore::FValue* Placeholder = FindValue(Value, "placeholder"))
    {
        GV2RuntimeCore::FTextSpec Spec;
        if (!ReadTextSpec(*Placeholder, Spec)) return false;
    }
    if (!ValidateDropdownOptions(Value)) return false;
    return AddSingleBinding(
        ScreenId,
        Field,
        Value,
        DropdownSelectInputSchema,
        TEXT("selected_key"),
        EGV2UiControlValueType::String,
        OutDefinitions);
}

bool BuildDropdown(
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    const TArray<FGV2UiBindingHandle>& Handles,
    int32& HandleIndex,
    FGV2ScreenFieldValue& OutField)
{
    if (!Handles.IsValidIndex(HandleIndex)) return false;
    FGV2DropdownSelectViewModel Model;
    if (const GV2RuntimeCore::FValue* Placeholder = FindValue(Value, "placeholder"))
    {
        if (!ResolveText(*Placeholder, Model.Placeholder)) return false;
    }
    const GV2RuntimeCore::FValue* SelectedValue = FindValue(Value, "selected_key");
    const std::string* SelectedKey = SelectedValue != nullptr
        ? std::get_if<std::string>(&SelectedValue->Data)
        : nullptr;
    const FArray& Items = *AsArray(*FindValue(Value, "items"));
    for (const GV2RuntimeCore::FValue& ItemValue : Items)
    {
        const FObject* Item = AsObject(ItemValue);
        if (Item == nullptr) return false;
        const std::string* Key = FindString(*Item, "key");
        if (Key == nullptr) return false;
        const GV2RuntimeCore::FValue* Text = FindValue(*Item, "text");
        FGV2DropdownOptionViewModel& Option = Model.Options.AddDefaulted_GetRef();
        Option.Key = FName(UTF8_TO_TCHAR(Key->c_str()));
        if (Text == nullptr || !ResolveText(*Text, Option.Text)) return false;
        Option.bSelected = SelectedKey != nullptr && *SelectedKey == *Key;
    }
    Model.Binding = Handles[HandleIndex++];
    OutField = FGV2ScreenFieldValue::MakeDropdownSelect(FName(*FieldId(Field)), Model);
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
        {ButtonListSchema, &PrepareButtonList, &BuildButtonList},
        {RichTextSchema, &PrepareRichText, &BuildRichText},
        {CheckboxSchema, &PrepareCheckbox, &BuildCheckbox},
        {InputFieldSchema, &PrepareInputField, &BuildInputField},
        {DropdownSelectSchema, &PrepareDropdown, &BuildDropdown},
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
            OutFields.Reset();
            return false;
        }
        OutFields.Add(MoveTemp(BuiltField));
    }
    if (HandleIndex != Handles.Num())
    {
        OutFields.Reset();
        return false;
    }
    return true;
}

int32 FGV2ScreenFieldAdapterRegistry::Num() const
{
    return Adapters.Num();
}
