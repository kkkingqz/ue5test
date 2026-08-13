#include "Application/GV2SessionCoordinator.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Templates/UnrealTemplate.h"
#include "UI/GV2TextPipeline.h"

namespace
{
constexpr std::string_view ButtonListFieldSchema = "core:schema.ui_field.button_list.v2";
constexpr std::string_view RichTextFieldSchema = "core:schema.ui_field.rich_text.v3";
constexpr std::string_view CheckboxFieldSchema = "core:schema.ui_field.checkbox.v1";
constexpr std::string_view InputFieldSchema = "core:schema.ui_field.input_field.v1";
constexpr std::string_view DropdownSelectFieldSchema = "core:schema.ui_field.dropdown_select.v1";
constexpr TCHAR CheckboxInputSchema[] = TEXT("core:schema.ui_input.checkbox_changed.v1");
constexpr TCHAR InputFieldInputSchema[] = TEXT("core:schema.ui_input.text_changed.v1");
constexpr TCHAR DropdownSelectInputSchema[] = TEXT("core:schema.ui_input.dropdown_selected.v1");

bool IsCanonicalPortableSegment(const std::string_view Value)
{
    if (Value.empty() || Value.size() > 64 || Value.front() < 'a' || Value.front() > 'z') return false;
    for (const char Character : Value)
    {
        if (!((Character >= 'a' && Character <= 'z')
            || (Character >= '0' && Character <= '9') || Character == '_')) return false;
    }
    return true;
}

bool IsPortableStableIdOfKind(const std::string_view Value, const std::string_view Kind)
{
    const std::size_t Colon = Value.find(':');
    const std::size_t Dot = Value.find('.', Colon == std::string_view::npos ? 0 : Colon + 1);
    if (Colon == std::string_view::npos || Dot == std::string_view::npos
        || !IsCanonicalPortableSegment(Value.substr(0, Colon))
        || Value.substr(Colon + 1, Dot - Colon - 1) != Kind) return false;
    std::size_t Start = Dot + 1;
    while (Start < Value.size())
    {
        const std::size_t End = Value.find('.', Start);
        if (!IsCanonicalPortableSegment(Value.substr(
            Start, End == std::string_view::npos ? Value.size() - Start : End - Start))) return false;
        if (End == std::string_view::npos) return true;
        Start = End + 1;
    }
    return false;
}

std::string ToUtf8(const FString& Value)
{
    const FTCHARToUTF8 Converted(*Value);
    return std::string(Converted.Get(), Converted.Length());
}

bool LoadPortableRuntimeSources(
    std::vector<GV2RuntimeCore::FRuntimeSource>& OutSources,
    GV2RuntimeCore::FRuntimeFault& OutFault)
{
    OutSources.clear();
    FString ScriptsDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Scripts"));
    FPaths::NormalizeDirectoryName(ScriptsDirectory);
    const FString ScriptsPrefix = ScriptsDirectory + TEXT("/");
    TArray<FString> SourceFiles;
    IFileManager::Get().FindFilesRecursive(
        SourceFiles,
        *ScriptsDirectory,
        TEXT("*.lua"),
        true,
        false,
        false);
    SourceFiles.Sort();
    if (SourceFiles.IsEmpty())
    {
        OutFault = {"LuaRuntimeSourceMissing", "Scripts directory contains no Lua sources."};
        return false;
    }

    OutSources.reserve(SourceFiles.Num());
    for (const FString& FullPath : SourceFiles)
    {
        TArray<uint8> Bytes;
        if (!FFileHelper::LoadFileToArray(Bytes, *FullPath))
        {
            OutFault = {
                "LuaRuntimeSourceMissing",
                ToUtf8(FString::Printf(TEXT("Lua source could not be read: %s"), *FullPath))};
            return false;
        }

        int32 Offset = 0;
        if (Bytes.Num() >= 3 && Bytes[0] == 0xef && Bytes[1] == 0xbb && Bytes[2] == 0xbf)
        {
            Offset = 3;
        }
        if (Bytes.Num() <= Offset)
        {
            OutFault = {
                "LuaRuntimeSourceInvalid",
                ToUtf8(FString::Printf(TEXT("Lua source is empty: %s"), *FullPath))};
            return false;
        }
        FString NormalizedFullPath = FullPath;
        FPaths::NormalizeFilename(NormalizedFullPath);
        if (!NormalizedFullPath.StartsWith(ScriptsPrefix, ESearchCase::CaseSensitive))
        {
            OutFault = {"LuaRuntimeSourceInvalid", "Lua source is outside the Scripts directory."};
            return false;
        }
        const FString RelativePath = NormalizedFullPath.RightChop(ScriptsPrefix.Len());
        GV2RuntimeCore::FRuntimeSource& Source = OutSources.emplace_back();
        Source.Name = "@Scripts/" + ToUtf8(RelativePath);
        Source.Text.assign(
            reinterpret_cast<const char*>(Bytes.GetData() + Offset),
            static_cast<std::size_t>(Bytes.Num() - Offset));
    }
    return true;
}

GV2RuntimeCore::FValue ToPortableValue(const FGV2UiControlValue& Value)
{
    switch (Value.Type)
    {
    case EGV2UiControlValueType::Null:
        return GV2RuntimeCore::FValue();
    case EGV2UiControlValueType::Boolean:
        return GV2RuntimeCore::FValue(Value.BooleanValue);
    case EGV2UiControlValueType::Integer:
        return GV2RuntimeCore::FValue(static_cast<std::int64_t>(Value.IntegerValue));
    case EGV2UiControlValueType::Number:
        return GV2RuntimeCore::FValue(Value.NumberValue);
    case EGV2UiControlValueType::String:
        return GV2RuntimeCore::FValue(ToUtf8(Value.StringValue));
    default:
        checkNoEntry();
        return GV2RuntimeCore::FValue();
    }
}

bool ToUiControlValue(
    const std::string& Name,
    const GV2RuntimeCore::FValue& Value,
    FGV2UiControlValue& OutValue)
{
    OutValue = {};
    OutValue.Name = FName(UTF8_TO_TCHAR(Name.c_str()));
    if (OutValue.Name.IsNone())
    {
        return false;
    }
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
        if (!FMath::IsFinite(*Number))
        {
            return false;
        }
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

bool ResolvePortableText(
    const GV2RuntimeCore::FTextSpec& Spec,
    FGV2TextViewModel& OutText)
{
    OutText = {};
    if (Spec.TextId.empty())
    {
        return true;
    }
    TArray<FGV2UiControlValue> Args;
    Args.Reserve(static_cast<int32>(Spec.Args.size()));
    for (const auto& [Name, Value] : Spec.Args)
    {
        FGV2UiControlValue& Arg = Args.AddDefaulted_GetRef();
        if (!ToUiControlValue(Name, Value, Arg))
        {
            return false;
        }
    }
    FString Error;
    return UGV2TextPipeline::Resolve(
        UTF8_TO_TCHAR(Spec.TextId.c_str()),
        Args,
        FName(UTF8_TO_TCHAR(Spec.Style.c_str())),
        OutText,
        Error);
}

GV2RuntimeCore::FSemanticInput ToPortableInput(const FGV2UiIngressItem& Item)
{
    GV2RuntimeCore::FSemanticInput Input;
    Input.SessionGeneration = Item.Binding.SessionGeneration;
    Input.UiInstanceId = ToUtf8(Item.Binding.UiInstanceId);
    Input.Revision = Item.Binding.Revision;
    Input.Sequence = Item.Sequence;
    Input.ElementId = ToUtf8(Item.Binding.ElementId);
    Input.CommandId = ToUtf8(Item.Binding.CommandId);
    Input.NodeKeyPath.reserve(Item.Binding.NodeKeyPath.Num());
    for (const FString& Segment : Item.Binding.NodeKeyPath)
    {
        Input.NodeKeyPath.push_back(ToUtf8(Segment));
    }
    for (const FGV2UiControlValue& Value : Item.Binding.BoundArgs)
    {
        Input.Args.emplace(ToUtf8(Value.Name.ToString()), ToPortableValue(Value));
    }
    for (const FGV2UiControlValue& Value : Item.InputValues)
    {
        Input.Args.emplace(ToUtf8(Value.Name.ToString()), ToPortableValue(Value));
    }
    return Input;
}

const GV2RuntimeCore::FValue::FObject* AsObject(const GV2RuntimeCore::FValue& Value)
{
    return std::get_if<GV2RuntimeCore::FValue::FObject>(&Value.Data);
}

const GV2RuntimeCore::FValue::FArray* AsArray(const GV2RuntimeCore::FValue& Value)
{
    if (const auto* Array = std::get_if<GV2RuntimeCore::FValue::FArray>(&Value.Data))
    {
        return Array;
    }
    // Lua has no intrinsic distinction between an empty array and an empty object.
    // The schema adapter supplies that context without introducing schema-specific Lua code.
    static const GV2RuntimeCore::FValue::FArray EmptyArray;
    const auto* Object = std::get_if<GV2RuntimeCore::FValue::FObject>(&Value.Data);
    return Object != nullptr && Object->empty() ? &EmptyArray : nullptr;
}

const std::string* FindString(
    const GV2RuntimeCore::FValue::FObject& Object,
    const std::string_view Name)
{
    const auto It = Object.find(Name);
    return It != Object.end() ? std::get_if<std::string>(&It->second.Data) : nullptr;
}

const bool* FindBoolean(
    const GV2RuntimeCore::FValue::FObject& Object,
    const std::string_view Name)
{
    const auto It = Object.find(Name);
    return It != Object.end() ? std::get_if<bool>(&It->second.Data) : nullptr;
}

const GV2RuntimeCore::FValue* FindValue(
    const GV2RuntimeCore::FValue::FObject& Object,
    const std::string_view Name)
{
    const auto It = Object.find(Name);
    return It != Object.end() ? &It->second : nullptr;
}

bool ReadTextSpecValue(const GV2RuntimeCore::FValue& Value, GV2RuntimeCore::FTextSpec& OutSpec)
{
    const GV2RuntimeCore::FValue::FObject* Object = AsObject(Value);
    const std::string* TextId = Object != nullptr ? FindString(*Object, "text_id") : nullptr;
    if (TextId == nullptr || !IsPortableStableIdOfKind(*TextId, "text"))
    {
        return false;
    }
    OutSpec = {};
    OutSpec.TextId = *TextId;
    if (const std::string* Style = FindString(*Object, "style"))
    {
        OutSpec.Style = *Style;
    }
    if (const GV2RuntimeCore::FValue* Args = FindValue(*Object, "args"))
    {
        const GV2RuntimeCore::FValue::FObject* ArgsObject = AsObject(*Args);
        if (ArgsObject == nullptr) return false;
        OutSpec.Args = *ArgsObject;
    }
    return true;
}

bool ReadBindingDefinition(
    const GV2RuntimeCore::FValue& Value,
    const TArray<FString>& NodePath,
    const FString& ElementId,
    FGV2UiBindingDefinition& OutDefinition)
{
    const GV2RuntimeCore::FValue::FObject* Object = AsObject(Value);
    const std::string* CommandId = Object != nullptr ? FindString(*Object, "command_id") : nullptr;
    if (CommandId == nullptr || !IsPortableStableIdOfKind(*CommandId, "command")) return false;
    OutDefinition = {};
    OutDefinition.NodeKeyPath = NodePath;
    OutDefinition.ElementId = ElementId;
    OutDefinition.CommandId = UTF8_TO_TCHAR(CommandId->c_str());
    if (const GV2RuntimeCore::FValue* Args = FindValue(*Object, "args"))
    {
        const GV2RuntimeCore::FValue::FObject* ArgsObject = AsObject(*Args);
        if (ArgsObject == nullptr) return false;
        for (const auto& [Name, ArgValue] : *ArgsObject)
        {
            FGV2UiControlValue& Argument = OutDefinition.BoundArgs.AddDefaulted_GetRef();
            if (!ToUiControlValue(Name, ArgValue, Argument)) return false;
        }
    }
    return true;
}
}

FGV2SessionCoordinator::FGV2SessionCoordinator(const int32 InIngressCapacity)
    : IngressQueue(InIngressCapacity)
{
}

void FGV2SessionCoordinator::SetInteractionSink(FInteractionSink InSink)
{
    InteractionSink = MoveTemp(InSink);
}

void FGV2SessionCoordinator::ClearInteractionSink()
{
    InteractionSink = nullptr;
}

void FGV2SessionCoordinator::SetScreenSink(FScreenSink InSink)
{
    ScreenSink = MoveTemp(InSink);
}

void FGV2SessionCoordinator::ClearScreenSink()
{
    ScreenSink = nullptr;
}

bool FGV2SessionCoordinator::StartSession()
{
    check(IsInGameThread());

    BindingRegistry.EndSession();
    IngressQueue.Reset();
    RuntimeSession.Stop();

    ++Status.SessionGeneration;
    Status.ApplicationState = EGV2ApplicationState::Bootstrapping;
    Status.SessionState = EGV2SessionState::Creating;
    Status.bIsReady = false;
    NextInputSequence = 1;
    UiRevision = 0;
    BindingRegistry.BeginSession(Status.SessionGeneration);

    GV2RuntimeCore::FRuntimeFault Fault;
    std::vector<GV2RuntimeCore::FRuntimeSource> RuntimeSources;
    if (!LoadPortableRuntimeSources(RuntimeSources, Fault)
        || !RuntimeSession.Start(Status.SessionGeneration, RuntimeSources, Fault))
    {
        FailRuntime(Fault);
        return false;
    }

    Status.ApplicationState = EGV2ApplicationState::MenuActive;
    Status.SessionState = EGV2SessionState::Ready;
    Status.bIsReady = true;
    return true;
}

void FGV2SessionCoordinator::EndSession(const EGV2SessionState FinalState)
{
    check(IsInGameThread());

    Status.bIsReady = false;
    BindingRegistry.EndSession();
    IngressQueue.Reset();
    RuntimeSession.Stop();
    Status.ApplicationState = EGV2ApplicationState::Uninitialized;
    Status.SessionState = FinalState;
    NextInputSequence = 1;
    UiRevision = 0;
}

const FGV2SessionStatus& FGV2SessionCoordinator::GetStatus() const
{
    return Status;
}

bool FGV2SessionCoordinator::PublishUiBindings(
    const FString& UiInstanceId,
    const int64 Revision,
    const TArray<FGV2UiBindingDefinition>& Definitions,
    TArray<FGV2UiBindingHandle>& OutHandles)
{
    check(IsInGameThread());

    if (!Status.bIsReady || Status.SessionState != EGV2SessionState::Ready)
    {
        OutHandles.Reset();
        return false;
    }

    return BindingRegistry.PublishBindings(UiInstanceId, Revision, Definitions, OutHandles);
}

bool FGV2SessionCoordinator::PublishScreenBindings(
    const TArray<FGV2UiBindingDefinition>& Definitions,
    TArray<FGV2UiBindingHandle>& OutHandles)
{
    const FString UiInstanceId = FString::Printf(TEXT("ui@%d:1"), Status.SessionGeneration);
    const int64 CandidateRevision = UiRevision + 1;
    if (!PublishUiBindings(UiInstanceId, CandidateRevision, Definitions, OutHandles))
    {
        return false;
    }

    UiRevision = CandidateRevision;
    return true;
}

bool FGV2SessionCoordinator::BuildDebugStartButtonModel(FGV2ButtonViewModel& OutModel)
{
    check(IsInGameThread());
    OutModel = {};
    OutModel.Key = TEXT("start");
    if (!Status.bIsReady || Status.SessionState != EGV2SessionState::Ready)
    {
        return false;
    }

    FGV2UiBindingDefinition Definition;
    Definition.NodeKeyPath = {TEXT("route"), TEXT("start")};
    Definition.ElementId = TEXT("core:screen.debug_start#widget.start");
    Definition.CommandId = TEXT("core:command.debug.start");

    TArray<FGV2UiBindingHandle> Handles;
    if (!PublishScreenBindings({Definition}, Handles) || Handles.Num() != 1)
    {
        return false;
    }

    FString TextError;
    if (!UGV2TextPipeline::Resolve(
            TEXT("core:text.debug.start"), {}, TEXT("button"), OutModel.Text, TextError))
    {
        return false;
    }
    OutModel.Binding = Handles[0];
    return true;
}

bool FGV2SessionCoordinator::PrepareScreenRequest(
    const GV2RuntimeCore::FScreenRequest& Request,
    FGV2ScreenViewModel& OutModel,
    FGV2PreparedBindingSet& OutBindings)
{
    OutModel = {};
    TArray<FGV2UiBindingDefinition> Definitions;
    for (const GV2RuntimeCore::FScreenField& Field : Request.Fields)
    {
        const GV2RuntimeCore::FValue::FObject* FieldObject = AsObject(Field.Value);
        if (FieldObject == nullptr) return false;
        if (Field.SchemaId == ButtonListFieldSchema)
        {
            const GV2RuntimeCore::FValue* ItemsValue = FindValue(*FieldObject, "items");
            const GV2RuntimeCore::FValue::FArray* Items = ItemsValue != nullptr ? AsArray(*ItemsValue) : nullptr;
            if (Items == nullptr) return false;
            for (const GV2RuntimeCore::FValue& ItemValue : *Items)
            {
                const GV2RuntimeCore::FValue::FObject* Item = AsObject(ItemValue);
                const std::string* Key = Item != nullptr ? FindString(*Item, "key") : nullptr;
                const GV2RuntimeCore::FValue* Binding = Item != nullptr ? FindValue(*Item, "binding") : nullptr;
                if (Key == nullptr || !IsCanonicalPortableSegment(*Key) || Binding == nullptr) return false;
                FGV2UiBindingDefinition& Definition = Definitions.AddDefaulted_GetRef();
                if (!ReadBindingDefinition(
                    *Binding,
                    {TEXT("route"), TEXT("main"), UTF8_TO_TCHAR(Field.FieldId.c_str()), UTF8_TO_TCHAR(Key->c_str())},
                    FString::Printf(TEXT("%s#widget.%s"), UTF8_TO_TCHAR(Request.ScreenId.c_str()), UTF8_TO_TCHAR(Key->c_str())),
                    Definition)) return false;
            }
        }
        else if (Field.SchemaId == RichTextFieldSchema)
        {
            const GV2RuntimeCore::FValue* SpansValue = FindValue(*FieldObject, "spans");
            const GV2RuntimeCore::FValue::FArray* Spans = SpansValue != nullptr ? AsArray(*SpansValue) : nullptr;
            if (Spans == nullptr) return false;
            for (const GV2RuntimeCore::FValue& SpanValue : *Spans)
            {
                const GV2RuntimeCore::FValue::FObject* Span = AsObject(SpanValue);
                const std::string* Key = Span != nullptr ? FindString(*Span, "key") : nullptr;
                const GV2RuntimeCore::FValue* Binding = Span != nullptr ? FindValue(*Span, "binding") : nullptr;
                if (Key == nullptr || !IsCanonicalPortableSegment(*Key)) return false;
                if (Binding != nullptr)
                {
                    FGV2UiBindingDefinition& Definition = Definitions.AddDefaulted_GetRef();
                    if (!ReadBindingDefinition(
                        *Binding,
                        {TEXT("route"), TEXT("main"), UTF8_TO_TCHAR(Field.FieldId.c_str()), UTF8_TO_TCHAR(Key->c_str())},
                        FString::Printf(TEXT("%s#span.%s"), UTF8_TO_TCHAR(Request.ScreenId.c_str()), UTF8_TO_TCHAR(Key->c_str())),
                        Definition)) return false;
                }
            }
        }
        else if (Field.SchemaId == CheckboxFieldSchema)
        {
            const GV2RuntimeCore::FValue* Text = FindValue(*FieldObject, "text");
            const bool* bIsChecked = FindBoolean(*FieldObject, "is_checked");
            const GV2RuntimeCore::FValue* Binding = FindValue(*FieldObject, "binding");
            GV2RuntimeCore::FTextSpec TextSpec;
            if (Text == nullptr || !ReadTextSpecValue(*Text, TextSpec)
                || bIsChecked == nullptr || Binding == nullptr)
            {
                return false;
            }

            FGV2UiBindingDefinition& Definition = Definitions.AddDefaulted_GetRef();
            const FString FieldId = UTF8_TO_TCHAR(Field.FieldId.c_str());
            if (!ReadBindingDefinition(
                    *Binding,
                    {TEXT("route"), TEXT("main"), FieldId},
                    FString::Printf(
                        TEXT("%s#widget.%s"),
                        UTF8_TO_TCHAR(Request.ScreenId.c_str()),
                        *FieldId),
                    Definition))
            {
                return false;
            }
            FGV2UiInputFieldDefinition& InputField = Definition.InputFields.AddDefaulted_GetRef();
            InputField.Name = TEXT("is_checked");
            InputField.Type = EGV2UiControlValueType::Boolean;
            InputField.bRequired = true;
            Definition.InputSchemaId = CheckboxInputSchema;
        }
        else if (Field.SchemaId == InputFieldSchema)
        {
            const GV2RuntimeCore::FValue* Binding = FindValue(*FieldObject, "binding");
            if (Binding == nullptr)
            {
                return false;
            }

            FGV2UiBindingDefinition& Definition = Definitions.AddDefaulted_GetRef();
            const FString FieldId = UTF8_TO_TCHAR(Field.FieldId.c_str());
            if (!ReadBindingDefinition(
                    *Binding,
                    {TEXT("route"), TEXT("main"), FieldId},
                    FString::Printf(
                        TEXT("%s#widget.%s"),
                        UTF8_TO_TCHAR(Request.ScreenId.c_str()),
                        *FieldId),
                    Definition))
            {
                return false;
            }
            FGV2UiInputFieldDefinition& InputFieldDef = Definition.InputFields.AddDefaulted_GetRef();
            InputFieldDef.Name = TEXT("value");
            InputFieldDef.Type = EGV2UiControlValueType::String;
            InputFieldDef.bRequired = true;
            Definition.InputSchemaId = InputFieldInputSchema;
        }
        else if (Field.SchemaId == DropdownSelectFieldSchema)
        {
            const GV2RuntimeCore::FValue* Binding = FindValue(*FieldObject, "binding");
            if (Binding == nullptr)
            {
                return false;
            }

            FGV2UiBindingDefinition& Definition = Definitions.AddDefaulted_GetRef();
            const FString FieldIdStr = UTF8_TO_TCHAR(Field.FieldId.c_str());
            if (!ReadBindingDefinition(
                    *Binding,
                    {TEXT("route"), TEXT("main"), FieldIdStr},
                    FString::Printf(
                        TEXT("%s#widget.%s"),
                        UTF8_TO_TCHAR(Request.ScreenId.c_str()),
                        *FieldIdStr),
                    Definition))
            {
                return false;
            }
            FGV2UiInputFieldDefinition& InputField = Definition.InputFields.AddDefaulted_GetRef();
            InputField.Name = TEXT("selected_key");
            InputField.Type = EGV2UiControlValueType::String;
            InputField.bRequired = true;
            Definition.InputSchemaId = DropdownSelectInputSchema;
        }
        else return false;
    }
    const FString UiInstanceId = FString::Printf(TEXT("ui@%d:1"), Status.SessionGeneration);
    const int64 CandidateRevision = UiRevision + 1;
    if (!BindingRegistry.PrepareBindings(UiInstanceId, CandidateRevision, Definitions, OutBindings)
        || OutBindings.Handles.Num() != Definitions.Num())
    {
        return false;
    }
    int32 HandleIndex = 0;
    OutModel.ScreenId = UTF8_TO_TCHAR(Request.ScreenId.c_str());
    for (const GV2RuntimeCore::FScreenField& Field : Request.Fields)
    {
        const GV2RuntimeCore::FValue::FObject& FieldObject = *AsObject(Field.Value);
        const FName FieldId(UTF8_TO_TCHAR(Field.FieldId.c_str()));
        if (Field.SchemaId == ButtonListFieldSchema)
        {
            TArray<FGV2ButtonViewModel> Buttons;
            const auto& Items = *AsArray(*FindValue(FieldObject, "items"));
            for (const GV2RuntimeCore::FValue& ItemValue : Items)
            {
                const auto& Item = *AsObject(ItemValue);
                FGV2ButtonViewModel& Button = Buttons.AddDefaulted_GetRef();
                Button.Key = FName(UTF8_TO_TCHAR(FindString(Item, "key")->c_str()));
                const GV2RuntimeCore::FValue* TextValue = FindValue(Item, "text");
                GV2RuntimeCore::FTextSpec TextSpec;
                if (TextValue == nullptr || !ReadTextSpecValue(*TextValue, TextSpec)
                    || !ResolvePortableText(TextSpec, Button.Text)) return false;
                Button.Binding = OutBindings.Handles[HandleIndex++];
            }
            OutModel.Fields.Add(FGV2ScreenFieldValue::MakeButtonList(FieldId, Buttons));
        }
        else if (Field.SchemaId == RichTextFieldSchema)
        {
            FGV2InteractiveRichTextViewModel RichText;
            GV2RuntimeCore::FTextSpec TextSpec;
            const GV2RuntimeCore::FValue* TextValue = FindValue(FieldObject, "text");
            if (TextValue == nullptr || !ReadTextSpecValue(*TextValue, TextSpec)
                || !ResolvePortableText(TextSpec, RichText.Text)) return false;
            const auto& Spans = *AsArray(*FindValue(FieldObject, "spans"));
            for (const GV2RuntimeCore::FValue& SpanValue : Spans)
            {
                const auto& Span = *AsObject(SpanValue);
                FGV2RichTextSpanViewModel& SpanModel = RichText.Spans.AddDefaulted_GetRef();
                const std::string& Key = *FindString(Span, "key");
                const std::string* SpanId = FindString(Span, "span_id");
                SpanModel.Key = FName(UTF8_TO_TCHAR(Key.c_str()));
                SpanModel.SpanId = FName(UTF8_TO_TCHAR((SpanId != nullptr ? *SpanId : Key).c_str()));
                if (const GV2RuntimeCore::FValue* HoverValue = FindValue(Span, "hover"))
                {
                    const auto* Hover = AsObject(*HoverValue);
                    if (Hover == nullptr) return false;
                    GV2RuntimeCore::FTextSpec HoverSpec;
                    if (const GV2RuntimeCore::FValue* Title = FindValue(*Hover, "title"))
                    {
                        if (!ReadTextSpecValue(*Title, HoverSpec)
                            || !ResolvePortableText(HoverSpec, SpanModel.Hover.Title)) return false;
                    }
                    if (const GV2RuntimeCore::FValue* Description = FindValue(*Hover, "description"))
                    {
                        if (!ReadTextSpecValue(*Description, HoverSpec)
                            || !ResolvePortableText(HoverSpec, SpanModel.Hover.Description)) return false;
                    }
                    if (const std::string* Image = FindString(*Hover, "image_resource_id"))
                    {
                        if (!IsPortableStableIdOfKind(*Image, "resource")) return false;
                        SpanModel.Hover.ImageResourceId = UTF8_TO_TCHAR(Image->c_str());
                    }
                }
                if (FindValue(Span, "binding") != nullptr)
                {
                    SpanModel.Binding = OutBindings.Handles[HandleIndex++];
                }
            }
            OutModel.Fields.Add(FGV2ScreenFieldValue::MakeInteractiveRichText(FieldId, RichText));
        }
        else if (Field.SchemaId == CheckboxFieldSchema)
        {
            FGV2CheckboxViewModel Checkbox;
            Checkbox.Key = FieldId;
            const GV2RuntimeCore::FValue* TextValue = FindValue(FieldObject, "text");
            GV2RuntimeCore::FTextSpec TextSpec;
            const bool* bIsChecked = FindBoolean(FieldObject, "is_checked");
            if (TextValue == nullptr || !ReadTextSpecValue(*TextValue, TextSpec)
                || !ResolvePortableText(TextSpec, Checkbox.Text)
                || bIsChecked == nullptr
                || !OutBindings.Handles.IsValidIndex(HandleIndex))
            {
                return false;
            }
            Checkbox.bIsChecked = *bIsChecked;
            Checkbox.Binding = OutBindings.Handles[HandleIndex++];
            OutModel.Fields.Add(FGV2ScreenFieldValue::MakeCheckbox(FieldId, Checkbox));
        }
        else if (Field.SchemaId == InputFieldSchema)
        {
            FGV2InputFieldViewModel InputFieldModel;
            InputFieldModel.Key = FieldId;
            if (const GV2RuntimeCore::FValue* Value = FindValue(FieldObject, "value"))
            {
                const std::string* TextVal = std::get_if<std::string>(&Value->Data);
                if (TextVal == nullptr)
                {
                    return false;
                }
                InputFieldModel.TextValue = UTF8_TO_TCHAR(TextVal->c_str());
            }
            if (const GV2RuntimeCore::FValue* PlaceholderTextVal = FindValue(FieldObject, "placeholder_text"))
            {
                GV2RuntimeCore::FTextSpec PlaceholderSpec;
                if (!ReadTextSpecValue(*PlaceholderTextVal, PlaceholderSpec)
                    || !ResolvePortableText(PlaceholderSpec, InputFieldModel.PlaceholderText))
                {
                    return false;
                }
            }
            if (const GV2RuntimeCore::FValue* TextVal = FindValue(FieldObject, "text"))
            {
                GV2RuntimeCore::FTextSpec TextSpec;
                if (!ReadTextSpecValue(*TextVal, TextSpec)
                    || !ResolvePortableText(TextSpec, InputFieldModel.Text))
                {
                    return false;
                }
            }
            if (!OutBindings.Handles.IsValidIndex(HandleIndex))
            {
                return false;
            }
            InputFieldModel.Binding = OutBindings.Handles[HandleIndex++];
            OutModel.Fields.Add(FGV2ScreenFieldValue::MakeInputField(FieldId, InputFieldModel));
        }
        else if (Field.SchemaId == DropdownSelectFieldSchema)
        {
            FGV2DropdownSelectViewModel DropdownModel;

            // Resolve placeholder text.
            if (const GV2RuntimeCore::FValue* PlaceholderVal = FindValue(FieldObject, "placeholder"))
            {
                GV2RuntimeCore::FTextSpec PlaceholderSpec;
                if (!ReadTextSpecValue(*PlaceholderVal, PlaceholderSpec)
                    || !ResolvePortableText(PlaceholderSpec, DropdownModel.Placeholder))
                {
                    return false;
                }
            }

            // Resolve selected_key.
            const GV2RuntimeCore::FValue* SelectedKeyValue = FindValue(FieldObject, "selected_key");
            const std::string* SelectedKeyStr = SelectedKeyValue != nullptr
                ? std::get_if<std::string>(&SelectedKeyValue->Data)
                : nullptr;
            if (SelectedKeyValue != nullptr
                && (SelectedKeyStr == nullptr || !IsCanonicalPortableSegment(*SelectedKeyStr)))
            {
                return false;
            }

            // Resolve option items.
            const GV2RuntimeCore::FValue* ItemsValue = FindValue(FieldObject, "items");
            const GV2RuntimeCore::FValue::FArray* Items = ItemsValue != nullptr ? AsArray(*ItemsValue) : nullptr;
            if (Items == nullptr)
            {
                return false;
            }
            TSet<FName> OptionKeys;
            bool bSelectedKeyFound = SelectedKeyStr == nullptr;
            for (const GV2RuntimeCore::FValue& ItemValue : *Items)
            {
                const GV2RuntimeCore::FValue::FObject* Item = AsObject(ItemValue);
                const std::string* Key = Item != nullptr ? FindString(*Item, "key") : nullptr;
                if (Key == nullptr || !IsCanonicalPortableSegment(*Key))
                {
                    return false;
                }

                const FName OptionKey(UTF8_TO_TCHAR(Key->c_str()));
                if (OptionKeys.Contains(OptionKey))
                {
                    return false;
                }
                OptionKeys.Add(OptionKey);

                FGV2DropdownOptionViewModel& Option = DropdownModel.Options.AddDefaulted_GetRef();
                Option.Key = OptionKey;

                const GV2RuntimeCore::FValue* TextVal = FindValue(*Item, "text");
                GV2RuntimeCore::FTextSpec TextSpec;
                if (TextVal == nullptr || !ReadTextSpecValue(*TextVal, TextSpec)
                    || !ResolvePortableText(TextSpec, Option.Text))
                {
                    return false;
                }

                Option.bSelected = (SelectedKeyStr != nullptr && *SelectedKeyStr == *Key);
                bSelectedKeyFound = bSelectedKeyFound || Option.bSelected;
            }

            if (!bSelectedKeyFound)
            {
                return false;
            }

            if (!OutBindings.Handles.IsValidIndex(HandleIndex))
            {
                return false;
            }
            DropdownModel.Binding = OutBindings.Handles[HandleIndex++];
            OutModel.Fields.Add(FGV2ScreenFieldValue::MakeDropdownSelect(FieldId, DropdownModel));
        }
        else
        {
            return false;
        }
    }
    return HandleIndex == OutBindings.Handles.Num();
}

EGV2SubmitUiInteractionResult FGV2SessionCoordinator::SubmitUiInteraction(
    const FGV2UiBindingHandle& BindingHandle,
    const TArray<FGV2UiControlValue>& InputValues)
{
    check(IsInGameThread());

    if (!Status.bIsReady || Status.SessionState != EGV2SessionState::Ready)
    {
        return EGV2SubmitUiInteractionResult::RuntimeNotReady;
    }

    FGV2UiBindingRecord Binding;
    switch (BindingRegistry.Resolve(BindingHandle, Binding))
    {
    case EGV2BindingResolveResult::Invalid:
        return EGV2SubmitUiInteractionResult::InvalidBindingHandle;
    case EGV2BindingResolveResult::Stale:
        return EGV2SubmitUiInteractionResult::StaleBindingHandle;
    case EGV2BindingResolveResult::Found:
        break;
    default:
        checkNoEntry();
        return EGV2SubmitUiInteractionResult::InvalidBindingHandle;
    }

    if (!ValidateInputValues(Binding, InputValues))
    {
        return EGV2SubmitUiInteractionResult::InvalidInputValues;
    }

    FGV2UiIngressItem Item;
    Item.BindingHandle = BindingHandle;
    Item.Binding = MoveTemp(Binding);
    Item.InputValues = InputValues;
    Item.Sequence = NextInputSequence;

    if (!IngressQueue.TryEnqueue(MoveTemp(Item)))
    {
        return EGV2SubmitUiInteractionResult::IngressQueueFull;
    }

    ++NextInputSequence;
    PumpIngress();
    return EGV2SubmitUiInteractionResult::Accepted;
}

bool FGV2SessionCoordinator::IsExecutingRuntime() const
{
    return bExecutingRuntime;
}

bool FGV2SessionCoordinator::IsLuaVmStarted() const
{
    return RuntimeSession.IsStarted();
}

int32 FGV2SessionCoordinator::GetQueuedIngressCount() const
{
    return IngressQueue.Num();
}

bool FGV2SessionCoordinator::ValidateInputValues(
    const FGV2UiBindingRecord& Binding,
    const TArray<FGV2UiControlValue>& InputValues)
{
    TSet<FName> SeenNames;
    for (const FGV2UiControlValue& InputValue : InputValues)
    {
        const EGV2UiControlValueType* ExpectedType = Binding.InputFieldTypes.Find(InputValue.Name);
        if (InputValue.Name.IsNone()
            || SeenNames.Contains(InputValue.Name)
            || ExpectedType == nullptr
            || InputValue.Type != *ExpectedType
            || (InputValue.Type == EGV2UiControlValueType::Number && !FMath::IsFinite(InputValue.NumberValue)))
        {
            return false;
        }
        SeenNames.Add(InputValue.Name);
    }

    for (const FName RequiredField : Binding.RequiredInputFields)
    {
        if (!SeenNames.Contains(RequiredField))
        {
            return false;
        }
    }

    return true;
}

void FGV2SessionCoordinator::PumpIngress()
{
    check(IsInGameThread());

    if (bPumpingIngress)
    {
        return;
    }

    TGuardValue<bool> PumpGuard(bPumpingIngress, true);
    FGV2UiIngressItem Item;
    while (Status.bIsReady && IngressQueue.Dequeue(Item))
    {
        std::optional<GV2RuntimeCore::FScreenRequest> PendingScreen;
        {
            TGuardValue<bool> ExecutionGuard(bExecutingRuntime, true);
            GV2RuntimeCore::FRuntimeFault Fault;
            if (!RuntimeSession.DispatchSemanticInput(ToPortableInput(Item), Fault))
            {
                FailRuntime(Fault);
                return;
            }
            if (!RuntimeSession.TakePendingScreen(PendingScreen, Fault))
            {
                FailRuntime(Fault);
                return;
            }
        }

        if (PendingScreen && ScreenSink)
        {
            FGV2ScreenViewModel Model;
            FGV2PreparedBindingSet PreparedBindings;
            if (PrepareScreenRequest(*PendingScreen, Model, PreparedBindings)
                && ScreenSink(Model)
                && BindingRegistry.CommitPreparedBindings(MoveTemp(PreparedBindings)))
            {
                ++UiRevision;
            }
        }
        if (InteractionSink)
        {
            InteractionSink(Item);
        }
    }
}

void FGV2SessionCoordinator::FailRuntime(const GV2RuntimeCore::FRuntimeFault& Fault)
{
    Status.bIsReady = false;
    Status.ApplicationState = EGV2ApplicationState::Failed;
    Status.SessionState = EGV2SessionState::Failed;
    BindingRegistry.EndSession();
    IngressQueue.Reset();

    UE_LOG(
        LogTemp,
        Error,
        TEXT("GV2 Lua runtime fault: code=%s message=%s"),
        UTF8_TO_TCHAR(Fault.Code.c_str()),
        UTF8_TO_TCHAR(Fault.Message.c_str()));
}
