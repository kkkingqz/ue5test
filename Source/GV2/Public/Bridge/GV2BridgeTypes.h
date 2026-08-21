#pragma once

#include "CoreMinimal.h"
#include "GV2BridgeTypes.generated.h"

UENUM(BlueprintType)
enum class EGV2UiControlValueType : uint8
{
    Null,
    Boolean,
    Integer,
    Number,
    String
};

UENUM(BlueprintType)
enum class EGV2SubmitUiInteractionResult : uint8
{
    Accepted,
    RuntimeNotReady,
    InvalidBindingHandle,
    StaleBindingHandle,
    InvalidInputValues,
    IngressQueueFull
};

UENUM(BlueprintType)
enum class EGV2ApplicationState : uint8
{
    Uninitialized,
    Bootstrapping,
    MenuActive,
    GameActive,
    Transitioning,
    ShuttingDown,
    Failed,
    Terminated
};

UENUM(BlueprintType)
enum class EGV2SessionState : uint8
{
    None,
    Creating,
    Registering,
    BuildingState,
    RestoringInstances,
    Starting,
    PreparingPresentation,
    Ready,
    Failed,
    Stopping,
    Destroyed
};

USTRUCT(BlueprintType)
struct GV2_API FGV2UiBindingHandle
{
    GENERATED_BODY()

public:
    bool IsValid() const
    {
        return !Value.IsEmpty();
    }

    const FString& ToString() const
    {
        return Value;
    }

    static FGV2UiBindingHandle Create(FString InValue)
    {
        FGV2UiBindingHandle Handle;
        Handle.Value = MoveTemp(InValue);
        return Handle;
    }

    static FGV2UiBindingHandle FromSerialized(FString InValue)
    {
        return Create(MoveTemp(InValue));
    }

    bool operator==(const FGV2UiBindingHandle& Other) const
    {
        return Value == Other.Value;
    }

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI", meta = (AllowPrivateAccess = "true"))
    FString Value;
};

FORCEINLINE uint32 GetTypeHash(const FGV2UiBindingHandle& Handle)
{
    return GetTypeHash(Handle.ToString());
}

USTRUCT(BlueprintType)
struct GV2_API FGV2UiControlValue
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI")
    FName Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI")
    EGV2UiControlValueType Type = EGV2UiControlValueType::Null;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI")
    bool BooleanValue = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI")
    int64 IntegerValue = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI")
    double NumberValue = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI")
    FString StringValue;
};

USTRUCT(BlueprintType)
struct GV2_API FGV2TextViewModel
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Text")
    FText Text;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Text")
    FName StyleToken;

    // Prepared renderer markup. Produced only by UGV2TextPipeline.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Text")
    FString NormalizedMarkup;
};

USTRUCT(BlueprintType)
struct GV2_API FGV2ButtonViewModel
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI")
    FName Key;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI")
    FGV2TextViewModel Text;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI")
    FGV2UiBindingHandle Binding;
};

USTRUCT(BlueprintType)
struct GV2_API FGV2CheckboxViewModel
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI")
    FName Key;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI")
    FGV2TextViewModel Text;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI")
    bool bIsChecked = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI")
    FGV2UiBindingHandle Binding;
};

USTRUCT(BlueprintType)
struct GV2_API FGV2InputFieldViewModel
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI")
    FName Key;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI")
    FGV2TextViewModel Text;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI")
    FGV2TextViewModel PlaceholderText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI")
    FString TextValue;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI")
    FGV2UiBindingHandle Binding;
};

USTRUCT(BlueprintType)
struct GV2_API FGV2DropdownOptionViewModel
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI")
	FName Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI")
	FGV2TextViewModel Text;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI")
	bool bSelected = false;
};

USTRUCT(BlueprintType)
struct GV2_API FGV2DropdownSelectViewModel
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI")
	FGV2TextViewModel Placeholder;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI")
	TArray<FGV2DropdownOptionViewModel> Options;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI")
	FGV2UiBindingHandle Binding;
};

USTRUCT(BlueprintType)
struct GV2_API FGV2RichTextHoverViewModel
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Rich Text")
    FGV2TextViewModel Title;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Rich Text")
    FGV2TextViewModel Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Rich Text")
    FString ImageResourceId;

    bool IsEmpty() const
    {
        return Title.Text.IsEmpty() && Description.Text.IsEmpty() && ImageResourceId.IsEmpty();
    }
};

USTRUCT(BlueprintType)
struct GV2_API FGV2RichTextSpanViewModel
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Rich Text")
    FName SpanId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Rich Text")
    FName Key;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Rich Text")
    FGV2RichTextHoverViewModel Hover;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Rich Text")
    FGV2UiBindingHandle Binding;
};

USTRUCT(BlueprintType)
struct GV2_API FGV2InteractiveRichTextViewModel
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Rich Text", meta = (MultiLine = "true"))
    FGV2TextViewModel Text;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Rich Text")
    TArray<FGV2RichTextSpanViewModel> Spans;
};

USTRUCT(BlueprintType)
struct GV2_API FGV2ImageFieldViewModel
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Image")
    FString ResourceId;
};

USTRUCT(BlueprintType)
struct GV2_API FGV2ProgressBarViewModel
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|ProgressBar")
    float Percent = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|ProgressBar")
    FGV2TextViewModel Label;
};

USTRUCT(BlueprintType)
struct GV2_API FGV2PortraitViewModel
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Portrait")
    FString ResourceId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Portrait")
    FString FrameResourceId;
};

USTRUCT(BlueprintType)
struct GV2_API FGV2ModalViewModel
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Modal")
    FGV2TextViewModel Title;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Modal")
    FGV2TextViewModel Content;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Modal")
    TArray<FGV2ButtonViewModel> Buttons;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Modal")
    FGV2UiBindingHandle BackdropCloseBinding;
};

// TextSystem owns these composite values.  They deliberately carry only
// resolved text, resource IDs and opaque semantic bindings; layout remains in
// the LocationScreen Widget Blueprint.
USTRUCT(BlueprintType)
struct GV2_API FGV2LocationTopBarViewModel
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Location") FGV2TextViewModel Day;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Location") FGV2TextViewModel Location;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Location") FGV2TextViewModel PrimaryResource;
};

USTRUCT(BlueprintType)
struct GV2_API FGV2LocationCharacterEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Location")
    FName Key;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Location")
    FString ResourceId;
};

USTRUCT(BlueprintType)
struct GV2_API FGV2LocationMeterEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Location")
    FName Key;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Location")
    FGV2ProgressBarViewModel Meter;
};

USTRUCT(BlueprintType)
struct GV2_API FGV2LocationPlayerStatusViewModel
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Location") FString PortraitResourceId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Location") FGV2TextViewModel Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Location") TArray<FGV2LocationMeterEntry> Meters;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Location") TArray<FString> ItemIconResourceIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Location") TArray<FString> EffectIconResourceIds;
};

USTRUCT(BlueprintType)
struct GV2_API FGV2LocationSceneViewModel
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Location") FString BackgroundTileResourceId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Location") FString BackgroundResourceId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Location") TArray<FGV2LocationCharacterEntry> Characters;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Location") FGV2TextViewModel ContextText;
};

USTRUCT(BlueprintType)
struct GV2_API FGV2ScreenFieldDescriptor
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Screen")
    FName FieldId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Screen")
    FString SchemaId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Screen")
    bool bRequired = true;

    bool IsConfigured() const
    {
        return !FieldId.IsNone();
    }
};

struct FGV2TabContainerViewModel;

USTRUCT(BlueprintType)
struct GV2_API FGV2ScreenFieldValue
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Screen")
    FName FieldId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Screen")
    FString SchemaId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Screen")
    FGV2InteractiveRichTextViewModel InteractiveRichTextValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Screen")
    TArray<FGV2ButtonViewModel> ButtonListValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Screen")
    FGV2CheckboxViewModel CheckboxValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Screen")
    FGV2InputFieldViewModel InputFieldValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Screen")
    FGV2DropdownSelectViewModel DropdownSelectValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Screen")
    FGV2ImageFieldViewModel ImageValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Screen")
    FGV2ProgressBarViewModel ProgressBarValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Screen")
    FGV2PortraitViewModel PortraitValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Screen")
    FGV2ModalViewModel ModalValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Screen")
    FGV2LocationTopBarViewModel LocationTopBarValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Screen")
    FGV2LocationPlayerStatusViewModel LocationPlayerStatusValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Screen")
    FGV2LocationSceneViewModel LocationSceneValue;

    TSharedPtr<FGV2TabContainerViewModel> TabContainerValue;

    static FGV2ScreenFieldValue MakeButtonList(
        const FName InFieldId,
        const TArray<FGV2ButtonViewModel>& InValue)
    {
        FGV2ScreenFieldValue Value;
        Value.FieldId = InFieldId;
        Value.SchemaId = TEXT("core:schema.ui_field.button_list.v2");
        Value.ButtonListValue = InValue;
        return Value;
    }

    static FGV2ScreenFieldValue MakeInteractiveRichText(
        const FName InFieldId,
        const FGV2InteractiveRichTextViewModel& InValue)
    {
        FGV2ScreenFieldValue Value;
        Value.FieldId = InFieldId;
        Value.SchemaId = TEXT("core:schema.ui_field.rich_text.v3");
        Value.InteractiveRichTextValue = InValue;
        return Value;
    }

    static FGV2ScreenFieldValue MakeCheckbox(
        const FName InFieldId,
        const FGV2CheckboxViewModel& InValue)
    {
        FGV2ScreenFieldValue Value;
        Value.FieldId = InFieldId;
        Value.SchemaId = TEXT("core:schema.ui_field.checkbox.v1");
        Value.CheckboxValue = InValue;
        return Value;
    }

    static FGV2ScreenFieldValue MakeInputField(
        const FName InFieldId,
        const FGV2InputFieldViewModel& InValue)
    {
        FGV2ScreenFieldValue Value;
        Value.FieldId = InFieldId;
        Value.SchemaId = TEXT("core:schema.ui_field.input_field.v1");
        Value.InputFieldValue = InValue;
        return Value;
    }

    static FGV2ScreenFieldValue MakeDropdownSelect(
        const FName InFieldId,
        const FGV2DropdownSelectViewModel& InValue)
    {
        FGV2ScreenFieldValue Value;
        Value.FieldId = InFieldId;
        Value.SchemaId = TEXT("core:schema.ui_field.dropdown_select.v1");
        Value.DropdownSelectValue = InValue;
        return Value;
    }

    static FGV2ScreenFieldValue MakeImage(
        const FName InFieldId,
        const FGV2ImageFieldViewModel& InValue)
    {
        FGV2ScreenFieldValue Value;
        Value.FieldId = InFieldId;
        Value.SchemaId = TEXT("core:schema.ui_field.image.v1");
        Value.ImageValue = InValue;
        return Value;
    }

    static FGV2ScreenFieldValue MakeProgressBar(
        const FName InFieldId,
        const FGV2ProgressBarViewModel& InValue)
    {
        FGV2ScreenFieldValue Value;
        Value.FieldId = InFieldId;
        Value.SchemaId = TEXT("core:schema.ui_field.progress_bar.v1");
        Value.ProgressBarValue = InValue;
        return Value;
    }

    static FGV2ScreenFieldValue MakePortrait(
        const FName InFieldId,
        const FGV2PortraitViewModel& InValue)
    {
        FGV2ScreenFieldValue Value;
        Value.FieldId = InFieldId;
        Value.SchemaId = TEXT("core:schema.ui_field.portrait.v1");
        Value.PortraitValue = InValue;
        return Value;
    }

    static FGV2ScreenFieldValue MakeModal(
        const FName InFieldId,
        const FGV2ModalViewModel& InValue)
    {
        FGV2ScreenFieldValue Value;
        Value.FieldId = InFieldId;
        Value.SchemaId = TEXT("core:schema.ui_field.modal.v1");
        Value.ModalValue = InValue;
        return Value;
    }

    static FGV2ScreenFieldValue MakeLocationTopBar(const FName Id, const FGV2LocationTopBarViewModel& Model)
    { FGV2ScreenFieldValue Value; Value.FieldId = Id; Value.SchemaId = TEXT("textsystem:schema.ui_field.location_top_bar.v1"); Value.LocationTopBarValue = Model; return Value; }
    static FGV2ScreenFieldValue MakeLocationPlayerStatus(const FName Id, const FGV2LocationPlayerStatusViewModel& Model)
    { FGV2ScreenFieldValue Value; Value.FieldId = Id; Value.SchemaId = TEXT("textsystem:schema.ui_field.location_player_status.v1"); Value.LocationPlayerStatusValue = Model; return Value; }
    static FGV2ScreenFieldValue MakeLocationScene(const FName Id, const FGV2LocationSceneViewModel& Model)
    { FGV2ScreenFieldValue Value; Value.FieldId = Id; Value.SchemaId = TEXT("textsystem:schema.ui_field.location_scene.v1"); Value.LocationSceneValue = Model; return Value; }
    static FGV2ScreenFieldValue MakeLocationCommands(const FName Id, const TArray<FGV2ButtonViewModel>& Model)
    { FGV2ScreenFieldValue Value; Value.FieldId = Id; Value.SchemaId = TEXT("textsystem:schema.ui_field.location_commands.v1"); Value.ButtonListValue = Model; return Value; }

    static FGV2ScreenFieldValue MakeTabContainer(
        const FName InFieldId,
        const FGV2TabContainerViewModel& InValue);
};

USTRUCT(BlueprintType)
struct GV2_API FGV2TabItemViewModel
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Tabs")
    FName Key;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Tabs")
    FGV2TextViewModel Title;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Tabs")
    FString ScreenId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Tabs")
    TArray<FGV2ScreenFieldValue> Fields;
};

USTRUCT(BlueprintType)
struct GV2_API FGV2TabContainerViewModel
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Tabs")
    FName DefaultTabKey;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Tabs")
    TArray<FGV2TabItemViewModel> Tabs;
};

inline FGV2ScreenFieldValue FGV2ScreenFieldValue::MakeTabContainer(
    const FName InFieldId,
    const FGV2TabContainerViewModel& InValue)
{
    FGV2ScreenFieldValue Value;
    Value.FieldId = InFieldId;
    Value.SchemaId = TEXT("core:schema.ui_field.tab_container.v1");
    Value.TabContainerValue = MakeShared<FGV2TabContainerViewModel>(InValue);
    return Value;
}

USTRUCT(BlueprintType)
struct GV2_API FGV2ScreenViewModel
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Screen")
    FString ScreenId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Screen")
    TArray<FGV2ScreenFieldValue> Fields;
};

USTRUCT(BlueprintType)
struct GV2_API FGV2ScreenInstanceViewModel
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Screen")
    FName Layer = TEXT("location_content");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Screen")
    FName InstanceKey = TEXT("main");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Screen")
    FString ScreenId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Screen")
    TArray<FGV2ScreenFieldValue> Fields;
};

USTRUCT(BlueprintType)
struct GV2_API FGV2UiDocumentViewModel
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Document")
    FString UiInstanceId = TEXT("ui@default");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Document")
    int64 Revision = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Document")
    bool bHasRoute = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Document")
    FGV2ScreenInstanceViewModel Route;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Document")
    TArray<FGV2ScreenInstanceViewModel> Overlays;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Document")
    TArray<FGV2ScreenInstanceViewModel> Modals;

    TArray<FGV2ScreenInstanceViewModel> GetAllScreenInstances() const
    {
        TArray<FGV2ScreenInstanceViewModel> Result;
        if (bHasRoute)
        {
            Result.Add(Route);
        }
        Result.Append(Overlays);
        Result.Append(Modals);
        return Result;
    }
};

USTRUCT(BlueprintType)
struct GV2_API FGV2SessionStatus
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|Runtime")
    EGV2ApplicationState ApplicationState = EGV2ApplicationState::Uninitialized;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|Runtime")
    EGV2SessionState SessionState = EGV2SessionState::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|Runtime")
    bool bIsReady = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|Runtime")
    int32 SessionGeneration = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|Runtime")
    int64 RepositoryVersion = 0;
};
