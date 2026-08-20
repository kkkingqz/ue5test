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
constexpr std::string_view ImageSchema = "core:schema.ui_field.image.v1";
constexpr std::string_view ProgressBarSchema = "core:schema.ui_field.progress_bar.v1";
constexpr std::string_view PortraitSchema = "core:schema.ui_field.portrait.v1";
constexpr std::string_view ModalSchema = "core:schema.ui_field.modal.v1";
constexpr std::string_view TabContainerSchema = "core:schema.ui_field.tab_container.v1";
constexpr std::string_view LocationTopBarSchema = "textsystem:schema.ui_field.location_top_bar.v1";
constexpr std::string_view LocationPlayerStatusSchema = "textsystem:schema.ui_field.location_player_status.v1";
constexpr std::string_view LocationSceneSchema = "textsystem:schema.ui_field.location_scene.v1";
constexpr std::string_view LocationCommandsSchema = "textsystem:schema.ui_field.location_commands.v1";
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

bool PrepareImage(
    const std::string& ScreenId,
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    TArray<FGV2UiBindingDefinition>& OutDefinitions)
{
    const std::string* ResourceId = FindString(Value, "resource_id");
    if (ResourceId == nullptr || ResourceId->empty()
        || !GV2RuntimeCore::FStableId::IsOfKind(*ResourceId, "resource"))
    {
        return false;
    }
    return true;
}

bool BuildImageField(
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    const TArray<FGV2UiBindingHandle>& Handles,
    int32& HandleIndex,
    FGV2ScreenFieldValue& OutField)
{
    const std::string* ResourceId = FindString(Value, "resource_id");
    if (ResourceId == nullptr) return false;
    FGV2ImageFieldViewModel Model;
    Model.ResourceId = UTF8_TO_TCHAR(ResourceId->c_str());
    OutField = FGV2ScreenFieldValue::MakeImage(FName(*FieldId(Field)), Model);
    return true;
}

bool PrepareProgressBar(
    const std::string& ScreenId,
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    TArray<FGV2UiBindingDefinition>& OutDefinitions)
{
    const GV2RuntimeCore::FValue* PercentVal = FindValue(Value, "percent");
    if (PercentVal == nullptr) return false;
    double Percent = 0.0;
    if (const double* Dbl = std::get_if<double>(&PercentVal->Data))
    {
        Percent = *Dbl;
    }
    else if (const std::int64_t* Int = std::get_if<std::int64_t>(&PercentVal->Data))
    {
        Percent = static_cast<double>(*Int);
    }
    else
    {
        return false;
    }
    if (!FMath::IsFinite(Percent) || Percent < 0.0 || Percent > 1.0) return false;

    if (const GV2RuntimeCore::FValue* LabelVal = FindValue(Value, "label"))
    {
        GV2RuntimeCore::FTextSpec LabelSpec;
        if (!ReadTextSpec(*LabelVal, LabelSpec)) return false;
    }
    return true;
}

bool BuildProgressBarField(
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    const TArray<FGV2UiBindingHandle>& Handles,
    int32& HandleIndex,
    FGV2ScreenFieldValue& OutField)
{
    const GV2RuntimeCore::FValue* PercentVal = FindValue(Value, "percent");
    if (PercentVal == nullptr) return false;
    float Percent = 0.0f;
    if (const double* Dbl = std::get_if<double>(&PercentVal->Data))
    {
        Percent = static_cast<float>(*Dbl);
    }
    else if (const std::int64_t* Int = std::get_if<std::int64_t>(&PercentVal->Data))
    {
        Percent = static_cast<float>(*Int);
    }
    else
    {
        return false;
    }

    FGV2ProgressBarViewModel Model;
    Model.Percent = FMath::Clamp(Percent, 0.0f, 1.0f);
    if (const GV2RuntimeCore::FValue* LabelVal = FindValue(Value, "label"))
    {
        if (!ResolveText(*LabelVal, Model.Label)) return false;
    }
    OutField = FGV2ScreenFieldValue::MakeProgressBar(FName(*FieldId(Field)), Model);
    return true;
}

bool PreparePortrait(
    const std::string& ScreenId,
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    TArray<FGV2UiBindingDefinition>& OutDefinitions)
{
    const std::string* ResourceId = FindString(Value, "resource_id");
    if (ResourceId == nullptr || ResourceId->empty()
        || !GV2RuntimeCore::FStableId::IsOfKind(*ResourceId, "resource"))
    {
        return false;
    }
    if (const std::string* FrameId = FindString(Value, "frame_resource_id"))
    {
        if (!FrameId->empty() && !GV2RuntimeCore::FStableId::IsOfKind(*FrameId, "resource"))
        {
            return false;
        }
    }
    return true;
}

bool BuildPortraitField(
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    const TArray<FGV2UiBindingHandle>& Handles,
    int32& HandleIndex,
    FGV2ScreenFieldValue& OutField)
{
    const std::string* ResourceId = FindString(Value, "resource_id");
    if (ResourceId == nullptr) return false;
    FGV2PortraitViewModel Model;
    Model.ResourceId = UTF8_TO_TCHAR(ResourceId->c_str());
    if (const std::string* FrameId = FindString(Value, "frame_resource_id"))
    {
        Model.FrameResourceId = UTF8_TO_TCHAR(FrameId->c_str());
    }
    OutField = FGV2ScreenFieldValue::MakePortrait(FName(*FieldId(Field)), Model);
    return true;
}

bool PrepareModal(
    const std::string& ScreenId,
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    TArray<FGV2UiBindingDefinition>& OutDefinitions)
{
    const GV2RuntimeCore::FValue* TitleVal = FindValue(Value, "title");
    const GV2RuntimeCore::FValue* ContentVal = FindValue(Value, "content");
    if (TitleVal == nullptr || ContentVal == nullptr) return false;
    GV2RuntimeCore::FTextSpec TitleSpec, ContentSpec;
    if (!ReadTextSpec(*TitleVal, TitleSpec) || !ReadTextSpec(*ContentVal, ContentSpec)) return false;

    if (const GV2RuntimeCore::FValue* ButtonsVal = FindValue(Value, "buttons"))
    {
        const FArray* Buttons = AsArray(*ButtonsVal);
        if (Buttons == nullptr) return false;
        TSet<FName> SeenKeys;
        for (const GV2RuntimeCore::FValue& BtnVal : *Buttons)
        {
            const FObject* BtnObj = AsObject(BtnVal);
            const std::string* Key = BtnObj != nullptr ? FindString(*BtnObj, "key") : nullptr;
            const GV2RuntimeCore::FValue* Binding = BtnObj != nullptr ? FindValue(*BtnObj, "binding") : nullptr;
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
    }

    if (const GV2RuntimeCore::FValue* BackdropBinding = FindValue(Value, "backdrop_close_action"))
    {
        FGV2UiBindingDefinition& Definition = OutDefinitions.AddDefaulted_GetRef();
        if (!ReadBinding(
                *BackdropBinding,
                {TEXT("route"), TEXT("main"), FieldId(Field), TEXT("backdrop")},
                FString::Printf(
                    TEXT("%s#widget.backdrop"),
                    UTF8_TO_TCHAR(ScreenId.c_str())),
                Definition))
        {
            return false;
        }
    }

    return true;
}

bool BuildModalField(
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    const TArray<FGV2UiBindingHandle>& Handles,
    int32& HandleIndex,
    FGV2ScreenFieldValue& OutField)
{
    const GV2RuntimeCore::FValue* TitleVal = FindValue(Value, "title");
    const GV2RuntimeCore::FValue* ContentVal = FindValue(Value, "content");
    if (TitleVal == nullptr || ContentVal == nullptr) return false;
    FGV2ModalViewModel Model;
    if (!ResolveText(*TitleVal, Model.Title) || !ResolveText(*ContentVal, Model.Content)) return false;

    if (const GV2RuntimeCore::FValue* ButtonsVal = FindValue(Value, "buttons"))
    {
        const FArray* Buttons = AsArray(*ButtonsVal);
        if (Buttons != nullptr)
        {
            Model.Buttons.Reserve(static_cast<int32>(Buttons->size()));
            for (const GV2RuntimeCore::FValue& BtnVal : *Buttons)
            {
                if (!Handles.IsValidIndex(HandleIndex)) return false;
                const FObject* BtnObj = AsObject(BtnVal);
                if (BtnObj == nullptr) return false;
                const std::string* Key = FindString(*BtnObj, "key");
                const GV2RuntimeCore::FValue* Text = FindValue(*BtnObj, "text");
                if (Key == nullptr || Text == nullptr) return false;
                FGV2ButtonViewModel& Btn = Model.Buttons.AddDefaulted_GetRef();
                Btn.Key = FName(UTF8_TO_TCHAR(Key->c_str()));
                if (!ResolveText(*Text, Btn.Text)) return false;
                Btn.Binding = Handles[HandleIndex++];
            }
        }
    }

    if (FindValue(Value, "backdrop_close_action") != nullptr)
    {
        if (!Handles.IsValidIndex(HandleIndex)) return false;
        Model.BackdropCloseBinding = Handles[HandleIndex++];
    }

    OutField = FGV2ScreenFieldValue::MakeModal(FName(*FieldId(Field)), Model);
    return true;
}

bool PrepareTabContainer(
    const std::string& ScreenId,
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    TArray<FGV2UiBindingDefinition>& OutDefinitions)
{
    const FArray* TabsArray = nullptr;
    if (const GV2RuntimeCore::FValue* TabsVal = FindValue(Value, "tabs"))
    {
        TabsArray = AsArray(*TabsVal);
    }
    if (TabsArray == nullptr || TabsArray->empty())
    {
        return false;
    }

    if (const std::string* DefaultKey = FindString(Value, "default_tab_key"))
    {
        if (!IsValidRepeatedElementKey(*DefaultKey))
        {
            return false;
        }
    }

    TSet<FString> TabKeys;
    const FGV2ScreenFieldAdapterRegistry& Registry = FGV2ScreenFieldAdapterRegistry::Get();

    for (const GV2RuntimeCore::FValue& TabVal : *TabsArray)
    {
        const FObject* TabObj = AsObject(TabVal);
        if (TabObj == nullptr)
        {
            return false;
        }

        const std::string* Key = FindString(*TabObj, "key");
        if (Key == nullptr || !IsValidRepeatedElementKey(*Key))
        {
            return false;
        }

        const FString TabKeyStr = UTF8_TO_TCHAR(Key->c_str());
        if (TabKeys.Contains(TabKeyStr))
        {
            return false; // Duplicate tab key
        }
        TabKeys.Add(TabKeyStr);

        const GV2RuntimeCore::FValue* TitleVal = FindValue(*TabObj, "title");
        GV2RuntimeCore::FTextSpec TitleSpec;
        if (TitleVal == nullptr || !ReadTextSpec(*TitleVal, TitleSpec))
        {
            return false; // Raw strings or invalid TextSpec rejected
        }

        const std::string* TabScreenId = FindString(*TabObj, "screen_id");
        if (TabScreenId == nullptr || !GV2RuntimeCore::FStableId::IsOfKind(*TabScreenId, "screen"))
        {
            return false;
        }

        const GV2RuntimeCore::FValue* FieldsVal = FindValue(*TabObj, "fields");
        const FObject* FieldsObj = FieldsVal != nullptr ? AsObject(*FieldsVal) : nullptr;
        if (FieldsObj != nullptr)
        {
            for (const auto& [ChildFieldId, ChildFieldVal] : *FieldsObj)
            {
                const FObject* ChildFieldObj = AsObject(ChildFieldVal);
                if (ChildFieldObj == nullptr)
                {
                    return false;
                }
                const std::string* ChildSchemaId = FindString(*ChildFieldObj, "schema_id");
                const GV2RuntimeCore::FValue* ChildValue = FindValue(*ChildFieldObj, "value");
                if (ChildSchemaId == nullptr || ChildValue == nullptr)
                {
                    return false;
                }

                // Check UIF-22: Tabs inside tabs disallowed
                if (*ChildSchemaId == TabContainerSchema)
                {
                    return false;
                }

                const FObject* ChildValObj = AsObject(*ChildValue);
                if (ChildValObj == nullptr)
                {
                    return false;
                }

                GV2RuntimeCore::FScreenField ChildField;
                ChildField.FieldId = ChildFieldId;
                ChildField.SchemaId = *ChildSchemaId;
                ChildField.Value = *ChildValue;

                const auto* ChildAdapter = Registry.Find(*ChildSchemaId);
                if (ChildAdapter == nullptr)
                {
                    return false;
                }

                TArray<FGV2UiBindingDefinition> ChildDefs;
                if (!ChildAdapter->PrepareBindings(*TabScreenId, ChildField, *ChildValObj, ChildDefs))
                {
                    return false;
                }

                for (FGV2UiBindingDefinition& Def : ChildDefs)
                {
                    if (Def.NodeKeyPath.Num() >= 2 && Def.NodeKeyPath[0] == TEXT("route") && Def.NodeKeyPath[1] == TEXT("main"))
                    {
                        Def.NodeKeyPath.RemoveAt(0, 2);
                    }
                    Def.NodeKeyPath.Insert(TabKeyStr, 0);
                    Def.NodeKeyPath.Insert(FieldId(Field), 0);
                    OutDefinitions.Add(MoveTemp(Def));
                }
            }
        }
    }

    return true;
}

bool BuildTabContainerField(
    const GV2RuntimeCore::FScreenField& Field,
    const FObject& Value,
    const TArray<FGV2UiBindingHandle>& Handles,
    int32& InOutHandleIndex,
    FGV2ScreenFieldValue& OutField)
{
    const FArray* TabsArray = nullptr;
    if (const GV2RuntimeCore::FValue* TabsVal = FindValue(Value, "tabs"))
    {
        TabsArray = AsArray(*TabsVal);
    }
    if (TabsArray == nullptr || TabsArray->empty())
    {
        return false;
    }

    FGV2TabContainerViewModel Model;
    if (const std::string* DefaultKey = FindString(Value, "default_tab_key"))
    {
        Model.DefaultTabKey = FName(UTF8_TO_TCHAR(DefaultKey->c_str()));
    }

    const FGV2ScreenFieldAdapterRegistry& Registry = FGV2ScreenFieldAdapterRegistry::Get();

    for (const GV2RuntimeCore::FValue& TabVal : *TabsArray)
    {
        const FObject* TabObj = AsObject(TabVal);
        if (TabObj == nullptr)
        {
            return false;
        }

        const std::string* Key = FindString(*TabObj, "key");
        if (Key == nullptr)
        {
            return false;
        }

        FGV2TabItemViewModel& TabItem = Model.Tabs.AddDefaulted_GetRef();
        TabItem.Key = FName(UTF8_TO_TCHAR(Key->c_str()));

        const GV2RuntimeCore::FValue* TitleVal = FindValue(*TabObj, "title");
        if (TitleVal == nullptr || !ResolveText(*TitleVal, TabItem.Title))
        {
            return false;
        }

        const std::string* TabScreenId = FindString(*TabObj, "screen_id");
        if (TabScreenId == nullptr)
        {
            return false;
        }
        TabItem.ScreenId = UTF8_TO_TCHAR(TabScreenId->c_str());

        const GV2RuntimeCore::FValue* FieldsVal = FindValue(*TabObj, "fields");
        const FObject* FieldsObj = FieldsVal != nullptr ? AsObject(*FieldsVal) : nullptr;
        if (FieldsObj != nullptr)
        {
            for (const auto& [ChildFieldId, ChildFieldVal] : *FieldsObj)
            {
                const FObject* ChildFieldObj = AsObject(ChildFieldVal);
                if (ChildFieldObj == nullptr)
                {
                    return false;
                }
                const std::string* ChildSchemaId = FindString(*ChildFieldObj, "schema_id");
                const GV2RuntimeCore::FValue* ChildValue = FindValue(*ChildFieldObj, "value");
                if (ChildSchemaId == nullptr || ChildValue == nullptr)
                {
                    return false;
                }

                const FObject* ChildValObj = AsObject(*ChildValue);
                if (ChildValObj == nullptr)
                {
                    return false;
                }

                GV2RuntimeCore::FScreenField ChildField;
                ChildField.FieldId = ChildFieldId;
                ChildField.SchemaId = *ChildSchemaId;
                ChildField.Value = *ChildValue;

                const auto* ChildAdapter = Registry.Find(*ChildSchemaId);
                if (ChildAdapter == nullptr)
                {
                    return false;
                }

                FGV2ScreenFieldValue BuiltChildField;
                if (!ChildAdapter->BuildField(ChildField, *ChildValObj, Handles, InOutHandleIndex, BuiltChildField))
                {
                    return false;
                }
                TabItem.Fields.Add(MoveTemp(BuiltChildField));
            }
        }
    }

    OutField = FGV2ScreenFieldValue::MakeTabContainer(FName(*FieldId(Field)), Model);
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

bool PrepareLocationTopBar(const std::string&, const GV2RuntimeCore::FScreenField&, const FObject& Value, TArray<FGV2UiBindingDefinition>&)
{
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
    FGV2LocationTopBarViewModel Model;
    return ResolveText(*FindValue(Value, "day"), Model.Day)
        && ResolveText(*FindValue(Value, "location"), Model.Location)
        && ResolveText(*FindValue(Value, "primary_resource"), Model.PrimaryResource)
        && (OutField = FGV2ScreenFieldValue::MakeLocationTopBar(FName(*FieldId(Field)), Model), true);
}

bool PrepareLocationPlayerStatus(const std::string&, const GV2RuntimeCore::FScreenField&, const FObject& Value, TArray<FGV2UiBindingDefinition>&)
{
    const GV2RuntimeCore::FValue* Name = FindValue(Value, "name");
    GV2RuntimeCore::FTextSpec Spec;
    FString PortraitResourceId;
    return Name != nullptr && ReadTextSpec(*Name, Spec) && ReadOptionalResource(Value, "portrait_resource_id", PortraitResourceId);
}

bool BuildLocationPlayerStatus(const GV2RuntimeCore::FScreenField& Field, const FObject& Value, const TArray<FGV2UiBindingHandle>&, int32&, FGV2ScreenFieldValue& OutField)
{
    FGV2LocationPlayerStatusViewModel Model;
    if (!ReadOptionalResource(Value, "portrait_resource_id", Model.PortraitResourceId)
        || !ResolveText(*FindValue(Value, "name"), Model.Name)) return false;
    OutField = FGV2ScreenFieldValue::MakeLocationPlayerStatus(FName(*FieldId(Field)), Model);
    return true;
}

bool PrepareLocationScene(const std::string&, const GV2RuntimeCore::FScreenField&, const FObject& Value, TArray<FGV2UiBindingDefinition>&)
{
    FString Ignored;
    if (!ReadOptionalResource(Value, "background_tile_resource_id", Ignored)
        || !ReadOptionalResource(Value, "background_resource_id", Ignored)) return false;
    if (const GV2RuntimeCore::FValue* Context = FindValue(Value, "context_text")) { GV2RuntimeCore::FTextSpec Spec; return ReadTextSpec(*Context, Spec); }
    return true;
}

bool BuildLocationScene(const GV2RuntimeCore::FScreenField& Field, const FObject& Value, const TArray<FGV2UiBindingHandle>&, int32&, FGV2ScreenFieldValue& OutField)
{
    FGV2LocationSceneViewModel Model;
    if (!ReadOptionalResource(Value, "background_tile_resource_id", Model.BackgroundTileResourceId)
        || !ReadOptionalResource(Value, "background_resource_id", Model.BackgroundResourceId)) return false;
    if (const GV2RuntimeCore::FValue* Context = FindValue(Value, "context_text") ; Context != nullptr && !ResolveText(*Context, Model.ContextText)) return false;
    OutField = FGV2ScreenFieldValue::MakeLocationScene(FName(*FieldId(Field)), Model);
    return true;
}

bool PrepareLocationCommands(const std::string& ScreenId, const GV2RuntimeCore::FScreenField& Field, const FObject& Value, TArray<FGV2UiBindingDefinition>& Definitions)
{ return PrepareButtonList(ScreenId, Field, Value, Definitions); }

bool BuildLocationCommands(const GV2RuntimeCore::FScreenField& Field, const FObject& Value, const TArray<FGV2UiBindingHandle>& Handles, int32& HandleIndex, FGV2ScreenFieldValue& OutField)
{
    FGV2ScreenFieldValue ButtonList;
    if (!BuildButtonList(Field, Value, Handles, HandleIndex, ButtonList)) return false;
    OutField = FGV2ScreenFieldValue::MakeLocationCommands(FName(*FieldId(Field)), ButtonList.ButtonListValue);
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
        {ImageSchema, &PrepareImage, &BuildImageField},
        {ProgressBarSchema, &PrepareProgressBar, &BuildProgressBarField},
        {PortraitSchema, &PreparePortrait, &BuildPortraitField},
        {ModalSchema, &PrepareModal, &BuildModalField},
        {TabContainerSchema, &PrepareTabContainer, &BuildTabContainerField},
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
