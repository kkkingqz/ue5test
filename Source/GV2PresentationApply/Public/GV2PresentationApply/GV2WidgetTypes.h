#pragma once

#include "CoreMinimal.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"
#include "Curves/CurveFloat.h"
#include "Styling/SlateTypes.h"

class UCommonTextStyle;
class UUserWidget;

#include "GV2WidgetTypes.generated.h"

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

USTRUCT(BlueprintType)
struct GV2PRESENTATIONAPPLY_API FGV2UiBindingHandle
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
struct GV2PRESENTATIONAPPLY_API FGV2UiControlValue
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
struct GV2PRESENTATIONAPPLY_API FGV2TextViewModel
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

// PEP-05 (ADR-0040): hover content is a nested screen, not a fixed Title/Description/
// ImageResourceId triple -- the same NestedScreen route FGV2TabItemEntry already uses
// for tab content (GV2TabContainerWidgetBase.h). ScreenId names what was resolved;
// ScreenWidget is the off-tree instance the owner's Prepare already built and styled
// (PSC-10B: a popover opening this hover never resolves or styles anything itself, it
// only asks for the reference and attaches it).
USTRUCT(BlueprintType)
struct GV2PRESENTATIONAPPLY_API FGV2RichTextHoverViewModel
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Rich Text")
    FString ScreenId;

    UPROPERTY(Transient)
    TWeakObjectPtr<UUserWidget> ScreenWidget;

    bool IsEmpty() const
    {
        return ScreenId.IsEmpty();
    }
};

USTRUCT(BlueprintType)
struct GV2PRESENTATIONAPPLY_API FGV2RichTextSpanViewModel
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
