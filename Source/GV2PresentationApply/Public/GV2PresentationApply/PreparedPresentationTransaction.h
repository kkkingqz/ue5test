#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

class UImage;
class UWidget;
class UCheckBox;
class UEditableTextBox;
class UProgressBar;

namespace GV2PresentationApply
{
// PSC-09A (ADR-0043 D3): a single already-resolved image mutation. Brush is the FINAL
// Slate brush -- Prepare (upper GV2) already validated scale-policy compatibility and
// baked Tiling/DrawAs for the target's scale policy, so Apply performs no decision, only
// widget mutation. No authority type is reachable from this struct's own module: this
// header's module (GV2PresentationApply.Build.cs) denies GV2, GV2ContentCore,
// GV2ContentHostSupport, GV2RuntimeCore, DeveloperSettings, AssetRegistry and ImageCore
// outright, so there is nothing here to dereference back into a resolver even if a field
// were added carelessly later.
struct GV2PRESENTATIONAPPLY_API FPreparedImageResourceOperation
{
    TWeakObjectPtr<UImage> TargetWidget;
    FSlateBrush Brush;
};

// PSC-09B: canonical lower replacement for GV2's own FGV2ResolvedImageResource/
// EGV2ImageRenderMode (GV2ImageResourceCatalog.h, USTRUCT/UENUM(BlueprintType) --
// UGV2ImageWidgetBase::ApplyResolvedImageResource/UGV2PortraitWidgetBase::
// ApplyResolvedPortrait take the GV2 type directly as a UFUNCTION(BlueprintCallable)
// parameter, so it stays GV2-owned; this is a distinct type built from the same
// resolved fields via plain Core types only, not a copy or alias of it.
enum class EPreparedImageRenderMode : uint8
{
    FixedAspect,
    NineSlice,
    Tile,
};

struct GV2PRESENTATIONAPPLY_API FPreparedResolvedImageValue
{
    FString ResourceId;
    EPreparedImageRenderMode RenderMode = EPreparedImageRenderMode::FixedAspect;
    float FixedAspectRatio = 0.0f;
    FSlateBrush Brush;
};

// PSC-09B: UGV2ImageWidgetBase/UGV2PortraitWidgetBase are GV2-owned -- entirely
// legacy-adapter territory, which also owns their own reset-specific behavior
// (UGV2PortraitWidgetBase additionally collapses visibility on reset; both reach past
// their own Apply UFUNCTION into their inner UImage directly for reset, the same way
// Commit() always did before this operation existed).
struct GV2PRESENTATIONAPPLY_API FPreparedImageHostOperation
{
    TWeakObjectPtr<UWidget> TargetWidget;
    FPreparedResolvedImageValue Resolved;
    bool bResetToDefault = false;
};

// PSC-09B (ADR-0043 D2/D3): Prepare (upper GV2) has already narrowed a boolean
// capability down to exactly which widget-native setter applies -- Apply performs no
// decision, only the cast+call for the setter it recognizes. RequiresLegacyAdapter marks
// an operation whose real target needs a GV2-owned widget/interface type (e.g.
// UGV2DropdownSelectWidgetBase::SetDropdownOpen) that this module cannot Cast to by
// construction (Build.cs denylist) -- GV2's own temporary GV2LegacyPresentationApplyAdapter
// finishes those, reading the exact same operation, not a second resolution.
enum class EPreparedBooleanTarget : uint8
{
    WidgetEnabled,
    CheckBoxChecked,
    EditableTextReadOnly,
    RequiresLegacyAdapter,
};

struct GV2PRESENTATIONAPPLY_API FPreparedBooleanOperation
{
    TWeakObjectPtr<UWidget> TargetWidget;
    EPreparedBooleanTarget Target = EPreparedBooleanTarget::WidgetEnabled;
    bool Value = false;
    // Carries no meaning to Apply() -- it never branches on it. Exists only so the
    // legacy adapter (which DOES need it, e.g. "is_open") can read it back without a
    // second lookup into whatever originally produced this operation.
    FName PropertyName;
};

// PSC-09B: UEditableTextBox is a plain UMG type -- Apply can set its text directly.
// Integer/String's OWN semantic meaning (max-length enforcement, truncation) is entirely
// UGV2InputFieldWidgetBase-owned, so those two kinds' own operations (below) still route
// through the legacy adapter for that part; this struct only covers the raw SetText call
// itself, when nothing GV2-specific stands between the resolved value and the widget.
struct GV2PRESENTATIONAPPLY_API FPreparedEditableTextValueOperation
{
    TWeakObjectPtr<UEditableTextBox> TargetWidget;
    FText Value;
};

// PSC-09B: UProgressBar is a plain UMG type -- Apply can set its percent directly.
// UGV2ProgressBarWidgetBase (the OTHER Number target) is GV2-owned and routes through
// the legacy adapter instead (see FPreparedNumberOperation below).
struct GV2PRESENTATIONAPPLY_API FPreparedProgressBarOperation
{
    TWeakObjectPtr<UProgressBar> TargetWidget;
    float Percent = 0.0f;
};

// PSC-09B: the OTHER Number target, UGV2ProgressBarWidgetBase, is GV2-owned -- entirely
// legacy-adapter territory (see FPreparedProgressBarOperation above for the plain
// UProgressBar case this module DOES apply directly).
struct GV2PRESENTATIONAPPLY_API FPreparedNumberOperation
{
    TWeakObjectPtr<UWidget> TargetWidget;
    double Value = 0.0;
};

// PSC-09B: every real Integer target (UGV2InputFieldWidgetBase's max-length enforcement)
// is GV2-owned -- this module has nothing to Cast to, so it only carries the resolved
// value for the legacy adapter. TargetWidget stays generic (UWidget) specifically so
// this struct's own declaration never needs a GV2 type.
struct GV2PRESENTATIONAPPLY_API FPreparedIntegerOperation
{
    TWeakObjectPtr<UWidget> TargetWidget;
    int64 Value = 0;
};

// PSC-09B: String's own truncation-against-max-length behavior is
// UGV2InputFieldWidgetBase-owned; the plain SetText part is also folded into the legacy
// adapter for this kind (not split across two operations) since the truncation decision
// and the SetText call are the same one step in the existing Commit -- splitting them
// across a module boundary here would add complexity with no read-only value.
struct GV2PRESENTATIONAPPLY_API FPreparedStringOperation
{
    TWeakObjectPtr<UWidget> TargetWidget;
    FString Value;
};

// PSC-09B: every real Key target (UGV2DropdownSelectWidgetBase::SetSelectedKey,
// UGV2TabContainerWidgetBase::ApplyDefaultTabKey, IGV2UiPropertyHost::SetKey) is
// GV2-owned or a GV2-owned interface -- entirely legacy-adapter territory.
struct GV2PRESENTATIONAPPLY_API FPreparedKeyOperation
{
    TWeakObjectPtr<UWidget> TargetWidget;
    FName PropertyName;
    FString Value;
};

// PSC-09B: IGV2UiBindingTarget is a GV2-owned interface -- entirely legacy-adapter
// territory. SerializedHandle is FGV2UiBindingHandle::ToString()'s own value (a plain
// FString, the handle's only field) rather than the USTRUCT itself, so this struct's
// declaration never needs GV2's own Bridge/GV2BridgeTypes.h.
struct GV2PRESENTATIONAPPLY_API FPreparedBindingOperation
{
    TWeakObjectPtr<UWidget> TargetWidget;
    FString SerializedHandle;
};

// PSC-09B: canonical lower replacement for GV2's own FGV2RichTextHoverViewModel/
// FGV2RichTextSpanViewModel USTRUCTs (Bridge/GV2BridgeTypes.h) -- not a copy of them or
// a compatibility alias, a distinct type this module owns, built from the same resolved
// fields via plain Core types only. UGV2RichTextWidgetBase::ApplySpans is entirely
// GV2-owned, so GV2LegacyPresentationApplyAdapter reconstructs the USTRUCT array from
// this before calling it; GV2PresentationApply::Apply() has nothing to do for this
// operation, same as Key/Binding. SerializedBinding mirrors FPreparedBindingOperation's
// own flattening of FGV2UiBindingHandle.
struct GV2PRESENTATIONAPPLY_API FPreparedRichTextHover
{
    FText Title;
    FText Description;
    FString ImageResourceId;
};

struct GV2PRESENTATIONAPPLY_API FPreparedRichTextSpan
{
    FName SpanId;
    FName Key;
    FPreparedRichTextHover Hover;
    FString SerializedBinding;
};

struct GV2PRESENTATIONAPPLY_API FPreparedRichTextSpansOperation
{
    TWeakObjectPtr<UWidget> TargetWidget;
    TArray<FPreparedRichTextSpan> Spans;
};

// PSC-09A/09B (ADR-0043 D3): immutable once built -- an upper GV2 preparer appends
// operations, then hands the finished transaction to Apply() below; nothing mutates it
// afterward, and it carries no PrepareContext, snapshot, or resolver of any kind.
class GV2PRESENTATIONAPPLY_API FGV2PreparedPresentationTransaction
{
public:
    void AddImageResourceOperation(FPreparedImageResourceOperation Operation)
    {
        ImageResourceOperations.Add(MoveTemp(Operation));
    }
    void AddImageHostOperation(FPreparedImageHostOperation Operation)
    {
        ImageHostOperations.Add(MoveTemp(Operation));
    }
    void AddBooleanOperation(FPreparedBooleanOperation Operation)
    {
        BooleanOperations.Add(MoveTemp(Operation));
    }
    void AddEditableTextValueOperation(FPreparedEditableTextValueOperation Operation)
    {
        EditableTextValueOperations.Add(MoveTemp(Operation));
    }
    void AddProgressBarOperation(FPreparedProgressBarOperation Operation)
    {
        ProgressBarOperations.Add(MoveTemp(Operation));
    }
    void AddNumberOperation(FPreparedNumberOperation Operation)
    {
        NumberOperations.Add(MoveTemp(Operation));
    }
    void AddIntegerOperation(FPreparedIntegerOperation Operation)
    {
        IntegerOperations.Add(MoveTemp(Operation));
    }
    void AddStringOperation(FPreparedStringOperation Operation)
    {
        StringOperations.Add(MoveTemp(Operation));
    }
    void AddKeyOperation(FPreparedKeyOperation Operation)
    {
        KeyOperations.Add(MoveTemp(Operation));
    }
    void AddBindingOperation(FPreparedBindingOperation Operation)
    {
        BindingOperations.Add(MoveTemp(Operation));
    }
    void AddRichTextSpansOperation(FPreparedRichTextSpansOperation Operation)
    {
        RichTextSpansOperations.Add(MoveTemp(Operation));
    }

    const TArray<FPreparedImageResourceOperation>& GetImageResourceOperations() const { return ImageResourceOperations; }
    const TArray<FPreparedImageHostOperation>& GetImageHostOperations() const { return ImageHostOperations; }
    const TArray<FPreparedBooleanOperation>& GetBooleanOperations() const { return BooleanOperations; }
    const TArray<FPreparedEditableTextValueOperation>& GetEditableTextValueOperations() const { return EditableTextValueOperations; }
    const TArray<FPreparedProgressBarOperation>& GetProgressBarOperations() const { return ProgressBarOperations; }
    const TArray<FPreparedNumberOperation>& GetNumberOperations() const { return NumberOperations; }
    const TArray<FPreparedIntegerOperation>& GetIntegerOperations() const { return IntegerOperations; }
    const TArray<FPreparedStringOperation>& GetStringOperations() const { return StringOperations; }
    const TArray<FPreparedKeyOperation>& GetKeyOperations() const { return KeyOperations; }
    const TArray<FPreparedBindingOperation>& GetBindingOperations() const { return BindingOperations; }
    const TArray<FPreparedRichTextSpansOperation>& GetRichTextSpansOperations() const { return RichTextSpansOperations; }

    bool IsEmpty() const
    {
        return ImageResourceOperations.IsEmpty()
            && ImageHostOperations.IsEmpty()
            && BooleanOperations.IsEmpty()
            && EditableTextValueOperations.IsEmpty()
            && ProgressBarOperations.IsEmpty()
            && NumberOperations.IsEmpty()
            && IntegerOperations.IsEmpty()
            && StringOperations.IsEmpty()
            && KeyOperations.IsEmpty()
            && BindingOperations.IsEmpty()
            && RichTextSpansOperations.IsEmpty();
    }

private:
    TArray<FPreparedImageResourceOperation> ImageResourceOperations;
    TArray<FPreparedImageHostOperation> ImageHostOperations;
    TArray<FPreparedBooleanOperation> BooleanOperations;
    TArray<FPreparedEditableTextValueOperation> EditableTextValueOperations;
    TArray<FPreparedProgressBarOperation> ProgressBarOperations;
    TArray<FPreparedNumberOperation> NumberOperations;
    TArray<FPreparedIntegerOperation> IntegerOperations;
    TArray<FPreparedStringOperation> StringOperations;
    TArray<FPreparedKeyOperation> KeyOperations;
    TArray<FPreparedBindingOperation> BindingOperations;
    TArray<FPreparedRichTextSpansOperation> RichTextSpansOperations;
};

// PSC-09A (ADR-0043 D2): the only public entry point physical Apply exposes -- there is
// no second path into this module's mutation logic. Performs every operation this
// module recognizes (plain Engine/UMG target types) and nothing else: no lookup, no
// resolution, no fallback to a value this transaction didn't already carry. An operation
// whose real target needs a GV2-owned type (EPreparedBooleanTarget::RequiresLegacyAdapter,
// or any Integer/String/Key/Binding operation, all of which are GV2-interface-only by
// construction) is silently left for GV2's own temporary
// GV2LegacyPresentationApplyAdapter -- not an error here, since this module has no way
// to tell "a GV2-owned target" apart from "a genuinely unsupported one" without the type
// it is explicitly denied from ever depending on.
GV2PRESENTATIONAPPLY_API bool Apply(const FGV2PreparedPresentationTransaction& Transaction, FString& OutError);
}
