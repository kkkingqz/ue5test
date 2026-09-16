#pragma once

#include "Blueprint/UserWidget.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Misc/TVariant.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

class UImage;
class UWidget;
class UCheckBox;
class UCommonRichTextBlock;
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
// decision, only the cast+call for the setter it recognizes. HostDeclaredBoolean marks
// an operation whose real target needs a GV2-owned widget/interface type (e.g.
// UGV2DropdownSelectWidgetBase::SetDropdownOpen) that this module cannot Cast to by
// construction (Build.cs denylist) -- PSC-11 reaches those through the value-only role
// IGV2PreparedBooleanTarget, from this same operation, not a second resolution.
enum class EPreparedBooleanTarget : uint8
{
    WidgetEnabled,
    CheckBoxChecked,
    EditableTextReadOnly,
    HostDeclaredBoolean,
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

// PSC-09B: canonical lower replacement for GV2's own FGV2TextViewModel
// (Bridge/GV2BridgeTypes.h, USTRUCT(BlueprintType)) -- built from the same resolved
// fields via plain Core types only. Every real Text target is either a GV2-owned widget
// wrapper (UGV2TextWidgetBase, UGV2ButtonWidgetBase, UGV2DropdownSelectWidgetBase,
// UGV2RichTextWidgetBase -- each with its own bookkeeping, e.g. CurrentContent, that a
// direct SetText would leave stale) or reached only via GV2's own UGV2TextPipeline UCLASS
// (a GV2-owned type, however plain the widgets it ultimately touches are). PSC-11 reaches
// each of those through IGV2PreparedTextTarget, whose implementation reconstructs the
// USTRUCT from this value (FGV2TextViewModel::FromPrepared, one implementation) and
// performs no Theme/token lookup; a plain CommonUI renderer is written here directly.
struct GV2PRESENTATIONAPPLY_API FPreparedTextValue
{
    FText Text;
    FName StyleToken;
    FString NormalizedMarkup;
    TSubclassOf<UCommonTextStyle> ResolvedStyleClass;
    float ResolvedBaseFontSize = 0.0f;
    float ResolvedMinReadableFontSize = 0.0f;
    float ResolvedReferenceViewportHeight = 0.0f;
    FRuntimeFloatCurve ResolvedFontScaleCurve;
    FTextBlockStyle ResolvedDefaultStyle;
    bool bHasResolvedPresentation = false;
    bool bHasResolvedDefaultStyle = false;
};

// PEP-05 (ADR-0040): canonical lower replacement for the hover/span USTRUCT. Hover
// content used to be a fixed Title/Description/ImageResourceId triple -- exactly the
// schema-specific DTO ADR-0040 abolished everywhere else. It is now a nested screen,
// prepared and instantiated off-tree the same way FPreparedTabEntry's ScreenWidget is:
// ScreenId names what was resolved, ScreenWidget is the already-prepared, already-styled
// widget instance. Opening a popover performs no resolution of its own (PSC-10B) -- it
// only asks for this reference and attaches it.
struct GV2PRESENTATIONAPPLY_API FPreparedRichTextHover
{
    FString ScreenId;
    TWeakObjectPtr<UWidget> ScreenWidget;
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

// PSC-09B: the widget-touching tail of FGV2KeyedCollectionPropertyConsumer::
// CommitWithFailureInjector()/Reset() -- panel child-list reconciliation, active-widget
// map publication and (for a Dropdown-owned collection) header
// label refresh. The recursive per-item CommitUiHostProperties() calls that precede this
// (each item's OWN capabilities, applied through their own already-migrated leaf
// consumers) and the pure bookkeeping SetLastCommittedSnapshot() calls that follow it
// (FGV2UiPropertyHostState accounting, no widget touched) are NOT part of this operation
// -- neither reaches a content/authority type, and neither is itself a widget mutation
// this module could apply; UPanelWidget::ClearChildren/AddChild is the one genuinely
// shared, plain-UMG step, and PSC-11 performs it here. The parts that are GV2-owned --
// which panel holds the children, and what bookkeeping settles afterwards -- are asked for
// through IGV2PreparedKeyedCollectionTarget, so the target says WHERE and WHAT SETTLES
// while the rebuild itself stays below the boundary.
struct GV2PRESENTATIONAPPLY_API FPreparedKeyedCollectionEntry
{
    FName Key;
    TWeakObjectPtr<UWidget> Widget;
};

struct GV2PRESENTATIONAPPLY_API FPreparedKeyedCollectionOperation
{
    TWeakObjectPtr<UWidget> TargetWidget;
    TArray<FPreparedKeyedCollectionEntry> OrderedEntries;
    bool bIsReset = false;
};

// PSC-09B: canonical lower replacement for GV2's own FGV2TabItemEntry
// (GV2TabContainerWidgetBase.h, USTRUCT(BlueprintType)) -- UGV2TabContainerWidgetBase::
// ApplyTabEntries/ResetTabContainerModel are GV2-owned, so PSC-11 delivers this flattened
// form through IGV2PreparedTabContainerTarget and the host reconstructs the USTRUCT array
// and TMap in its own sink; this is the sole physical
// mutation FGV2TabContainerTabsPropertyConsumer's own Commit()/Reset() performs directly
// -- per-tab nested-screen field commits (CommitScreenFields, DUC-09) are unchanged,
// applied through each tab's own screen the same way a top-level screen is, and neither
// reach a content/authority type nor are themselves this consumer's own mutation.
struct GV2PRESENTATIONAPPLY_API FPreparedTabEntry
{
    FName Key;
    FPreparedTextValue Title;
    FString ScreenId;
    TWeakObjectPtr<UWidget> ScreenWidget;
};

struct GV2PRESENTATIONAPPLY_API FPreparedTabContainerOperation
{
    TWeakObjectPtr<UWidget> TargetWidget;
    TArray<FPreparedTabEntry> Entries;
    bool bIsReset = false;
};

struct GV2PRESENTATIONAPPLY_API FPreparedTextOperation
{
    TWeakObjectPtr<UWidget> TargetWidget;
    FPreparedTextValue Value;
    // Selects between two ORIGINALLY DIFFERENT UGV2RichTextWidgetBase redirect rules the
    // pre-transaction Commit()/Reset() each had (Commit redirected to the inner
    // RichTextBlock when present; Reset did not) -- not a general-purpose flag, only the
    // text dispatch in the Apply facade reads it, to reproduce that exact asymmetry.
    bool bIsReset = false;
};

// PSC-10A (ADR-0043 D3): resolved once in Prepare from Theme (BaseFontSize is the
// already-resolved UnscaledSize for a style/size token; MinReadableFontSize/
// ReferenceViewportHeight/ScaleCurve are copied straight from the resolved Theme) --
// EvaluatePreparedFontSize() below is a pure function of this policy and CURRENT
// geometry, read live by Apply itself. No Theme object or token lookup happens here.
struct GV2PRESENTATIONAPPLY_API FPreparedTextScalePolicy
{
    float BaseFontSize = 14.0f;
    float MinReadableFontSize = 10.0f;
    float ReferenceViewportHeight = 1080.0f;
    FRuntimeFloatCurve ScaleCurve;

};

// Mirrors UGV2UiTheme::EvaluateTextScale + GetEffectiveFontSize's own math exactly, as a
// free function with no Theme/UObject dependency -- the curve/fallback-lerp evaluation
// itself needs no authority, only the already-resolved policy plus current geometry.
GV2PRESENTATIONAPPLY_API float EvaluatePreparedFontSize(const FPreparedTextScalePolicy& Policy, float ViewportHeight);

// PSC-10B (ADR-0035, DCA-15): the viewport-derived scale factor on its own, for a prepared
// operation that scales something other than a font -- a dropdown's popup box follows the
// same curve as the option text inside it, or a fixed max height would show fewer options
// as the screen grows. Same curve, same breakpoints, one implementation:
// EvaluatePreparedFontSize is defined in terms of this.
struct GV2PRESENTATIONAPPLY_API FPreparedViewportScalePolicy
{
    FRuntimeFloatCurve ScaleCurve;
    float ReferenceViewportHeight = 1080.0f;
};

GV2PRESENTATIONAPPLY_API float EvaluatePreparedViewportScale(const FPreparedViewportScalePolicy& Policy, float ViewportHeight);

// Live viewport query (GEngine->GameViewport, falling back to ContextWidget's own
// world) -- the same lookup UGV2TextPipeline::GetViewportHeight used to perform, minus
// the Theme-sourced fallback (now a parameter, itself resolved in Prepare).
GV2PRESENTATIONAPPLY_API float ResolveLiveViewportHeight(const UWidget* ContextWidget, float FallbackHeight);

// PSC-09B/10A (ADR-0043 D2/D3, PAH-R1): UGV2TextPipeline::Apply's own widget mutation --
// unlike the FPreparedTextOperation family above (built for widget wrappers/UGV2TextPipeline
// itself as an opaque call), this is UGV2TextPipeline's OWN internals split across the
// module boundary: Style/ScalePolicy are already resolved (upper, from Theme, PSC-10A),
// so Apply performs no decision, only the CommonUI/UMG-native SetStyle/SetText/SetFont
// calls plus the scale-policy pure-function evaluation against live geometry.
struct GV2PRESENTATIONAPPLY_API FPreparedPlainTextOperation
{
    TWeakObjectPtr<UCommonTextBlock> TargetWidget;
    TSubclassOf<UCommonTextStyle> Style;
    FText Text;
    FPreparedTextScalePolicy ScalePolicy;
};

// PSC-09B/10A: UGV2TextPipeline::ApplyRichText's own widget mutation, split the same
// way. ScalePolicy always contains unscaled values prepared from the session Theme.
struct GV2PRESENTATIONAPPLY_API FPreparedRichTextRenderOperation
{
    TWeakObjectPtr<UCommonRichTextBlock> TargetWidget;
    TSubclassOf<UCommonTextStyle> Style;
    FTextBlockStyle DefaultStyle;
    bool bHasDefaultStyle = false;
    FString Markup;
    FPreparedTextScalePolicy ScalePolicy;
};

// PSC-09B: UGV2TextPipeline::ApplyHint's own widget mutation, split the same way.
struct GV2PRESENTATIONAPPLY_API FPreparedTextHintOperation
{
    TWeakObjectPtr<UEditableTextBox> TargetWidget;
    FText Text;
};

// PSC-10A (ADR-0043 D3): the whole family of operation kinds as ONE enum/variant, not
// parallel per-kind arrays -- Visit() over FGV2PreparedOperationVariant is the only
// dispatch mechanism Apply() is allowed to use;
// a lambda-overload-set missing a case for one of these alternatives
// is a COMPILE ERROR (see PreparedPresentationTransaction.cpp's TOverloaded<...> uses),
// not a silently-skipped `default:` branch. Declaration order here MUST match
// FGV2PreparedOperationVariant's template argument order below -- GetPreparedOperationKind()
// relies on it (variant index == enum value) and a self-test in the field-inventory gate
// checks this invariant against the source text directly.
enum class EGV2PreparedOperationKind : uint8
{
    ImageResource,
    ImageHost,
    Boolean,
    EditableTextValue,
    ProgressBar,
    Number,
    Integer,
    String,
    Key,
    Binding,
    RichTextSpans,
    Text,
    KeyedCollection,
    TabContainer,
    PlainText,
    RichTextRender,
    TextHint,
    CentralStyle,
    ViewportRefresh,
};

// PSC-10B (ADR-0043 D3): central style as prepared operations. The payload is a VARIANT
// BY STYLE ROLE, not one struct with an optional field per widget class: a bag of
// optionals would accept a new style target silently, which is the shape this plan exists
// to remove. A new role without an Apply alternative is a compile error, exactly like a
// new operation kind.
//
// Roles carry finished physical values -- brush, colour, thickness, padding -- resolved
// by Prepare from the session snapshot's theme. Apply performs no lookup and holds no
// reference to a theme, resolved or otherwise.
struct GV2PRESENTATIONAPPLY_API FPreparedSeparatorStyle
{
    FSlateBrush Brush;
    float Thickness = 1.0f;
    bool bHorizontal = true;
};

struct GV2PRESENTATIONAPPLY_API FPreparedTintStyle
{
    FLinearColor Tint = FLinearColor::White;
};

struct GV2PRESENTATIONAPPLY_API FPreparedItemPaddingStyle
{
    FMargin Padding;
};

struct GV2PRESENTATIONAPPLY_API FPreparedProgressBarStyle
{
    FProgressBarStyle WidgetStyle;
    FLinearColor FillColor = FLinearColor::White;
};

// PSC-10B: DefaultLabelStyle/DefaultLabelScale are the style a label carrying NO style
// token would otherwise have none at all. A label that does carry a token was already
// styled by that token's own text operation, earlier in the same transaction -- central
// style deliberately does not restyle it, which is what removes the duplicate resolution
// these classes used to perform on every ApplyCentralStyle call.
struct GV2PRESENTATIONAPPLY_API FPreparedButtonStyle
{
    TSubclassOf<UCommonButtonStyle> ButtonStyle;
    TSubclassOf<UCommonTextStyle> DefaultLabelStyle;
    FPreparedTextScalePolicy DefaultLabelScale;
};

// PSC-10B: one style token resolved to finished values. BaseStyle is the token's style
// WITHOUT its final size; UnscaledFontSize is scaled against the LIVE viewport at use time,
// because a rich-text run is styled by a Slate decorator during rendering, long after Apply
// -- baking the size here would stop the text reflowing when the window is resized, which
// the pull path did not do either.
struct GV2PRESENTATIONAPPLY_API FPreparedRichTextTokenStyle
{
    TSubclassOf<UCommonTextStyle> StyleClass;
    FTextBlockStyle BaseStyle;
    float UnscaledFontSize = 0.0f;
    bool bResolved = false;
};

// PSC-10B: finished values for a RichText hover popover. The popover instance is created
// after the screen transaction, but it still applies this role through the same transaction
// facade: the creating RichText widget retains the value prepared for it, not a Theme.
struct GV2PRESENTATIONAPPLY_API FPreparedRichTextPopoverStyle
{
    FSlateBrush Background;
    FMargin Padding;
    float MaxWidth = 0.0f;
    float MaxHeight = 0.0f;
    FLinearColor ImageTint = FLinearColor::White;
    FPreparedViewportScalePolicy Scale;
};

// PSC-10B: everything UGV2RichTextWidgetBase and the hover popover it creates need, resolved
// once in Prepare. The decorator's own resolution (run style, interactive style) reads THIS,
// not a Theme: the token tables are finished values, so a synchronous Slate callback during
// rendering can be served without any authority being reachable from it.
struct GV2PRESENTATIONAPPLY_API FPreparedRichTextStyle
{
    FName DefaultTokenName;
    TSubclassOf<UCommonTextStyle> DefaultStyleClass;
    FPreparedRichTextTokenStyle DefaultToken;
    TMap<FName, FPreparedRichTextTokenStyle> StyleByToken;
    TMap<FName, FLinearColor> ColorByToken;
    TMap<FName, float> UnscaledSizeByToken;
    FPreparedTextScalePolicy ScalePolicy;
    FHyperlinkStyle InteractiveStyle;

    // Already loaded during Prepare: the hover path must not perform a synchronous load.
    TSubclassOf<UUserWidget> PopoverClass;
    FPreparedRichTextPopoverStyle PopoverStyle;
    bool bIsResolved = false;
};

struct GV2PRESENTATIONAPPLY_API FPreparedDropdownStyle
{
    TSubclassOf<UCommonButtonStyle> HeaderStyle;
    FSlateBrush PopupBackground;
    FMargin PopupPadding;
    FMargin OptionItemPadding;
    float MaxPopupHeight = 200.0f;
    FPreparedViewportScalePolicy PopupScale;
};

struct GV2PRESENTATIONAPPLY_API FPreparedInputFieldStyle
{
    FEditableTextBoxStyle WidgetStyle;
    TSubclassOf<UCommonTextStyle> DefaultLabelStyle;
    FPreparedTextScalePolicy DefaultLabelScale;
};

struct GV2PRESENTATIONAPPLY_API FPreparedCheckboxStyle
{
    FCheckBoxStyle WidgetStyle;
    TSubclassOf<UCommonTextStyle> DefaultLabelStyle;
};

struct GV2PRESENTATIONAPPLY_API FPreparedLoadingIndicatorStyle
{
    FSlateBrush Brush;
    float Period = 0.75f;
    float Radius = 64.0f;
    int32 Pieces = 6;
};

using FPreparedCentralStylePayload = TVariant<
    FPreparedSeparatorStyle,
    FPreparedTintStyle,
    FPreparedItemPaddingStyle,
    FPreparedProgressBarStyle,
    FPreparedLoadingIndicatorStyle,
    FPreparedButtonStyle,
    FPreparedCheckboxStyle,
    FPreparedInputFieldStyle,
    FPreparedDropdownStyle,
    FPreparedRichTextPopoverStyle,
    FPreparedRichTextStyle
>;

// TargetWidget is a GV2-owned widget base for every role that exists today. PSC-11 reaches
// each through its own value-only role interface (PreparedApplyTargets.h) rather than
// through its concrete type, which this module may not name; PSC-12 moves the UCLASSes
// themselves down. The operation, and every value in it, already lives here.
struct GV2PRESENTATIONAPPLY_API FPreparedCentralStyleOperation
{
    TWeakObjectPtr<UWidget> TargetWidget;
    FPreparedCentralStylePayload Payload;
};

// PSC-14: a presentation-only reaction to an Engine viewport resize. RootWidget is an
// already-committed screen and ViewportHeight is current physical geometry, not content or
// presentation authority. Apply walks the existing widget tree and asks only declared
// value-sink roles to recompute viewport-derived physical values from data cached during
// the original Prepare. No document replay, Theme lookup or widget replacement is involved.
struct GV2PRESENTATIONAPPLY_API FPreparedViewportRefreshOperation
{
    TWeakObjectPtr<UWidget> RootWidget;
    float ViewportHeight = 0.0f;
};

using FGV2PreparedOperationVariant = TVariant<
    FPreparedImageResourceOperation,
    FPreparedImageHostOperation,
    FPreparedBooleanOperation,
    FPreparedEditableTextValueOperation,
    FPreparedProgressBarOperation,
    FPreparedNumberOperation,
    FPreparedIntegerOperation,
    FPreparedStringOperation,
    FPreparedKeyOperation,
    FPreparedBindingOperation,
    FPreparedRichTextSpansOperation,
    FPreparedTextOperation,
    FPreparedKeyedCollectionOperation,
    FPreparedTabContainerOperation,
    FPreparedPlainTextOperation,
    FPreparedRichTextRenderOperation,
    FPreparedTextHintOperation,
    FPreparedCentralStyleOperation,
    FPreparedViewportRefreshOperation
>;

GV2PRESENTATIONAPPLY_API EGV2PreparedOperationKind GetPreparedOperationKind(const FGV2PreparedOperationVariant& Operation);

// PSC-09A/09B/10A (ADR-0043 D3): immutable once built -- an upper GV2 preparer appends
// operations, then hands the finished transaction to Apply() below; nothing mutates it
// afterward, and it carries no PrepareContext, snapshot, or resolver of any kind. Typed
// Add*Operation() convenience wrappers keep every existing producer call site unchanged
// (same pattern FGV2PreparedUiValue's Make*() factories use over their own TVariant) --
// the structural claim "one enum/variant" is about STORAGE (one TArray<FGV2PreparedOperationVariant>
// below, not seventeen parallel arrays) and DISPATCH (Visit(), not per-kind Get*Operations()
// accessors letting a consumer silently ignore a kind it doesn't yet know about).
class GV2PRESENTATIONAPPLY_API FGV2PreparedPresentationTransaction
{
public:
    void AddOperation(FGV2PreparedOperationVariant Operation)
    {
        Operations.Add(MoveTemp(Operation));
    }

    void AddImageResourceOperation(FPreparedImageResourceOperation Operation)
    {
        AddOperation(FGV2PreparedOperationVariant(TInPlaceType<FPreparedImageResourceOperation>(), MoveTemp(Operation)));
    }
    void AddImageHostOperation(FPreparedImageHostOperation Operation)
    {
        AddOperation(FGV2PreparedOperationVariant(TInPlaceType<FPreparedImageHostOperation>(), MoveTemp(Operation)));
    }
    void AddBooleanOperation(FPreparedBooleanOperation Operation)
    {
        AddOperation(FGV2PreparedOperationVariant(TInPlaceType<FPreparedBooleanOperation>(), MoveTemp(Operation)));
    }
    void AddEditableTextValueOperation(FPreparedEditableTextValueOperation Operation)
    {
        AddOperation(FGV2PreparedOperationVariant(TInPlaceType<FPreparedEditableTextValueOperation>(), MoveTemp(Operation)));
    }
    void AddProgressBarOperation(FPreparedProgressBarOperation Operation)
    {
        AddOperation(FGV2PreparedOperationVariant(TInPlaceType<FPreparedProgressBarOperation>(), MoveTemp(Operation)));
    }
    void AddNumberOperation(FPreparedNumberOperation Operation)
    {
        AddOperation(FGV2PreparedOperationVariant(TInPlaceType<FPreparedNumberOperation>(), MoveTemp(Operation)));
    }
    void AddIntegerOperation(FPreparedIntegerOperation Operation)
    {
        AddOperation(FGV2PreparedOperationVariant(TInPlaceType<FPreparedIntegerOperation>(), MoveTemp(Operation)));
    }
    void AddStringOperation(FPreparedStringOperation Operation)
    {
        AddOperation(FGV2PreparedOperationVariant(TInPlaceType<FPreparedStringOperation>(), MoveTemp(Operation)));
    }
    void AddKeyOperation(FPreparedKeyOperation Operation)
    {
        AddOperation(FGV2PreparedOperationVariant(TInPlaceType<FPreparedKeyOperation>(), MoveTemp(Operation)));
    }
    void AddBindingOperation(FPreparedBindingOperation Operation)
    {
        AddOperation(FGV2PreparedOperationVariant(TInPlaceType<FPreparedBindingOperation>(), MoveTemp(Operation)));
    }
    void AddRichTextSpansOperation(FPreparedRichTextSpansOperation Operation)
    {
        AddOperation(FGV2PreparedOperationVariant(TInPlaceType<FPreparedRichTextSpansOperation>(), MoveTemp(Operation)));
    }
    void AddTextOperation(FPreparedTextOperation Operation)
    {
        AddOperation(FGV2PreparedOperationVariant(TInPlaceType<FPreparedTextOperation>(), MoveTemp(Operation)));
    }
    void AddKeyedCollectionOperation(FPreparedKeyedCollectionOperation Operation)
    {
        AddOperation(FGV2PreparedOperationVariant(TInPlaceType<FPreparedKeyedCollectionOperation>(), MoveTemp(Operation)));
    }
    void AddTabContainerOperation(FPreparedTabContainerOperation Operation)
    {
        AddOperation(FGV2PreparedOperationVariant(TInPlaceType<FPreparedTabContainerOperation>(), MoveTemp(Operation)));
    }
    void AddPlainTextOperation(FPreparedPlainTextOperation Operation)
    {
        AddOperation(FGV2PreparedOperationVariant(TInPlaceType<FPreparedPlainTextOperation>(), MoveTemp(Operation)));
    }
    void AddRichTextRenderOperation(FPreparedRichTextRenderOperation Operation)
    {
        AddOperation(FGV2PreparedOperationVariant(TInPlaceType<FPreparedRichTextRenderOperation>(), MoveTemp(Operation)));
    }
    void AddTextHintOperation(FPreparedTextHintOperation Operation)
    {
        AddOperation(FGV2PreparedOperationVariant(TInPlaceType<FPreparedTextHintOperation>(), MoveTemp(Operation)));
    }

    void AddCentralStyleOperation(FPreparedCentralStyleOperation Operation)
    {
        AddOperation(FGV2PreparedOperationVariant(TInPlaceType<FPreparedCentralStyleOperation>(), MoveTemp(Operation)));
    }

    void AddViewportRefreshOperation(FPreparedViewportRefreshOperation Operation)
    {
        AddOperation(FGV2PreparedOperationVariant(TInPlaceType<FPreparedViewportRefreshOperation>(), MoveTemp(Operation)));
    }

    const TArray<FGV2PreparedOperationVariant>& GetOperations() const { return Operations; }

    bool IsEmpty() const { return Operations.IsEmpty(); }

private:
    TArray<FGV2PreparedOperationVariant> Operations;
};

}

// PSC-11 (ADR-0043 D2): the outcome of applying one whole transaction. A struct rather than
// a bare bool + FString so a caller cannot take the diagnostic without the verdict, and so a
// later field (counts, health) does not change every call site again.
struct GV2PRESENTATIONAPPLY_API FGV2PresentationApplyResult
{
    bool bApplied = true;
    int32 AppliedOperationCount = 0;
    FString Error;
};

// PSC-11: THE production entry point for applying a prepared transaction. There is exactly
// one, and it is here, below the boundary. Until PSC-11 there were two -- this module's own
// visitor plus GV2LegacyPresentationApplyAdapter for the GV2-owned targets this module
// cannot Cast to -- so every caller had to remember both and "applied" was not one fact.
// Those targets are now reached through the value-only role interfaces in
// PreparedApplyTargets.h, which this module CAN name, so the second entry point is gone.
class GV2PRESENTATIONAPPLY_API FGV2PresentationApply
{
public:
    static bool Apply(
        const GV2PresentationApply::FGV2PreparedPresentationTransaction& Transaction,
        FGV2PresentationApplyResult& OutResult);
};
