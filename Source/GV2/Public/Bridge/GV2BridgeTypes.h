#pragma once

#include "CoreMinimal.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"
#include "Curves/CurveFloat.h"
#include "Styling/SlateTypes.h"

#include <memory>

namespace GV2ContentCore { struct FCompiledUiFieldSpec; }
class FGV2PreparedUiObject;
class UCommonTextStyle;

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

    // Resolved presentation populated during semantic Prepare from the pinned session
    // snapshot. Apply()/ApplyRichText() reject a value without it and perform no Theme or
    // token lookup. This is derived presentation data and is excluded from operator==.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Text")
    bool bHasResolvedPresentation = false;

    // PSC-11: the single inflation of a prepared text value into this view model. It lived
    // in GV2LegacyPresentationApplyAdapter while that file was the second apply entry point;
    // with the dispatch moved below the boundary, each value sink needs it, and one
    // implementation is what keeps them from drifting.
    static FGV2TextViewModel FromPrepared(const GV2PresentationApply::FPreparedTextValue& Value);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Text")
    TSubclassOf<UCommonTextStyle> ResolvedStyleClass;

    // GetEffectiveFontSize's own UnscaledSize -- resolved once from Theme's
    // TextSizeTokens/TextStyleTokens/hardcoded fallback, still needs live viewport height
    // (only known where the widget actually renders) to become a final font size.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Text")
    float ResolvedBaseFontSize = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Text")
    float ResolvedMinReadableFontSize = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Text")
    float ResolvedReferenceViewportHeight = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Text")
    FRuntimeFloatCurve ResolvedFontScaleCurve;

    // RichText-only: the resolved default text style, WITHOUT a font size baked in --
    // Apply computes the scaled size itself from the fields above plus live geometry.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Text")
    bool bHasResolvedDefaultStyle = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Text")
    FTextBlockStyle ResolvedDefaultStyle;

    bool operator==(const FGV2TextViewModel& Other) const
    {
        return StyleToken == Other.StyleToken
            && NormalizedMarkup == Other.NormalizedMarkup
            && Text.EqualTo(Other.Text);
    }

    bool operator!=(const FGV2TextViewModel& Other) const
    {
        return !(*this == Other);
    }
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

    // PSC-10B: transient value prepared from ImageResourceId through the session
    // snapshot. Tooltip opening applies this brush directly and never reaches a catalog.
    UPROPERTY(Transient)
    FSlateBrush ResolvedImageBrush;

    UPROPERTY(Transient)
    bool bHasResolvedImage = false;

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
struct GV2_API FGV2ScreenFieldValue
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Screen")
    FName FieldId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Screen")
    FString SchemaId;

    // UPP-27: materialized candidate value and its compiled schema, built once by
    // GV2ScreenFieldMaterializer::BuildFields from the raw Lua value and consumed by
    // UGV2ScreenWidgetBase's Prepare/Commit. Not UPROPERTY -- neither type is
    // UHT-reflectable, and this payload is transient view-model data, never
    // saved/replicated/Blueprint-authored the way FieldId/SchemaId above are.
    TSharedPtr<const FGV2PreparedUiObject> PreparedValue;
    std::shared_ptr<const GV2ContentCore::FCompiledUiFieldSpec> CompiledSchema;
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
