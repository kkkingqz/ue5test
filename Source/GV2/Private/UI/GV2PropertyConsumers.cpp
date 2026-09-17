#include "UI/GV2PropertyConsumers.h"

#include "Misc/ScopeExit.h"
#include "Application/GV2ScreenFieldMaterializer.h"
#include "Application/GV2SessionContentSnapshot.h"
#include "CommonTextBlock.h"
#include "CommonRichTextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2ImagePresentation.h"
#include "UI/GV2ImageWidgetBase.h"
#include "UI/GV2ApplyTransaction.h"
#include "UI/GV2UiBindingTarget.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2ButtonListWidgetBase.h"
#include "UI/GV2CentralStylePreparer.h"
#include "UI/GV2DropdownSelectWidgetBase.h"
#include "UI/GV2CheckboxWidgetBase.h"
#include "UI/GV2InputFieldWidgetBase.h"
#include "UI/GV2ProgressBarWidgetBase.h"
#include "UI/GV2PortraitWidgetBase.h"
#include "UI/GV2TextWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2ModalWidgetBase.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "UI/GV2TabContainerWidgetBase.h"
#include "UI/GV2ScreenRegistry.h"
#include "UI/GV2ScreenWidgetBase.h"
#include "UI/GV2UiMutationPlan.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2UiStyleConsumer.h"
#include "UI/GV2UiTheme.h"
#include "Bridge/GV2StableIdUE.h"
#include "Framework/Text/RichTextMarkupProcessing.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBox.h"
#include "Blueprint/UserWidget.h"

namespace
{
GV2PresentationApply::FPreparedTextValue FlattenTextValue(const FGV2TextViewModel& Text)
{
    GV2PresentationApply::FPreparedTextValue Flat;
    Flat.Text = Text.Text;
    Flat.StyleToken = Text.StyleToken;
    Flat.NormalizedMarkup = Text.NormalizedMarkup;
    Flat.ResolvedStyleClass = Text.ResolvedStyleClass;
    Flat.ResolvedBaseFontSize = Text.ResolvedBaseFontSize;
    Flat.ResolvedMinReadableFontSize = Text.ResolvedMinReadableFontSize;
    Flat.ResolvedReferenceViewportHeight = Text.ResolvedReferenceViewportHeight;
    Flat.ResolvedFontScaleCurve = Text.ResolvedFontScaleCurve;
    Flat.ResolvedDefaultStyle = Text.ResolvedDefaultStyle;
    Flat.bHasResolvedPresentation = Text.bHasResolvedPresentation;
    Flat.bHasResolvedDefaultStyle = Text.bHasResolvedDefaultStyle;
    return Flat;
}
}

// --- FGV2TextPropertyConsumer ---

bool FGV2TextPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsText();
}

bool FGV2TextPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    UWidget* ResolvedWidget = TargetWidget;
    if (UGV2TextWidgetBase* TW = Cast<UGV2TextWidgetBase>(TargetWidget))
    {
        ResolvedWidget = TW->GetTextBlock() ? Cast<UWidget>(TW->GetTextBlock()) : Cast<UWidget>(TW);
    }
    else if (UGV2RichTextWidgetBase* RTW = Cast<UGV2RichTextWidgetBase>(TargetWidget))
    {
        ResolvedWidget = RTW->GetRichTextBlock() ? Cast<UWidget>(RTW->GetRichTextBlock()) : Cast<UWidget>(RTW);
    }
    else if (UGV2ButtonWidgetBase* BW = Cast<UGV2ButtonWidgetBase>(TargetWidget))
    {
        ResolvedWidget = BW->GetLabelText() ? Cast<UWidget>(BW->GetLabelText()) : Cast<UWidget>(BW);
    }
    else if (UGV2DropdownSelectWidgetBase* DW = Cast<UGV2DropdownSelectWidgetBase>(TargetWidget))
    {
        ResolvedWidget = DW;
    }

    if (!ResolvedWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for text capability");
        return false;
    }

    if (!Cast<UCommonTextBlock>(ResolvedWidget) && !Cast<UCommonRichTextBlock>(ResolvedWidget)
        && !Cast<UEditableTextBox>(ResolvedWidget) && !Cast<UGV2ButtonWidgetBase>(ResolvedWidget)
        && !Cast<UGV2DropdownSelectWidgetBase>(ResolvedWidget) && !Cast<UGV2TextWidgetBase>(ResolvedWidget)
        && !Cast<UGV2RichTextWidgetBase>(ResolvedWidget))
    {
        OutError = TEXT("core:diagnostic.ui_consumer.target_type_mismatch: Target widget is not a supported text renderer");
        return false;
    }

    if (!Value.IsText())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected Text value");
        return false;
    }

    PreparedText = Value.AsText();
    if (Cast<UEditableTextBox>(ResolvedWidget) || Cast<UCommonTextBlock>(ResolvedWidget)
        || Cast<UGV2ButtonWidgetBase>(ResolvedWidget) || Cast<UGV2DropdownSelectWidgetBase>(ResolvedWidget)
        || Cast<UGV2TextWidgetBase>(ResolvedWidget))
    {
        if (PreparedText.NormalizedMarkup.Contains(TEXT("<gv2")))
        {
            OutError = TEXT("core:diagnostic.ui_consumer.unsupported_text_markup: Plain text renderer does not support formatted markup");
            return false;
        }
    }
    return true;
}

bool FGV2TextPropertyConsumer::BuildPreparedOperation(
    UWidget* TargetWidget,
    GV2PresentationApply::FGV2PreparedPresentationTransaction& OutTransaction,
    FString& OutError) const
{
    GV2PresentationApply::FPreparedTextOperation Operation;
    Operation.TargetWidget = TargetWidget;
    Operation.Value = FlattenTextValue(PreparedText);
    OutTransaction.AddTextOperation(MoveTemp(Operation));
    OutError.Reset();
    return true;
}

// GBF-07: rollback_leaf=PropertyMutation
bool FGV2TextPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
    if (!BuildPreparedOperation(TargetWidget, Transaction, OutError))
    {
        return false;
    }
    FGV2PresentationApplyResult ApplyResult;
    if (!FGV2PresentationApply::Apply(Transaction, ApplyResult))
    {
        OutError = ApplyResult.Error;
        return false;
    }
    return true;
}

void FGV2TextPropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (!TargetWidget)
    {
        return;
    }

    GV2PresentationApply::FPreparedTextOperation Operation;
    Operation.TargetWidget = TargetWidget;
    Operation.bIsReset = true;
    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
    Transaction.AddTextOperation(MoveTemp(Operation));
    FString ApplyError;
    GV2ApplyTransaction(Transaction, ApplyError);
}

// --- FGV2ImageResourcePropertyConsumer ---

bool FGV2ImageResourcePropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsStableId() && Value.AsStableId().TargetKind == TEXT("resource");
}

// PAH-08: phase=prepare -- validates the candidate resource against the catalog
// before any widget is touched.
bool FGV2ImageResourcePropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for image capability");
        return false;
    }

    UImage* ImageWidget = Cast<UImage>(TargetWidget);
    if (!ImageWidget)
    {
        if (UGV2ImageWidgetBase* ImageBase = Cast<UGV2ImageWidgetBase>(TargetWidget))
        {
            ImageWidget = ImageBase->GetImageWidget();
        }
        else if (UGV2PortraitWidgetBase* PortraitWidget = Cast<UGV2PortraitWidgetBase>(TargetWidget))
        {
            ImageWidget = PortraitWidget->GetPortraitImage();
        }
    }

    if (!ImageWidget && !Cast<UGV2ImageWidgetBase>(TargetWidget) && !Cast<UGV2PortraitWidgetBase>(TargetWidget))
    {
        OutError = TEXT("core:diagnostic.ui_consumer.target_type_mismatch: Target widget is not a UImage or image host");
        return false;
    }

    // GBH-07: checked against the capability's own declared TargetKind, not a literal
    // "resource" -- the declared constraint (GBH-06) is what Prepare must honor; every
    // capability that reaches this consumer today happens to declare "resource" (the
    // only target_kind this consumer's catalog resolution supports), but the check
    // itself no longer assumes that instead of reading it.
    if (!Value.IsStableId() || Value.AsStableId().TargetKind != Capability.TargetKind)
    {
        OutError = FString::Printf(
            TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected StableId with target_kind '%s'"),
            *Capability.TargetKind);
        return false;
    }

    EGV2PrimitiveScalePolicy Policy = EGV2PrimitiveScalePolicy::Unset;
    TOptional<float> FixedAspect;
    if (UGV2ImageWidgetBase* ImageHost = Cast<UGV2ImageWidgetBase>(TargetWidget))
    {
        Policy = ImageHost->GetScalePolicy();
        if (ImageHost->GetFixedAspectRatio() > 0.0f)
        {
            FixedAspect = ImageHost->GetFixedAspectRatio();
        }
    }
    else if (UGV2ImageWidgetBase* OuterHost = TargetWidget->GetTypedOuter<UGV2ImageWidgetBase>())
    {
        Policy = OuterHost->GetScalePolicy();
        if (OuterHost->GetFixedAspectRatio() > 0.0f)
        {
            FixedAspect = OuterHost->GetFixedAspectRatio();
        }
    }
    else if (UGV2PortraitWidgetBase* PortraitHost = Cast<UGV2PortraitWidgetBase>(TargetWidget))
    {
        Policy = EGV2PrimitiveScalePolicy::PreserveAspect;
        if (PortraitHost->GetPortraitAspectRatio() > 0.0f)
        {
            FixedAspect = PortraitHost->GetPortraitAspectRatio();
        }
    }
    else
    {
        Policy = EGV2PrimitiveScalePolicy::PreserveAspect;
    }

    if (Policy == EGV2PrimitiveScalePolicy::Unset)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.unset_scale_policy: Primitive scale policy is Unset");
        return false;
    }

    PreparedResourceId = Value.AsStableId().Id;
    PreparedScalePolicy = Policy;
    PreparedFixedAspectRatio = FixedAspect;

    // PSC-10B (ADR-0043 D1): resource authority is the pinned session snapshot. A
    // missing context is a typed Prepare failure, never permission to consult a second
    // process-global catalog.
    FGV2ResolvedImageResource Candidate;
    if (PrepareContext == nullptr)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_prepare_context: session image catalog is unavailable");
        return false;
    }
    if (!PrepareContext->ResolveResource(PreparedResourceId, Candidate, OutError))
    {
        return false;
    }
    // STATUS-012: keep the resolution, not just the id it came from.
    PreparedResource = Candidate;

    if (!IsScalePolicyCompatible(PreparedScalePolicy, Candidate.RenderMode))
    {
        OutError = TEXT("core:diagnostic.ui_consumer.incompatible_scale_policy: Image resource render mode is incompatible with primitive scaling policy");
        return false;
    }

    if (PreparedScalePolicy == EGV2PrimitiveScalePolicy::PreserveAspect && PreparedFixedAspectRatio.IsSet()
        && (!FMath::IsFinite(PreparedFixedAspectRatio.GetValue()) || PreparedFixedAspectRatio.GetValue() <= 0.0f
            || !FMath::IsNearlyEqual(Candidate.FixedAspectRatio, PreparedFixedAspectRatio.GetValue(), 0.001f)))
    {
        OutError = TEXT("core:diagnostic.ui_consumer.aspect_ratio_mismatch: fixed_aspect resource ratio does not match the target block ratio");
        return false;
    }

    return true;
}

// GBF-07: rollback_leaf=PropertyMutation

// STATUS-012 (ADR-0042, INV-P5): every branch applies PreparedResource, the resolution
// Prepare validated -- none re-consults the catalog, so the value that reaches the
// widget is the value that was approved.
bool FGV2ImageResourcePropertyConsumer::BuildPreparedOperation(
    UWidget* TargetWidget,
    GV2PresentationApply::FGV2PreparedPresentationTransaction& OutTransaction,
    FString& OutError) const
{
    if (UImage* ImageWidget = Cast<UImage>(TargetWidget))
    {
        FSlateBrush FinalBrush = PreparedResource.Brush;
        switch (PreparedScalePolicy)
        {
        case EGV2PrimitiveScalePolicy::Tile:
            FinalBrush.Tiling = ESlateBrushTileType::Both;
            FinalBrush.DrawAs = ESlateBrushDrawType::Image;
            break;
        case EGV2PrimitiveScalePolicy::NineSlice:
            FinalBrush.DrawAs = ESlateBrushDrawType::Box;
            break;
        case EGV2PrimitiveScalePolicy::Unset:
        case EGV2PrimitiveScalePolicy::FreeStretch:
        case EGV2PrimitiveScalePolicy::PreserveAspect:
            FinalBrush.Tiling = ESlateBrushTileType::NoTile;
            FinalBrush.DrawAs = ESlateBrushDrawType::Image;
            break;
        }

        GV2PresentationApply::FPreparedImageResourceOperation Operation;
        Operation.TargetWidget = ImageWidget;
        Operation.Brush = FinalBrush;
        OutTransaction.AddImageResourceOperation(MoveTemp(Operation));
    }
    // PSC-09B/11: UGV2ImageWidgetBase/UGV2PortraitWidgetBase route through their own
    // IGV2PreparedImageHostTarget sink -- which calls ApplyResolvedImageResource/
    // ApplyResolvedPortrait -- rather than reaching past them to their inner UImage as the
    // plain-UImage branch above does: those methods also update bookkeeping
    // (AppliedResourceId/ResolvedAspectRatio, GetPortraitResourceId/GetFrameResourceId)
    // that a direct SetBrush would leave frozen at whatever NativePreConstruct set.
    else if (Cast<UGV2ImageWidgetBase>(TargetWidget) != nullptr || Cast<UGV2PortraitWidgetBase>(TargetWidget) != nullptr)
    {
        GV2PresentationApply::FPreparedImageHostOperation Operation;
        Operation.TargetWidget = TargetWidget;
        Operation.Resolved.ResourceId = PreparedResource.ResourceId;
        Operation.Resolved.RenderMode = FGV2ImagePresentation::ToPreparedRenderMode(PreparedResource.RenderMode);
        Operation.Resolved.FixedAspectRatio = PreparedResource.FixedAspectRatio;
        Operation.Resolved.Brush = PreparedResource.Brush;
        OutTransaction.AddImageHostOperation(MoveTemp(Operation));
    }
    else
    {
        OutError = TEXT("core:diagnostic.ui_consumer.target_type_mismatch: Target widget is not a UImage or image host");
        return false;
    }

    OutError.Reset();
    return true;
}

// GBF-07: rollback_leaf=PropertyMutation
bool FGV2ImageResourcePropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
    if (!BuildPreparedOperation(TargetWidget, Transaction, OutError))
    {
        return false;
    }
    FGV2PresentationApplyResult ApplyResult;
    if (!FGV2PresentationApply::Apply(Transaction, ApplyResult))
    {
        OutError = ApplyResult.Error;
        return false;
    }
    return true;
}

void FGV2ImageResourcePropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (!TargetWidget)
    {
        return;
    }

    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
    if (UImage* ImageWidget = Cast<UImage>(TargetWidget))
    {
        // A default-constructed operation's Brush is FSlateBrush()'s own default, the
        // same value this Reset always applied directly before.
        GV2PresentationApply::FPreparedImageResourceOperation Operation;
        Operation.TargetWidget = ImageWidget;
        Transaction.AddImageResourceOperation(MoveTemp(Operation));
        FString ApplyError;
        GV2ApplyTransaction(Transaction, ApplyError);
    }
    else
    {
        GV2PresentationApply::FPreparedImageHostOperation Operation;
        Operation.TargetWidget = TargetWidget;
        Operation.bResetToDefault = true;
        Transaction.AddImageHostOperation(MoveTemp(Operation));
        FString ApplyError;
        GV2ApplyTransaction(Transaction, ApplyError);
    }

    FGV2PresentationApplyResult ApplyResult;
    FGV2PresentationApply::Apply(Transaction, ApplyResult);
}

// --- FGV2BooleanPropertyConsumer ---

bool FGV2BooleanPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsBoolean();
}

bool FGV2BooleanPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for boolean capability");
        return false;
    }

    if (!Value.IsBoolean())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected Boolean value");
        return false;
    }

    bPreparedValue = Value.AsBoolean();
    PropertyName = Capability.PropertyName;
    return true;
}

namespace
{
// PSC-09B: shared by BuildPreparedOperation (Commit) and Reset -- both need to agree on
// exactly which setter a given (TargetWidget, PropertyName) pair maps to; only the VALUE
// applied differs between the two.
GV2PresentationApply::EPreparedBooleanTarget DetermineBooleanOperationTarget(UWidget* TargetWidget, const FString& PropertyName)
{
    if (Cast<UCheckBox>(TargetWidget) != nullptr)
    {
        return PropertyName == TEXT("is_read_only")
            ? GV2PresentationApply::EPreparedBooleanTarget::WidgetEnabled
            : GV2PresentationApply::EPreparedBooleanTarget::CheckBoxChecked;
    }
    if (Cast<UEditableTextBox>(TargetWidget) != nullptr)
    {
        return PropertyName == TEXT("is_read_only")
            ? GV2PresentationApply::EPreparedBooleanTarget::EditableTextReadOnly
            : GV2PresentationApply::EPreparedBooleanTarget::WidgetEnabled;
    }
    if (Cast<UGV2DropdownSelectWidgetBase>(TargetWidget) != nullptr && PropertyName == TEXT("is_open"))
    {
        return GV2PresentationApply::EPreparedBooleanTarget::HostDeclaredBoolean;
    }
    return GV2PresentationApply::EPreparedBooleanTarget::WidgetEnabled;
}
}

// PSC-09B (ADR-0043 D2/D3): narrows to exactly which setter Commit() will apply --
// CheckBox's own "is_read_only" branch inverts the value (SetIsEnabled(!bPreparedValue)),
// every other WidgetEnabled branch applies it directly.
bool FGV2BooleanPropertyConsumer::BuildPreparedOperation(
    UWidget* TargetWidget,
    GV2PresentationApply::FGV2PreparedPresentationTransaction& OutTransaction,
    FString& OutError) const
{
    GV2PresentationApply::FPreparedBooleanOperation Operation;
    Operation.TargetWidget = TargetWidget;
    Operation.PropertyName = FName(*PropertyName);
    Operation.Target = DetermineBooleanOperationTarget(TargetWidget, PropertyName);
    Operation.Value = (Operation.Target == GV2PresentationApply::EPreparedBooleanTarget::WidgetEnabled
            && Cast<UCheckBox>(TargetWidget) != nullptr
            && PropertyName == TEXT("is_read_only"))
        ? !bPreparedValue
        : bPreparedValue;

    OutTransaction.AddBooleanOperation(MoveTemp(Operation));
    OutError.Reset();
    return true;
}

// GBF-07: rollback_leaf=PropertyMutation
bool FGV2BooleanPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
    if (!BuildPreparedOperation(TargetWidget, Transaction, OutError))
    {
        return false;
    }
    FGV2PresentationApplyResult ApplyResult;
    if (!FGV2PresentationApply::Apply(Transaction, ApplyResult))
    {
        OutError = ApplyResult.Error;
        return false;
    }
    return true;
}

void FGV2BooleanPropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (!TargetWidget)
    {
        return;
    }

    GV2PresentationApply::FPreparedBooleanOperation Operation;
    Operation.TargetWidget = TargetWidget;
    Operation.PropertyName = FName(*PropertyName);
    Operation.Target = DetermineBooleanOperationTarget(TargetWidget, PropertyName);
    // PSC-09B: every branch's own reset default -- WidgetEnabled resets to enabled
    // (true), every other target resets to false (unchecked/not-read-only/closed).
    Operation.Value = Operation.Target == GV2PresentationApply::EPreparedBooleanTarget::WidgetEnabled;

    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
    Transaction.AddBooleanOperation(MoveTemp(Operation));
    FGV2PresentationApplyResult ApplyResult;
    FGV2PresentationApply::Apply(Transaction, ApplyResult);
}

// --- FGV2IntegerPropertyConsumer ---

bool FGV2IntegerPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsInteger();
}

bool FGV2IntegerPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for integer capability");
        return false;
    }

    if (!Value.IsInteger())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected Integer value");
        return false;
    }

    const int64 Candidate = Value.AsInteger();
    // GBH-07: same reasoning as FGV2NumberPropertyConsumer -- the declared range is
    // checked here, in Prepare, not left to whatever the target happens to do with an
    // out-of-range value.
    if (Capability.IntMin.IsSet() && Candidate < *Capability.IntMin)
    {
        OutError = FString::Printf(
            TEXT("core:diagnostic.ui_consumer.value_out_of_range: Integer %lld is below capability minimum %lld"),
            Candidate, *Capability.IntMin);
        return false;
    }
    if (Capability.IntMax.IsSet() && Candidate > *Capability.IntMax)
    {
        OutError = FString::Printf(
            TEXT("core:diagnostic.ui_consumer.value_out_of_range: Integer %lld is above capability maximum %lld"),
            Candidate, *Capability.IntMax);
        return false;
    }

    PreparedValue = Candidate;
    PropertyName = Capability.PropertyName;
    return true;
}

bool FGV2IntegerPropertyConsumer::BuildPreparedOperation(
    UWidget* TargetWidget,
    GV2PresentationApply::FGV2PreparedPresentationTransaction& OutTransaction,
    FString& OutError) const
{
    GV2PresentationApply::FPreparedIntegerOperation Operation;
    Operation.TargetWidget = TargetWidget;
    Operation.Value = PreparedValue;
    OutTransaction.AddIntegerOperation(MoveTemp(Operation));
    OutError.Reset();
    return true;
}

// GBF-07: rollback_leaf=PropertyMutation
bool FGV2IntegerPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
    if (!BuildPreparedOperation(TargetWidget, Transaction, OutError))
    {
        return false;
    }
    FGV2PresentationApplyResult ApplyResult;
    if (!FGV2PresentationApply::Apply(Transaction, ApplyResult))
    {
        OutError = ApplyResult.Error;
        return false;
    }
    return true;
}

void FGV2IntegerPropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (TargetWidget)
    {
        GV2PresentationApply::FPreparedIntegerOperation Operation;
        Operation.TargetWidget = TargetWidget;
        Operation.Value = 0;
        GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
        Transaction.AddIntegerOperation(MoveTemp(Operation));
        FString ApplyError;
        GV2ApplyTransaction(Transaction, ApplyError);
    }
    PreparedValue = 0;
}

// --- FGV2NumberPropertyConsumer ---

bool FGV2NumberPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsNumber();
}

bool FGV2NumberPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for number capability");
        return false;
    }

    if (!Value.IsNumber())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected Number value");
        return false;
    }

    const double Candidate = Value.AsNumber();
    // GBH-07: the declared capability's own range is the first place this is checked,
    // not the widget clamping it at render (REM-01) -- Schema <-> capability range
    // compatibility (GBH-06) only ever compared the two *declared* ranges; nothing
    // before this stopped an in-range-for-the-schema but genuinely malformed runtime
    // value (or one reaching Prepare through a path the schema check does not cover)
    // from silently reaching Commit.
    if (Capability.NumberMin.IsSet() && Candidate < *Capability.NumberMin)
    {
        OutError = FString::Printf(
            TEXT("core:diagnostic.ui_consumer.value_out_of_range: Number %f is below capability minimum %f"),
            Candidate, *Capability.NumberMin);
        return false;
    }
    if (Capability.NumberMax.IsSet() && Candidate > *Capability.NumberMax)
    {
        OutError = FString::Printf(
            TEXT("core:diagnostic.ui_consumer.value_out_of_range: Number %f is above capability maximum %f"),
            Candidate, *Capability.NumberMax);
        return false;
    }

    PreparedValue = Candidate;
    return true;
}

bool FGV2NumberPropertyConsumer::BuildPreparedOperation(
    UWidget* TargetWidget,
    GV2PresentationApply::FGV2PreparedPresentationTransaction& OutTransaction,
    FString& OutError) const
{
    if (UProgressBar* PB = Cast<UProgressBar>(TargetWidget))
    {
        GV2PresentationApply::FPreparedProgressBarOperation Operation;
        Operation.TargetWidget = PB;
        Operation.Percent = static_cast<float>(PreparedValue);
        OutTransaction.AddProgressBarOperation(MoveTemp(Operation));
    }
    else
    {
        GV2PresentationApply::FPreparedNumberOperation Operation;
        Operation.TargetWidget = TargetWidget;
        Operation.Value = PreparedValue;
        OutTransaction.AddNumberOperation(MoveTemp(Operation));
    }
    OutError.Reset();
    return true;
}

// GBF-07: rollback_leaf=PropertyMutation
bool FGV2NumberPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
    if (!BuildPreparedOperation(TargetWidget, Transaction, OutError))
    {
        return false;
    }
    FGV2PresentationApplyResult ApplyResult;
    if (!FGV2PresentationApply::Apply(Transaction, ApplyResult))
    {
        OutError = ApplyResult.Error;
        return false;
    }
    return true;
}

void FGV2NumberPropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (!TargetWidget)
    {
        return;
    }

    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
    if (UProgressBar* PB = Cast<UProgressBar>(TargetWidget))
    {
        GV2PresentationApply::FPreparedProgressBarOperation Operation;
        Operation.TargetWidget = PB;
        Operation.Percent = 0.0f;
        Transaction.AddProgressBarOperation(MoveTemp(Operation));
        FString ApplyError;
        GV2ApplyTransaction(Transaction, ApplyError);
    }
    else
    {
        GV2PresentationApply::FPreparedNumberOperation Operation;
        Operation.TargetWidget = TargetWidget;
        Operation.Value = 0.0;
        Transaction.AddNumberOperation(MoveTemp(Operation));
        FString ApplyError;
        GV2ApplyTransaction(Transaction, ApplyError);
    }
    FGV2PresentationApplyResult ApplyResult;
    FGV2PresentationApply::Apply(Transaction, ApplyResult);
}

// --- FGV2StringPropertyConsumer ---

bool FGV2StringPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsString();
}

bool FGV2StringPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for string capability");
        return false;
    }

    if (!Value.IsString())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected String value");
        return false;
    }

    PreparedValue = Value.AsString();
    return true;
}

bool FGV2StringPropertyConsumer::BuildPreparedOperation(
    UWidget* TargetWidget,
    GV2PresentationApply::FGV2PreparedPresentationTransaction& OutTransaction,
    FString& OutError) const
{
    GV2PresentationApply::FPreparedStringOperation Operation;
    Operation.TargetWidget = TargetWidget;
    Operation.Value = PreparedValue;
    OutTransaction.AddStringOperation(MoveTemp(Operation));
    OutError.Reset();
    return true;
}

// GBF-07: rollback_leaf=PropertyMutation
bool FGV2StringPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
    if (!BuildPreparedOperation(TargetWidget, Transaction, OutError))
    {
        return false;
    }
    FGV2PresentationApplyResult ApplyResult;
    if (!FGV2PresentationApply::Apply(Transaction, ApplyResult))
    {
        OutError = ApplyResult.Error;
        return false;
    }
    return true;
}

void FGV2StringPropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (TargetWidget)
    {
        // PSC-09B: reset always applies the empty string, so the adapter's own
        // max-length truncation is a no-op here regardless of whether the target has a
        // UGV2InputFieldWidgetBase host -- same adapter path as Commit, no special case.
        GV2PresentationApply::FPreparedStringOperation Operation;
        Operation.TargetWidget = TargetWidget;
        GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
        Transaction.AddStringOperation(MoveTemp(Operation));
        FString ApplyError;
        GV2ApplyTransaction(Transaction, ApplyError);
    }
    PreparedValue.Empty();
}

// --- FGV2KeyPropertyConsumer ---

bool FGV2KeyPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsKey();
}

bool FGV2KeyPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for key capability");
        return false;
    }

    if (!Value.IsKey())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected Key value");
        return false;
    }

    PreparedValue = Value.AsKey();
    PropertyName = Capability.PropertyName;
    return true;
}

// DUC-03: "selected_key" and "default_tab_key" are their own capabilities, not the
// generic `key` identity, and are routed by name so they can never be shadowed by (or
// shadow) a host's real `key` -- see UGV2TabContainerWidgetBase, which declares both
// "key" (its own identity) and "default_tab_key" (its own concept) on itself. Actual
// dispatch is the target's own IGV2PreparedKeyTarget routing; a host that does not own the
// name refuses it (IsHostClaimedKeyCapability, GV2UiPropertyHost.h) and the facade reports
// core:diagnostic.ui_consumer.unhandled_target.
bool FGV2KeyPropertyConsumer::BuildPreparedOperation(
    UWidget* TargetWidget,
    GV2PresentationApply::FGV2PreparedPresentationTransaction& OutTransaction,
    FString& OutError) const
{
    GV2PresentationApply::FPreparedKeyOperation Operation;
    Operation.TargetWidget = TargetWidget;
    Operation.PropertyName = FName(*PropertyName);
    Operation.Value = PreparedValue;
    OutTransaction.AddKeyOperation(MoveTemp(Operation));
    OutError.Reset();
    return true;
}

// GBF-07: rollback_leaf=PropertyMutation
bool FGV2KeyPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
    if (!BuildPreparedOperation(TargetWidget, Transaction, OutError))
    {
        return false;
    }
    FGV2PresentationApplyResult ApplyResult;
    if (!FGV2PresentationApply::Apply(Transaction, ApplyResult))
    {
        OutError = ApplyResult.Error;
        return false;
    }
    return true;
}

void FGV2KeyPropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (TargetWidget)
    {
        GV2PresentationApply::FPreparedKeyOperation Operation;
        Operation.TargetWidget = TargetWidget;
        Operation.PropertyName = FName(*PropertyName);
        Operation.Value = FString(); // NAME_None once reconstructed by the adapter
        GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
        Transaction.AddKeyOperation(MoveTemp(Operation));
        FString ApplyError;
        GV2ApplyTransaction(Transaction, ApplyError);
    }
    PreparedValue.Empty();
    PropertyName.Empty();
}

// --- FGV2BindingPropertyConsumer ---

bool FGV2BindingPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsBinding();
}

bool FGV2BindingPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for binding capability");
        return false;
    }

    if (!Cast<IGV2UiBindingTarget>(TargetWidget))
    {
        OutError = TEXT("core:diagnostic.ui_consumer.target_type_mismatch: Target widget does not implement IGV2UiBindingTarget");
        return false;
    }

    if (!Value.IsBinding())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected Binding value");
        return false;
    }

    PreparedBinding = Value.AsBinding();
    return true;
}

bool FGV2BindingPropertyConsumer::BuildPreparedOperation(
    UWidget* TargetWidget,
    GV2PresentationApply::FGV2PreparedPresentationTransaction& OutTransaction,
    FString& OutError) const
{
    GV2PresentationApply::FPreparedBindingOperation Operation;
    Operation.TargetWidget = TargetWidget;
    Operation.SerializedHandle = PreparedBinding.ToString();
    OutTransaction.AddBindingOperation(MoveTemp(Operation));
    OutError.Reset();
    return true;
}

// GBF-07: rollback_leaf=PropertyMutation
bool FGV2BindingPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
    if (!BuildPreparedOperation(TargetWidget, Transaction, OutError))
    {
        return false;
    }
    FGV2PresentationApplyResult ApplyResult;
    if (!FGV2PresentationApply::Apply(Transaction, ApplyResult))
    {
        OutError = ApplyResult.Error;
        return false;
    }
    return true;
}

void FGV2BindingPropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (TargetWidget)
    {
        GV2PresentationApply::FPreparedBindingOperation Operation;
        Operation.TargetWidget = TargetWidget;
        Operation.SerializedHandle = FGV2UiBindingHandle().ToString();
        GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
        Transaction.AddBindingOperation(MoveTemp(Operation));
        FString ApplyError;
        GV2ApplyTransaction(Transaction, ApplyError);
    }
}

// --- FGV2KeyedCollectionPropertyConsumer ---

FGV2KeyedCollectionPropertyConsumer::FGV2KeyedCollectionPropertyConsumer() = default;
FGV2KeyedCollectionPropertyConsumer::~FGV2KeyedCollectionPropertyConsumer() = default;

bool FGV2KeyedCollectionPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsArray();
}

static GV2ContentCore::FCompiledUiFieldSpecPtr MakeCollectionSpecFromCapability(const FGV2UiPropertyCapability& Cap)
{
    using namespace GV2ContentCore;
    switch (Cap.SupportedKind)
    {
    case EGV2PreparedUiValueKind::Boolean:
    {
        auto Spec = std::make_shared<FCompiledUiFieldSpec>();
        Spec->Kind = EUiFieldKind::Scalar;
        Spec->Scalar = FScalarFieldSpec{};
        Spec->Scalar->Kind = EScalarFieldKind::Boolean;
        return Spec;
    }
    case EGV2PreparedUiValueKind::Integer:
    {
        auto Spec = std::make_shared<FCompiledUiFieldSpec>();
        Spec->Kind = EUiFieldKind::Scalar;
        FScalarFieldSpec Scalar;
        Scalar.Kind = EScalarFieldKind::Integer;
        if (Cap.IntMin.IsSet()) { Scalar.MinimumInteger = Cap.IntMin.GetValue(); }
        if (Cap.IntMax.IsSet()) { Scalar.MaximumInteger = Cap.IntMax.GetValue(); }
        Spec->Scalar = MoveTemp(Scalar);
        return Spec;
    }
    case EGV2PreparedUiValueKind::Number:
    {
        auto Spec = std::make_shared<FCompiledUiFieldSpec>();
        Spec->Kind = EUiFieldKind::Scalar;
        FScalarFieldSpec Scalar;
        Scalar.Kind = EScalarFieldKind::Number;
        if (Cap.NumberMin.IsSet()) { Scalar.MinimumNumber = Cap.NumberMin.GetValue(); }
        if (Cap.NumberMax.IsSet()) { Scalar.MaximumNumber = Cap.NumberMax.GetValue(); }
        Spec->Scalar = MoveTemp(Scalar);
        return Spec;
    }
    case EGV2PreparedUiValueKind::String:
    {
        auto Spec = std::make_shared<FCompiledUiFieldSpec>();
        Spec->Kind = EUiFieldKind::Scalar;
        Spec->Scalar = FScalarFieldSpec{};
        Spec->Scalar->Kind = EScalarFieldKind::String;
        return Spec;
    }
    case EGV2PreparedUiValueKind::Key:
        return std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Key);
    case EGV2PreparedUiValueKind::Text:
        return std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Text);
    case EGV2PreparedUiValueKind::Binding:
        return std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Binding);
    case EGV2PreparedUiValueKind::StableId:
    {
        auto Spec = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Ref);
        Spec->RefTargetKind = Cap.TargetKind.IsEmpty() ? "resource" : TCHAR_TO_UTF8(*Cap.TargetKind);
        return Spec;
    }
    case EGV2PreparedUiValueKind::Object:
    {
        auto Spec = std::make_shared<FCompiledUiFieldSpec>(EUiFieldKind::Object);
        if (Cap.ChildTree)
        {
            for (const auto& Entry : Cap.ChildTree->Properties)
            {
                Spec->Fields.push_back({ TCHAR_TO_UTF8(*Entry.Key), false, MakeCollectionSpecFromCapability(Entry.Value) });
            }
        }
        return Spec;
    }
    case EGV2PreparedUiValueKind::Null:
    case EGV2PreparedUiValueKind::Array:
        return nullptr;
    }
    return nullptr;
}

static TArray<FGV2CollectionItemDiscrepancy> GAllRecordedDiscrepancies;

FString FGV2CollectionItemDiscrepancy::ToSection32String() const
{
    return FString::Printf(
        TEXT("UiPropertyDiscrepancy:\n  screen=%s\n  field=%s\n  schema=%s\n  path=%s\n  widget=%s\n  capability=%s\n  code=%s\n  message=%s"),
        ScreenId.IsEmpty() ? TEXT("none") : *ScreenId,
        FieldId.IsEmpty() ? TEXT("none") : *FieldId,
        SchemaId.IsEmpty() ? TEXT("none") : *SchemaId,
        *PropertyPath,
        *WidgetClass,
        *Capability,
        *Code,
        *Message);
}

FString FGV2CollectionItemDiscrepancy::ToLogString() const
{
    return FString::Printf(
        TEXT("[UiPropertyDiscrepancy] screen=%s field=%s schema=%s path=%s widget=%s capability=%s code=%s message=%s"),
        ScreenId.IsEmpty() ? TEXT("none") : *ScreenId,
        FieldId.IsEmpty() ? TEXT("none") : *FieldId,
        SchemaId.IsEmpty() ? TEXT("none") : *SchemaId,
        *PropertyPath,
        *WidgetClass,
        *Capability,
        *Code,
        *Message);
}

const TArray<FGV2CollectionItemDiscrepancy>& FGV2KeyedCollectionPropertyConsumer::GetAllRecordedDiscrepancies()
{
    return GAllRecordedDiscrepancies;
}

void FGV2KeyedCollectionPropertyConsumer::ClearAllRecordedDiscrepancies()
{
    GAllRecordedDiscrepancies.Reset();
}

bool FGV2KeyedCollectionPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    // GBF-05: same shape as the tab consumer -- entries accumulate per item and
    // several rejections happen after the first ones exist. A rejected Prepare
    // must leave nothing committable behind (ADR-0041).
    bool bCollectionPrepareAccepted = false;
    ON_SCOPE_EXIT
    {
        if (!bCollectionPrepareAccepted)
        {
            PreparedItems.Reset();
            CandidateWidgetsByKey.Reset();
        }
    };

    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null for keyed collection capability");
        return false;
    }

    if (!Value.IsArray())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected Array value for keyed collection");
        return false;
    }

    KeyPropertyName = Capability.KeyPropertyName.IsEmpty() ? TEXT("key") : Capability.KeyPropertyName;
    PreparedItems.Reset();
    CandidateWidgetsByKey.Reset();
    Discrepancies.Reset();

    const FGV2PreparedUiArray& Array = Value.AsArray();
    UGV2ListViewWidgetBase* ListView = Cast<UGV2ListViewWidgetBase>(TargetWidget);
    UPanelWidget* Panel = ListView ? ListView->GetContainerPanel() : Cast<UPanelWidget>(TargetWidget);

    TMap<FName, TObjectPtr<UWidget>> ExistingWidgets;
    if (ListView)
    {
        ExistingWidgets = ListView->GetActiveWidgetsMap();
    }
    else
    {
        for (const auto& Pair : ActiveWidgetsByKey)
        {
            if (Pair.Value.IsValid())
            {
                ExistingWidgets.Add(Pair.Key, Pair.Value.Get());
            }
        }
    }

    // Determine fallback widget class if needed
    UClass* EntryClass = Capability.EntryWidgetClass != nullptr ? Capability.EntryWidgetClass.Get() : nullptr;
    if (!EntryClass)
    {
        if (ExistingWidgets.Num() > 0)
        {
            for (const auto& Pair : ExistingWidgets)
            {
                if (Pair.Value != nullptr)
                {
                    EntryClass = Pair.Value->GetClass();
                    break;
                }
            }
        }
        else if (Panel && Panel->GetChildrenCount() > 0)
        {
            EntryClass = Panel->GetChildAt(0)->GetClass();
        }
    }

    for (const FGV2PreparedUiValue& ItemVal : Array)
    {
        if (!ItemVal.IsObject())
        {
            OutError = TEXT("core:diagnostic.ui_consumer.invalid_item: Keyed collection item must be an Object");
            return false;
        }

        const FGV2PreparedUiObject& ItemObj = ItemVal.AsObject();
        const FGV2PreparedUiValue* KeyVal = ItemObj.FindField(KeyPropertyName);
        if (!KeyVal || !KeyVal->IsKey())
        {
            OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.missing_item_key: Keyed collection item missing required Key field '%s'"), *KeyPropertyName);
            return false;
        }

        const FName ItemKey = FName(*KeyVal->AsKey());
        if (ItemKey.IsNone())
        {
            OutError = TEXT("core:diagnostic.ui_consumer.invalid_item_key: Keyed collection item key is empty");
            return false;
        }

        if (CandidateWidgetsByKey.Contains(ItemKey))
        {
            OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.duplicate_item_key: Duplicate key '%s' in keyed collection"), *ItemKey.ToString());
            return false;
        }

        UWidget* ItemWidget = ExistingWidgets.FindRef(ItemKey);
        const bool bItemReused = (ItemWidget != nullptr);
        if (!ItemWidget)
        {
            if (!EntryClass)
            {
                OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.missing_entry_class: No entry class available to instantiate item for key '%s'"), *ItemKey.ToString());
                return false;
            }

            UWorld* World = TargetWidget->GetWorld();
            if (EntryClass->IsChildOf(UUserWidget::StaticClass()))
            {
                ItemWidget = CreateWidget<UUserWidget>(World ? World : TargetWidget->GetTypedOuter<UWorld>(), EntryClass);
            }
            else
            {
                ItemWidget = NewObject<UWidget>(TargetWidget, EntryClass);
            }

            if (!ItemWidget)
            {
                OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.item_widget_creation_failed: Failed to create widget for key '%s'"), *ItemKey.ToString());
                return false;
            }
        }

        CandidateWidgetsByKey.Add(ItemKey, TStrongObjectPtr<UWidget>(ItemWidget));

        FGV2UiCapabilityTree ItemCaps;
        if (IGV2UiPropertyHost* ItemHost = Cast<IGV2UiPropertyHost>(ItemWidget))
        {
            FGV2UiCapabilityBuilder ItemCapBuilder;
            ItemHost->DescribeUiCapabilities(ItemCapBuilder);
            ItemCaps = ItemCapBuilder.Build();
        }

        if (Cast<IGV2UiPropertyHost>(ItemWidget) != nullptr)
        {
            if (CompiledItemSpec == nullptr)
            {
                OutError = TEXT("core:diagnostic.ui_consumer.missing_schema: CompiledItemSpec is required for collection item host");
                return false;
            }

            const FString FullItemPrefix = ContextPropertyPath.IsEmpty()
                ? FString::Printf(TEXT("[%s]"), *ItemKey.ToString())
                : FString::Printf(TEXT("%s[%s]"), *ContextPropertyPath, *ItemKey.ToString());

            IGV2UiPropertyHost* const ItemHost = Cast<IGV2UiPropertyHost>(ItemWidget);
            FGV2UiHostSemanticState& ItemState = GetUiHostSemanticState(ItemHost->GetPropertyHostState());
            FGV2UiHostCommittedSnapshot PreviousItemSnapshot = ItemState.GetCommittedSnapshot();
            const FGV2PreparedUiObject PreviousItemValue = ItemState.GetLastCommittedProperties();
            const std::shared_ptr<const GV2ContentCore::FCompiledUiFieldSpec>& PreviousItemSchema = ItemState.GetLastCommittedSchema();
            const FString& PreviousItemSchemaId = ItemState.GetLastCommittedSchemaId();
            const bool bHasPreviousItemSnapshot = PreviousItemSchema && !PreviousItemSchemaId.IsEmpty();
            if (!PreviousItemValue.IsEmpty() && !bHasPreviousItemSnapshot)
            {
                OutError = FString::Printf(
                    TEXT("core:diagnostic.ui_rollback.missing_committed_schema: collection item '%s' has a previous value but no committed schema snapshot"),
                    *ItemKey.ToString());
                return false;
            }

            TSharedPtr<FGV2UiHostMutationPlan> ItemPlan = MakeShared<FGV2UiHostMutationPlan>();
            TArray<FGV2UiSchemaCompatibilityDiagnostic> Diagnostics;
            if (!PrepareUiHostProperties(
                    Cast<UUserWidget>(ItemWidget),
                    ItemCaps,
                    ItemObj,
                    *CompiledItemSpec,
                    ContextSchemaId.IsEmpty() ? TEXT("core:schema.ui_value.collection_item.v1") : ContextSchemaId,
                    FullItemPrefix,
                    PreviousItemValue,
                    *ItemPlan,
                    Diagnostics,
                    nullptr,
                    PrepareContext))
            {
                for (const FGV2UiSchemaCompatibilityDiagnostic& Diag : Diagnostics)
                {
                    FGV2CollectionItemDiscrepancy Discrepancy;
                    Discrepancy.ScreenId = ContextScreenId;
                    Discrepancy.FieldId = ContextFieldId;
                    Discrepancy.SchemaId = Diag.SchemaId;
                    Discrepancy.PropertyPath = Diag.PropertyPath;
                    Discrepancy.WidgetClass = ItemWidget ? ItemWidget->GetClass()->GetName() : TEXT("None");
                    Discrepancy.Capability = Diag.PropertyPath;
                    Discrepancy.Code = Diag.Code;
                    Discrepancy.Message = Diag.Message;

                    Discrepancies.Add(Discrepancy);
                    GAllRecordedDiscrepancies.Add(Discrepancy);

                    UE_LOG(LogTemp, Display, TEXT("%s"), *Discrepancy.ToLogString());
                }

                OutError = Diagnostics.Num() > 0
                    ? FString::Printf(TEXT("%s: %s"), *Diagnostics[0].Code, *Diagnostics[0].Message)
                    : TEXT("core:diagnostic.ui_mutation.prepare_failed: Prepare failed on collection item");
                return false;
            }

            FPreparedCollectionItem PreparedItem;
            PreparedItem.Key = ItemKey;
            PreparedItem.Widget = ItemWidget;
            PreparedItem.Plan = ItemPlan;
            PreparedItem.bIsHost = true;
            PreparedItem.bIsReused = bItemReused;
            PreparedItem.CommittedValue = ItemVal.AsObjectRef();
            PreparedItem.PreviousCommittedSnapshot = MoveTemp(PreviousItemSnapshot);

            // GBF-04 (ADR-0041): every direct item mutation receives an inverse. A
            // reused entry is restored from its committed schema snapshot; a fresh entry
            // has an empty prior state, so the candidate schema produces its all-Reset
            // inverse. Both branches remain off-panel until the collection publishes.
            const GV2ContentCore::FCompiledUiFieldSpec& RollbackSchema = bHasPreviousItemSnapshot
                ? *PreviousItemSchema
                : *CompiledItemSpec;
            const FString& RollbackSchemaId = bHasPreviousItemSnapshot
                ? PreviousItemSchemaId
                : (ContextSchemaId.IsEmpty() ? TEXT("core:schema.ui_value.collection_item.v1") : ContextSchemaId);
            TSharedPtr<FGV2UiHostMutationPlan> ItemRollbackPlan = MakeShared<FGV2UiHostMutationPlan>();
            TArray<FGV2UiSchemaCompatibilityDiagnostic> RollbackDiagnostics;
            if (!PrepareUiHostRollbackPlan(
                    Cast<UUserWidget>(ItemWidget),
                    ItemCaps,
                    *ItemPlan,
                    PreviousItemValue,
                    RollbackSchema,
                    RollbackSchemaId,
                    *CompiledItemSpec,
                    ContextSchemaId.IsEmpty() ? TEXT("core:schema.ui_value.collection_item.v1") : ContextSchemaId,
                    FullItemPrefix,
                    *ItemRollbackPlan,
                    RollbackDiagnostics,
                    nullptr,
                    PrepareContext))
            {
                OutError = RollbackDiagnostics.Num() > 0
                    ? FString::Printf(TEXT("core:diagnostic.ui_rollback.prepare_failed: collection item '%s' cannot prepare inverse: %s"), *ItemKey.ToString(), *RollbackDiagnostics[0].ToString())
                    : FString::Printf(TEXT("core:diagnostic.ui_rollback.prepare_failed: collection item '%s' cannot prepare inverse"), *ItemKey.ToString());
                return false;
            }
            FString ItemRollbackPlanError;
            if (!ValidateUiRollbackPlan(*ItemPlan, *ItemRollbackPlan, ItemRollbackPlanError))
            {
                OutError = FString::Printf(
                    TEXT("core:diagnostic.ui_rollback.prepare_failed: collection item '%s' has no valid inverse: %s"),
                    *ItemKey.ToString(), *ItemRollbackPlanError);
                return false;
            }
            PreparedItem.RollbackPlan = MoveTemp(ItemRollbackPlan);

            // PSC-10B: a collection item is created here, during Prepare, so its central
            // style is prepared here too. No context means no style for a widget that
            // resolves none of its own -- an error, not a silent skip.
            if (PrepareContext == nullptr)
            {
                OutError = FString::Printf(
                    TEXT("core:diagnostic.ui_central_style.missing_prepare_context: collection item '%s'"),
                    *ItemKey.ToString());
                return false;
            }
            if (!GV2CentralStylePreparer::PrepareForSubtree(
                    ItemWidget,
                    *PrepareContext,
                    PreparedItem.CentralStyleTransaction,
                    OutError))
            {
                return false;
            }

            PreparedItems.Add(MoveTemp(PreparedItem));
        }
        else
        {
            if (CompiledItemSpec != nullptr)
            {
                const FString FullItemPrefix = ContextPropertyPath.IsEmpty()
                    ? FString::Printf(TEXT("[%s]"), *ItemKey.ToString())
                    : FString::Printf(TEXT("%s[%s]"), *ContextPropertyPath, *ItemKey.ToString());
                TArray<FGV2UiSchemaCompatibilityDiagnostic> DiscrepancyDiags;
                if (!CheckUiSchemaCapabilityCompatibility(
                        *CompiledItemSpec,
                        ItemCaps,
                        ContextSchemaId,
                        FullItemPrefix,
                        DiscrepancyDiags))
                {
                    for (const FGV2UiSchemaCompatibilityDiagnostic& Diag : DiscrepancyDiags)
                    {
                        FGV2CollectionItemDiscrepancy Discrepancy;
                        Discrepancy.ScreenId = ContextScreenId;
                        Discrepancy.FieldId = ContextFieldId;
                        Discrepancy.SchemaId = Diag.SchemaId;
                        Discrepancy.PropertyPath = Diag.PropertyPath;
                        Discrepancy.WidgetClass = ItemWidget ? ItemWidget->GetClass()->GetName() : TEXT("None");
                        Discrepancy.Capability = Diag.PropertyPath;
                        Discrepancy.Code = Diag.Code;
                        Discrepancy.Message = Diag.Message;

                        Discrepancies.Add(Discrepancy);
                        GAllRecordedDiscrepancies.Add(Discrepancy);

                        UE_LOG(LogTemp, Display, TEXT("%s"), *Discrepancy.ToLogString());
                    }

                    OutError = DiscrepancyDiags.Num() > 0
                        ? FString::Printf(TEXT("%s: %s"), *DiscrepancyDiags[0].Code, *DiscrepancyDiags[0].Message)
                        : TEXT("core:diagnostic.ui_mutation.prepare_failed: Item schema incompatible with entry widget");
                    return false;
                }
            }

            FPreparedCollectionItem PreparedItem;
            PreparedItem.Key = ItemKey;
            PreparedItem.Widget = ItemWidget;
            PreparedItem.bIsHost = false;
            PreparedItems.Add(MoveTemp(PreparedItem));
        }
    }

    bCollectionPrepareAccepted = true;
    return true;
}

// GBF-07: rollback_delegate=KeyedCollection
bool FGV2KeyedCollectionPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    const TFunction<bool(const FString& PropertyPath)> NoFailureInjector;
    return CommitWithFailureInjector(TargetWidget, OutError, NoFailureInjector, FString());
}

// GBF-07: rollback_boundary=KeyedCollection
bool FGV2KeyedCollectionPropertyConsumer::CommitWithFailureInjector(
    UWidget* TargetWidget,
    FString& OutError,
    const TFunction<bool(const FString& PropertyPath)>& FailureInjector,
    const FString& PropertyPath)
{
    if (!TargetWidget)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.missing_target: Target widget is null during Commit");
        return false;
    }

    for (int32 ItemIndex = 0; ItemIndex < PreparedItems.Num(); ++ItemIndex)
    {
        FPreparedCollectionItem& Item = PreparedItems[ItemIndex];
        if (Item.bIsHost && Item.Plan)
        {
            TFunction<bool(const FString&)> ItemFailureInjector = nullptr;
            if (FailureInjector)
            {
                const FString ItemPrefix = FString::Printf(TEXT("%s[%s]"), *PropertyPath, *Item.Key.ToString());
                ItemFailureInjector = [FailureInjector, ItemPrefix](const FString& ChildPropertyPath)
                {
                    return FailureInjector(FString::Printf(TEXT("%s.%s"), *ItemPrefix, *ChildPropertyPath));
                };
            }
            FString FailedPath, CommitError;
            const FGV2UiHostMutationPlan* ItemRollbackPlan = Item.RollbackPlan.IsValid()
                ? Item.RollbackPlan.Get()
                : nullptr;
            const bool bPropertiesCommitted = CommitUiHostProperties(
                Cast<UUserWidget>(Item.Widget),
                *Item.Plan,
                FailedPath,
                CommitError,
                ItemFailureInjector,
                ItemRollbackPlan);
            bool bItemCommitted = bPropertiesCommitted;
            if (bItemCommitted)
            {
                bItemCommitted = GV2ApplyTransaction(
                        Item.CentralStyleTransaction,
                        CommitError);
            }
            if (!bItemCommitted)
            {
                // A style failure happens after this item's fields committed. Restore the
                // item's logical value before rolling back already committed siblings.
                if (bPropertiesCommitted && ItemRollbackPlan != nullptr)
                {
                    FString RollbackFailedPath, RollbackError;
                    if (!CommitUiHostProperties(
                            Cast<UUserWidget>(Item.Widget),
                            *ItemRollbackPlan,
                            RollbackFailedPath,
                            RollbackError))
                    {
                        CommitError = FString::Printf(
                            TEXT("%s: central style failed and item rollback failed on '%s': %s"),
                            GGV2UiRollbackFailedDiagnosticCode,
                            *RollbackFailedPath,
                            *RollbackError);
                    }
                    else if (IGV2UiPropertyHost* ItemHost = Cast<IGV2UiPropertyHost>(Item.Widget))
                    {
                        GetUiHostSemanticState(ItemHost->GetPropertyHostState()).RestoreCommittedSnapshot(
                            Item.PreviousCommittedSnapshot);
                    }
                }
                // GBH-10 (ADR-0041): this item's own properties already self-healed via
                // ItemRollbackPlan if it had one. Items committed earlier in this same call may be
                // reused entries already visible with their new value -- Panel/
                // ActiveWidgetsByKey have not advanced yet (that only happens below,
                // once every item commits cleanly), so this collection is not
                // publishing this revision and no reused entry may be left on it.
                bool bSiblingRollbackFailed = false;
                for (int32 RollbackIndex = ItemIndex - 1; RollbackIndex >= 0; --RollbackIndex)
                {
                    FPreparedCollectionItem& CommittedItem = PreparedItems[RollbackIndex];
                    if (CommittedItem.RollbackPlan.IsValid())
                    {
                        FString RollbackFailedPath, RollbackError;
                        if (!CommitUiHostProperties(Cast<UUserWidget>(CommittedItem.Widget), *CommittedItem.RollbackPlan, RollbackFailedPath, RollbackError))
                        {
                            bSiblingRollbackFailed = true;
                            UE_LOG(LogTemp, Error,
                                TEXT("GBH-10: rollback failed restoring collection item '%s' property '%s': %s -- invariant violation"),
                                *CommittedItem.Key.ToString(), *RollbackFailedPath, *RollbackError);
                        }
                        else if (IGV2UiPropertyHost* CommittedItemHost = Cast<IGV2UiPropertyHost>(CommittedItem.Widget))
                        {
                            GetUiHostSemanticState(CommittedItemHost->GetPropertyHostState()).RestoreCommittedSnapshot(
                                CommittedItem.PreviousCommittedSnapshot);
                        }
                    }
                }
                OutError = (bSiblingRollbackFailed && !CommitError.Contains(GGV2UiRollbackFailedDiagnosticCode))
                    ? FString::Printf(TEXT("%s: %s"), GGV2UiRollbackFailedDiagnosticCode, *CommitError)
                    : CommitError;
                return false;
            }
        }
    }

    // The widget-touching tail (panel reconciliation, active-widget map and Dropdown
    // header) flows through a transaction. The relevant APIs are GV2-owned, so the adapter performs
    // the actual dispatch this used to perform directly. GBH-10's bookkeeping loop
    // (SetLastCommittedSnapshot) stays here unchanged -- it updates only
    // FGV2UiPropertyHostState accounting on each item host, never a widget.
    GV2PresentationApply::FPreparedKeyedCollectionOperation Operation;
    Operation.TargetWidget = TargetWidget;
    Operation.OrderedEntries.Reserve(PreparedItems.Num());
    for (const FPreparedCollectionItem& Item : PreparedItems)
    {
        GV2PresentationApply::FPreparedKeyedCollectionEntry Entry;
        Entry.Key = Item.Key;
        Entry.Widget = Item.Widget;
        Operation.OrderedEntries.Add(MoveTemp(Entry));
    }
    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
    Transaction.AddKeyedCollectionOperation(MoveTemp(Operation));
    if (!GV2ApplyTransaction(Transaction, OutError))
    {
        return false;
    }

    // GBH-10 (ADR-0041): only reached once every item above committed cleanly. Nothing
    // else in this consumer ever recorded a collection item's own LastCommittedProperties
    // -- without this, a future revision's RollbackPlan for a reused item would have
    // only an empty object to prepare against (an all-Reset plan, not an actual restore).
    for (const FPreparedCollectionItem& Item : PreparedItems)
    {
        if (Item.bIsHost && Item.CommittedValue.IsValid())
        {
            if (IGV2UiPropertyHost* ItemHost = Cast<IGV2UiPropertyHost>(Item.Widget))
            {
                GetUiHostSemanticState(ItemHost->GetPropertyHostState()).SetLastCommittedSnapshot(
                    *Item.CommittedValue,
                    CompiledItemSpec,
                    ContextSchemaId.IsEmpty() ? TEXT("core:schema.ui_value.collection_item.v1") : ContextSchemaId);
            }
        }
    }

    ActiveWidgetsByKey.Reset();
    for (const auto& Pair : CandidateWidgetsByKey)
    {
        ActiveWidgetsByKey.Add(Pair.Key, Pair.Value.Get());
    }
    CandidateWidgetsByKey.Reset();

    return true;
}

void FGV2KeyedCollectionPropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (TargetWidget)
    {
        GV2PresentationApply::FPreparedKeyedCollectionOperation Operation;
        Operation.TargetWidget = TargetWidget;
        Operation.bIsReset = true;
        GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
        Transaction.AddKeyedCollectionOperation(MoveTemp(Operation));
        FString ApplyError;
        GV2ApplyTransaction(Transaction, ApplyError);
    }
    ActiveWidgetsByKey.Reset();
    CandidateWidgetsByKey.Reset();
    PreparedItems.Reset();
}

// --- FGV2RichTextSpansPropertyConsumer ---

bool FGV2RichTextSpansPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsArray();
}

bool FGV2RichTextSpansPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    // GBF-05 (mirrors FGV2TabContainerTabsPropertyConsumer::Prepare): leaving a partial
    // PreparedSpans behind after a rejected Prepare would make this consumer committable
    // on a prefix of a transaction that was never accepted.
    bool bSpansPrepareAccepted = false;
    bHasAcceptedRevision = false;
    ON_SCOPE_EXIT
    {
        if (!bSpansPrepareAccepted)
        {
            PreparedSpans.Reset();
        }
    };

    PreparedSpans.Reset();
    if (!Value.IsArray())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.kind_mismatch: Expected Array kind for RichText spans");
        return false;
    }

    UGV2RichTextWidgetBase* Owner = Cast<UGV2RichTextWidgetBase>(TargetWidget);
    if (!Owner && TargetWidget)
    {
        Owner = TargetWidget->GetTypedOuter<UGV2RichTextWidgetBase>();
    }

    TSet<FName> SeenKeys;

    for (const FGV2PreparedUiValue& Item : Value.AsArray())
    {
        if (!Item.IsObject())
        {
            OutError = TEXT("core:diagnostic.ui_consumer.item_kind_mismatch: Expected Object for RichText span item");
            return false;
        }

        const FGV2PreparedUiObject& ItemObj = Item.AsObject();
        const FGV2PreparedUiValue* KeyVal = ItemObj.FindField(TEXT("key"));
        if (!KeyVal || !KeyVal->IsKey() || KeyVal->AsKey().IsEmpty())
        {
            OutError = TEXT("core:diagnostic.ui_consumer.missing_span_key: RichText span requires a non-empty key");
            return false;
        }

        const FName ItemKey = FName(*KeyVal->AsKey());
        if (SeenKeys.Contains(ItemKey))
        {
            OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.duplicate_span_key: Duplicate span key '%s'"), *ItemKey.ToString());
            return false;
        }
        SeenKeys.Add(ItemKey);

        FPreparedSpanItem PreparedItem;
        FGV2RichTextSpanViewModel& Span = PreparedItem.Span;
        Span.Key = ItemKey;
        Span.SpanId = ItemKey;

        if (const FGV2PreparedUiValue* SpanIdVal = ItemObj.FindField(TEXT("span_id")))
        {
            if (SpanIdVal->IsKey() && !SpanIdVal->AsKey().IsEmpty())
            {
                Span.SpanId = FName(*SpanIdVal->AsKey());
            }
            else if (SpanIdVal->IsString() && !SpanIdVal->AsString().IsEmpty())
            {
                Span.SpanId = FName(*SpanIdVal->AsString());
            }
        }

        if (!GV2StableIdUE::IsValidSegment(Span.SpanId.ToString()))
        {
            OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.invalid_span_id: Invalid span ID '%s'"), *Span.SpanId.ToString());
            return false;
        }

        if (const FGV2PreparedUiValue* BindingVal = ItemObj.FindField(TEXT("binding")))
        {
            if (BindingVal->IsBinding())
            {
                Span.Binding = BindingVal->AsBinding();
            }
        }

        // PEP-05 (ADR-0040, DUC-09/10/11): hover is a nested screen -- the same
        // resolve/instantiate/prepare-child-fields/prepare-central-style sequence
        // FGV2TabContainerTabsPropertyConsumer::Prepare runs for each tab's own screen,
        // just for a single embedded screen instead of a keyed array of them.
        if (const FGV2PreparedUiValue* HoverVal = ItemObj.FindField(TEXT("hover")))
        {
            if (!HoverVal->IsNull())
            {
                // PEP-06B: the hover popover is gone as a Theme-resolved renderer class --
                // a hovered span's content is now the resolved hover screen itself
                // (host-local overlay_stack participant), so there is nothing left to check
                // for availability here; the screen_id resolution below is the only gate.
                if (!HoverVal->IsObject())
                {
                    OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.item_kind_mismatch: Span '%s' hover must be an object"), *ItemKey.ToString());
                    return false;
                }
                const FGV2PreparedUiObject& HoverObj = HoverVal->AsObject();

                const FGV2PreparedUiValue* ScreenIdVal = HoverObj.FindField(TEXT("screen_id"));
                if (!ScreenIdVal || (!ScreenIdVal->IsStableId() && !ScreenIdVal->IsString()))
                {
                    OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.missing_tab_screen_id: Span '%s' hover requires a screen_id"), *ItemKey.ToString());
                    return false;
                }
                const FString HoverScreenId = ScreenIdVal->IsStableId() ? ScreenIdVal->AsStableId().Id : ScreenIdVal->AsString();
                if (!GV2StableIdUE::IsOfKind(HoverScreenId, "screen"))
                {
                    OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.invalid_tab_screen_id: Span '%s' hover has invalid screen_id '%s'"), *ItemKey.ToString(), *HoverScreenId);
                    return false;
                }

                // DUC-11: composition cycle guard, same reasoning as
                // FGV2TabContainerTabsPropertyConsumer::Prepare's own check.
                if (ActiveCompositionChain != nullptr && ActiveCompositionChain->Contains(HoverScreenId))
                {
                    const FString Chain = FString::Join(*ActiveCompositionChain, TEXT(" -> ")) + TEXT(" -> ") + HoverScreenId;
                    OutError = FString::Printf(
                        TEXT("core:diagnostic.ui_composition.cycle_detected: Span '%s' hover composition cycle: %s"),
                        *ItemKey.ToString(), *Chain);
                    return false;
                }

                FGV2ResolvedScreenDescriptor Descriptor;
                FGV2ScreenResolutionRejection Rejection;
                bool bScreenResolved = false;
                if (PrepareContext == nullptr)
                {
                    Rejection.Message = TEXT("core:diagnostic.ui_screen_registry.no_resolver_available: no PrepareContext is available");
                }
                else
                {
                    bScreenResolved = PrepareContext->ResolveScreen(HoverScreenId, FGV2ScreenPlacement::Embedded(), Descriptor, Rejection);
                }
                if (!bScreenResolved)
                {
                    OutError = FString::Printf(
                        TEXT("core:diagnostic.ui_consumer.unregistered_screen_id: Screen '%s' for span '%s' hover: %s"),
                        *HoverScreenId, *ItemKey.ToString(), *Rejection.Message);
                    return false;
                }
                const TSubclassOf<UGV2ScreenWidgetBase> TargetWidgetClass = Descriptor.WidgetClass;

                // Reuse the previous revision's already-prepared instance for this span,
                // the same off-tree-candidate-reuse FGV2TabContainerTabsPropertyConsumer
                // gets from GetScreenWidgetForTab -- here read directly off the owner's own
                // CurrentSpans/FindInteractiveSpan, which already is that cache.
                TObjectPtr<UGV2ScreenWidgetBase> ChildWidget = nullptr;
                if (Owner != nullptr)
                {
                    if (const FGV2RichTextSpanViewModel* Existing = Owner->FindInteractiveSpan(ItemKey))
                    {
                        ChildWidget = Cast<UGV2ScreenWidgetBase>(Existing->Hover.ScreenWidget.Get());
                    }
                }
                if (!ChildWidget && TargetWidget != nullptr)
                {
                    if (UWorld* World = TargetWidget->GetWorld())
                    {
                        ChildWidget = CreateWidget<UGV2ScreenWidgetBase>(World, TargetWidgetClass);
                    }
                }
                if (ChildWidget == nullptr)
                {
                    OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.missing_target: Span '%s' hover screen widget could not be instantiated"), *ItemKey.ToString());
                    return false;
                }

                Span.Hover.ScreenId = HoverScreenId;
                Span.Hover.ScreenWidget = ChildWidget;
                PreparedItem.HoverScreenWidgetClass = TargetWidgetClass;

                const FGV2PreparedUiValue* FieldsVal = HoverObj.FindField(TEXT("fields"));
                if (FieldsVal != nullptr && !FieldsVal->IsNull())
                {
                    if (!FieldsVal->IsArray())
                    {
                        OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.kind_mismatch: Span '%s' hover fields must be an array"), *ItemKey.ToString());
                        return false;
                    }

                    TArray<FGV2ScreenFieldValue> NestedFields;
                    for (const FGV2PreparedUiValue& EnvelopeVal : FieldsVal->AsArray().GetElements())
                    {
                        if (!EnvelopeVal.IsObject())
                        {
                            OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.item_kind_mismatch: Span '%s' hover field envelope must be an object"), *ItemKey.ToString());
                            return false;
                        }
                        const FGV2PreparedUiObject& EnvelopeObj = EnvelopeVal.AsObject();
                        const FGV2PreparedUiValue* FieldIdVal = EnvelopeObj.FindField(TEXT("field_id"));
                        const FGV2PreparedUiValue* SchemaIdVal = EnvelopeObj.FindField(TEXT("schema_id"));
                        const FGV2PreparedUiValue* InnerValueVal = EnvelopeObj.FindField(TEXT("value"));
                        if (FieldIdVal == nullptr || !FieldIdVal->IsKey()
                            || SchemaIdVal == nullptr || !SchemaIdVal->IsString()
                            || InnerValueVal == nullptr || !InnerValueVal->IsObject())
                        {
                            OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.malformed_screen_field_envelope: Span '%s' hover has a malformed nested field envelope"), *ItemKey.ToString());
                            return false;
                        }

                        const FString SchemaIdStr = SchemaIdVal->AsString();
                        FString SchemaError;
                        const std::shared_ptr<const GV2ContentCore::FCompiledUiFieldSpec> NestedSchema =
                            GV2ScreenFieldMaterializer::GetCompiledSchema(*PrepareContext, TCHAR_TO_UTF8(*SchemaIdStr), SchemaError);
                        if (!NestedSchema)
                        {
                            OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.unknown_schema: Span '%s' hover nested field schema '%s' could not be compiled: %s"), *ItemKey.ToString(), *SchemaIdStr, *SchemaError);
                            return false;
                        }

                        FGV2ScreenFieldValue& NestedField = NestedFields.AddDefaulted_GetRef();
                        NestedField.FieldId = FName(FieldIdVal->AsKey());
                        NestedField.SchemaId = SchemaIdStr;
                        NestedField.PreparedValue = InnerValueVal->AsObjectRef();
                        NestedField.CompiledSchema = NestedSchema;
                    }

                    TArray<FString> ChildCompositionChain;
                    if (ActiveCompositionChain != nullptr)
                    {
                        ChildCompositionChain = *ActiveCompositionChain;
                    }
                    ChildCompositionChain.Add(HoverScreenId);

                    PreparedItem.HoverChildScreenPlan = MakeShared<FGV2ScreenMutationPlan>();
                    FString ChildPrepareError;
                    if (!ChildWidget->PrepareScreenFields(
                            NestedFields,
                            *PreparedItem.HoverChildScreenPlan,
                            ChildPrepareError,
                            &ChildCompositionChain,
                            PrepareContext))
                    {
                        OutError = FString::Printf(TEXT("core:diagnostic.ui_mutation.prepare_failed: Span '%s' hover nested screen fields failed to prepare: %s"), *ItemKey.ToString(), *ChildPrepareError);
                        return false;
                    }
                    PreparedItem.bHoverHasChildPlan = true;
                }

                if (PrepareContext == nullptr)
                {
                    OutError = FString::Printf(TEXT("core:diagnostic.ui_central_style.missing_prepare_context: span '%s' hover"), *ItemKey.ToString());
                    return false;
                }
                if (!GV2CentralStylePreparer::PrepareForSubtree(
                        ChildWidget,
                        *PrepareContext,
                        PreparedItem.HoverCentralStyleTransaction,
                        OutError))
                {
                    return false;
                }
            }
        }

        if (Span.Hover.IsEmpty() && !Span.Binding.IsValid())
        {
            OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.empty_span: Span '%s' must have hover or binding"), *ItemKey.ToString());
            return false;
        }

        PreparedSpans.Add(MoveTemp(PreparedItem));
    }

    bSpansPrepareAccepted = true;
    bHasAcceptedRevision = true;
    return true;
}

namespace
{
GV2PresentationApply::FPreparedRichTextSpan FlattenRichTextSpan(const FGV2RichTextSpanViewModel& Span)
{
    GV2PresentationApply::FPreparedRichTextSpan Flattened;
    Flattened.SpanId = Span.SpanId;
    Flattened.Key = Span.Key;
    Flattened.Hover.ScreenId = Span.Hover.ScreenId;
    Flattened.Hover.ScreenWidget = Span.Hover.ScreenWidget.Get();
    Flattened.SerializedBinding = Span.Binding.ToString();
    return Flattened;
}
}

bool FGV2RichTextSpansPropertyConsumer::BuildPreparedOperation(
    UWidget* TargetWidget,
    GV2PresentationApply::FGV2PreparedPresentationTransaction& OutTransaction,
    FString& OutError) const
{
    GV2PresentationApply::FPreparedRichTextSpansOperation Operation;
    Operation.TargetWidget = TargetWidget;
    Operation.Spans.Reserve(PreparedSpans.Num());
    for (const FPreparedSpanItem& Item : PreparedSpans)
    {
        Operation.Spans.Add(FlattenRichTextSpan(Item.Span));
    }
    OutTransaction.AddRichTextSpansOperation(MoveTemp(Operation));
    OutError.Reset();
    return true;
}

// GBF-07: rollback_boundary=RichTextSpansHover
bool FGV2RichTextSpansPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    // GBF-05: nothing was accepted, so there is nothing to publish -- mirrors
    // FGV2TabContainerTabsPropertyConsumer::CommitWithFailureInjector's own guard.
    if (!bHasAcceptedRevision)
    {
        return true;
    }

    // Commit each span's own hover nested screen fields first, through the same
    // CommitScreenFields a top-level screen uses (DUC-09), with sibling rollback on
    // failure -- mirrors FGV2TabContainerTabsPropertyConsumer::CommitWithFailureInjector.
    for (int32 SpanIndex = 0; SpanIndex < PreparedSpans.Num(); ++SpanIndex)
    {
        FPreparedSpanItem& Item = PreparedSpans[SpanIndex];
        if (Item.bHoverHasChildPlan && Item.HoverChildScreenPlan.IsValid() && Item.Span.Hover.ScreenWidget.IsValid())
        {
            UGV2ScreenWidgetBase* ChildWidget = Cast<UGV2ScreenWidgetBase>(Item.Span.Hover.ScreenWidget.Get());
            FString ChildCommitError;
            if (ChildWidget == nullptr || !ChildWidget->CommitScreenFields(*Item.HoverChildScreenPlan, ChildCommitError, nullptr))
            {
                bool bSiblingRollbackFailed = false;
                for (int32 RollbackIndex = SpanIndex - 1; RollbackIndex >= 0; --RollbackIndex)
                {
                    const FPreparedSpanItem& CommittedItem = PreparedSpans[RollbackIndex];
                    if (CommittedItem.bHoverHasChildPlan && CommittedItem.HoverChildScreenPlan.IsValid())
                    {
                        const FGV2UiRollbackResult SiblingRollback = RollbackFieldPlans(CommittedItem.HoverChildScreenPlan->FieldPlans);
                        bSiblingRollbackFailed |= !SiblingRollback.bRestored;
                    }
                }
                OutError = FString::Printf(
                    TEXT("nested screen fields commit failed for span '%s' hover: %s"),
                    *Item.Span.Key.ToString(), *ChildCommitError);
                if (bSiblingRollbackFailed && !OutError.Contains(GGV2UiRollbackFailedDiagnosticCode))
                {
                    OutError = FString::Printf(TEXT("%s: %s"), GGV2UiRollbackFailedDiagnosticCode, *OutError);
                }
                return false;
            }
        }

        if (Item.Span.Hover.ScreenWidget.IsValid())
        {
            FString StyleError;
            if (!GV2ApplyTransaction(Item.HoverCentralStyleTransaction, StyleError))
            {
                bool bRollbackFailed = false;
                if (Item.bHoverHasChildPlan && Item.HoverChildScreenPlan.IsValid())
                {
                    const FGV2UiRollbackResult CurrentRollback = RollbackFieldPlans(Item.HoverChildScreenPlan->FieldPlans);
                    bRollbackFailed |= !CurrentRollback.bRestored;
                }
                for (int32 RollbackIndex = SpanIndex - 1; RollbackIndex >= 0; --RollbackIndex)
                {
                    const FPreparedSpanItem& CommittedItem = PreparedSpans[RollbackIndex];
                    if (CommittedItem.bHoverHasChildPlan && CommittedItem.HoverChildScreenPlan.IsValid())
                    {
                        const FGV2UiRollbackResult SiblingRollback = RollbackFieldPlans(CommittedItem.HoverChildScreenPlan->FieldPlans);
                        bRollbackFailed |= !SiblingRollback.bRestored;
                    }
                }
                OutError = FString::Printf(
                    TEXT("nested screen central style commit failed for span '%s' hover: %s"),
                    *Item.Span.Key.ToString(), *StyleError);
                if (bRollbackFailed && !OutError.Contains(GGV2UiRollbackFailedDiagnosticCode))
                {
                    OutError = FString::Printf(TEXT("%s: %s"), GGV2UiRollbackFailedDiagnosticCode, *OutError);
                }
                return false;
            }
        }
    }

    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
    if (!BuildPreparedOperation(TargetWidget, Transaction, OutError))
    {
        return false;
    }
    return GV2ApplyTransaction(Transaction, OutError);
}

void FGV2RichTextSpansPropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (TargetWidget)
    {
        GV2PresentationApply::FPreparedRichTextSpansOperation Operation;
        Operation.TargetWidget = TargetWidget;
        GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
        Transaction.AddRichTextSpansOperation(MoveTemp(Operation));
        FString ApplyError;
        GV2ApplyTransaction(Transaction, ApplyError);
    }
    PreparedSpans.Reset();
}

// --- FGV2TabContainerTabsPropertyConsumer ---

FGV2TabContainerTabsPropertyConsumer::FGV2TabContainerTabsPropertyConsumer() = default;
FGV2TabContainerTabsPropertyConsumer::~FGV2TabContainerTabsPropertyConsumer() = default;

bool FGV2TabContainerTabsPropertyConsumer::CanConsume(const FGV2PreparedUiValue& Value) const
{
    return Value.IsArray();
}

// PAH-08: phase=prepare -- resolves each tab's screen and nested field schemas
// off-tree; this is the legitimate both-phases-in-one-file case the gate's
// function granularity exists for (its Commit is in this same translation unit).
bool FGV2TabContainerTabsPropertyConsumer::Prepare(
    const FGV2PreparedUiValue& Value,
    const FGV2UiPropertyCapability& Capability,
    UWidget* TargetWidget,
    FString& OutError)
{
    PreparedTabs.Reset();
    CandidateWidgetsByKey.Reset();

    // GBF-05: Prepare accumulates one entry per tab and rejects from many points
    // after the first entries are already built. Leaving them behind makes the
    // consumer committable after a rejected Prepare, and Commit would then apply
    // the prefix of a transaction that was never accepted -- the partial state
    // ADR-0041 forbids. The guard covers every exit, including ones added later.
    bool bTabsPrepareAccepted = false;
    bHasAcceptedRevision = false;
    ON_SCOPE_EXIT
    {
        if (!bTabsPrepareAccepted)
        {
            PreparedTabs.Reset();
            CandidateWidgetsByKey.Reset();
        }
    };

    if (!Value.IsArray())
    {
        OutError = TEXT("core:diagnostic.ui_consumer.value_kind_mismatch: Expected Array for tab container tabs");
        return false;
    }

    const TArray<FGV2PreparedUiValue>& TabsArray = Value.AsArray().GetElements();
    if (TabsArray.Num() == 0)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.empty_tabs: Tab container requires a non-empty tabs array");
        return false;
    }

    UGV2TabContainerWidgetBase* TabContainer = Cast<UGV2TabContainerWidgetBase>(TargetWidget);
    if (!TabContainer && TargetWidget)
    {
        TabContainer = TargetWidget->GetTypedOuter<UGV2TabContainerWidgetBase>();
    }

    TSet<FName> SeenKeys;

    for (const FGV2PreparedUiValue& TabVal : TabsArray)
    {
        if (!TabVal.IsObject())
        {
            OutError = TEXT("core:diagnostic.ui_consumer.item_kind_mismatch: Each tab item must be an object");
            return false;
        }

        const FGV2PreparedUiObject& TabObj = TabVal.AsObject();

        // 1. Key
        const FGV2PreparedUiValue* KeyVal = TabObj.FindField(TEXT("key"));
        if (!KeyVal || !KeyVal->IsKey() || KeyVal->AsKey().IsEmpty())
        {
            OutError = TEXT("core:diagnostic.ui_consumer.missing_tab_key: Tab requires a non-empty key");
            return false;
        }

        const FName TabKey = FName(*KeyVal->AsKey());
        if (SeenKeys.Contains(TabKey))
        {
            OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.duplicate_tab_key: Duplicate tab key '%s'"), *TabKey.ToString());
            return false;
        }
        SeenKeys.Add(TabKey);

        // 2. Title
        const FGV2PreparedUiValue* TitleVal = TabObj.FindField(TEXT("title"));
        if (!TitleVal || !TitleVal->IsText())
        {
            OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.missing_tab_title: Tab '%s' requires a text title"), *TabKey.ToString());
            return false;
        }

        // 3. ScreenId
        const FGV2PreparedUiValue* ScreenIdVal = TabObj.FindField(TEXT("screen_id"));
        if (!ScreenIdVal || (!ScreenIdVal->IsStableId() && !ScreenIdVal->IsString()))
        {
            OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.missing_tab_screen_id: Tab '%s' requires a screen_id"), *TabKey.ToString());
            return false;
        }

        const FString TabScreenId = ScreenIdVal->IsStableId() ? ScreenIdVal->AsStableId().Id : ScreenIdVal->AsString();
        if (!GV2StableIdUE::IsOfKind(TabScreenId, "screen"))
        {
            OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.invalid_tab_screen_id: Tab '%s' has invalid screen_id '%s'"), *TabKey.ToString(), *TabScreenId);
            return false;
        }

        // 3b. DUC-11: composition cycle guard. screen_id is a runtime string looked
        // up via Screen Registry, not a compile-time Blueprint class reference, so
        // UMG's own circular-dependency detection (WidgetTree class graph, checked
        // by the Designer and the Blueprint compiler) has no visibility into this
        // edge -- a Designer-placed self-containment is already impossible, but
        // nothing stops content from declaring a tab whose screen_id is the screen
        // currently being prepared, directly or through an intermediate screen.
        // ActiveCompositionChain is the ordered path of screen_ids already being
        // prepared on this call stack (seeded at the root by
        // FGV2LayeredUiReconciler::PrepareReconcile); TabScreenId reappearing on it
        // is rejected before Ready, exactly where DUC-05's missing-target check and
        // DUC-07's kind-mismatch check already reject their own drift.
        if (ActiveCompositionChain != nullptr && ActiveCompositionChain->Contains(TabScreenId))
        {
            const FString Chain = FString::Join(*ActiveCompositionChain, TEXT(" -> ")) + TEXT(" -> ") + TabScreenId;
            OutError = FString::Printf(
                TEXT("core:diagnostic.ui_composition.cycle_detected: Tab '%s' composition cycle: %s"),
                *TabKey.ToString(), *Chain);
            return false;
        }

        // 4. Resolve ScreenId in UGV2ScreenRegistry. PAH-02: this is the Embedded
        // placement -- a screen registered only for a GameShell top-level layer is
        // rejected here now, instead of being handed out on nothing more than
        // Entry->WidgetClass being non-null.
        // PSC-08 (ADR-0043 D1, PAH-R4): there is no default/generic WidgetClass here
        // anymore -- every branch below either resolves TargetWidgetClass from a
        // successful Resolve() or falls into the shared rejection below. "Neither a
        // PrepareContext nor a configured Registry is available" used to silently leave
        // TargetWidgetClass at UGV2ScreenWidgetBase::StaticClass() and proceed to
        // instantiate a blank generic screen -- that is now itself a typed Prepare
        // failure (EGV2ScreenResolutionError::UnknownScreenId), not a resolver-less
        // fallthrough.
        FGV2ResolvedScreenDescriptor Descriptor;
        FGV2ScreenResolutionRejection Rejection;
        bool bScreenResolved = false;
        if (PrepareContext == nullptr)
        {
            Rejection.Code = EGV2ScreenResolutionError::UnknownScreenId;
            Rejection.Message = TEXT("core:diagnostic.ui_screen_registry.no_resolver_available: no PrepareContext is available");
        }
        else
        {
            bScreenResolved = PrepareContext->ResolveScreen(TabScreenId, FGV2ScreenPlacement::Embedded(), Descriptor, Rejection);
        }

        if (!bScreenResolved)
        {
            OutError = FString::Printf(
                TEXT("core:diagnostic.ui_consumer.unregistered_screen_id: Screen '%s' for tab '%s': %s"),
                *TabScreenId, *TabKey.ToString(), *Rejection.Message);
            return false;
        }

        const TSubclassOf<UGV2ScreenWidgetBase> TargetWidgetClass = Descriptor.WidgetClass;

        // 5. Reconcile / instantiate screen widget off-tree
        TObjectPtr<UGV2ScreenWidgetBase> ChildWidget = nullptr;
        if (TabContainer != nullptr)
        {
            ChildWidget = Cast<UGV2ScreenWidgetBase>(TabContainer->GetScreenWidgetForTab(TabKey));
        }

        if (!ChildWidget && TargetWidget != nullptr)
        {
            UWorld* World = TargetWidget->GetWorld();
            if (World != nullptr)
            {
                ChildWidget = CreateWidget<UGV2ScreenWidgetBase>(World, TargetWidgetClass);
            }
        }

        FPreparedTabItem PreparedItem;
        PreparedItem.Key = TabKey;
        PreparedItem.Title = TitleVal->AsText();
        PreparedItem.ScreenId = TabScreenId;
        PreparedItem.ScreenWidgetClass = TargetWidgetClass;
        PreparedItem.ScreenWidget = ChildWidget;

        // 6. Child fields preparation off-tree. DUC-09: `fields` is an array of the
        // same field_id/schema_id/value envelope a top-level screen request uses --
        // no separate protocol. ProjectMaterializedValue's ScreenFields case has
        // already resolved each envelope's own schema_id and fully validated +
        // materialized its value; this only rebuilds real FGV2ScreenFieldValue
        // entries and prepares them through the child screen's own public
        // PrepareScreenFields -- the exact two-phase API a top-level screen uses,
        // not a schema synthesized from the child's capability tree.
        const FGV2PreparedUiValue* FieldsVal = TabObj.FindField(TEXT("fields"));
        if (FieldsVal != nullptr && !FieldsVal->IsNull())
        {
            if (!FieldsVal->IsArray())
            {
                OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.kind_mismatch: Tab '%s' fields must be an array"), *TabKey.ToString());
                return false;
            }
            if (ChildWidget == nullptr)
            {
                OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.missing_target: Tab '%s' has fields but no screen widget instantiated"), *TabKey.ToString());
                return false;
            }

            TArray<FGV2ScreenFieldValue> NestedFields;
            for (const FGV2PreparedUiValue& EnvelopeVal : FieldsVal->AsArray().GetElements())
            {
                if (!EnvelopeVal.IsObject())
                {
                    OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.item_kind_mismatch: Tab '%s' field envelope must be an object"), *TabKey.ToString());
                    return false;
                }
                const FGV2PreparedUiObject& EnvelopeObj = EnvelopeVal.AsObject();
                const FGV2PreparedUiValue* FieldIdVal = EnvelopeObj.FindField(TEXT("field_id"));
                const FGV2PreparedUiValue* SchemaIdVal = EnvelopeObj.FindField(TEXT("schema_id"));
                const FGV2PreparedUiValue* InnerValueVal = EnvelopeObj.FindField(TEXT("value"));
                if (FieldIdVal == nullptr || !FieldIdVal->IsKey()
                    || SchemaIdVal == nullptr || !SchemaIdVal->IsString()
                    || InnerValueVal == nullptr || !InnerValueVal->IsObject())
                {
                    OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.malformed_screen_field_envelope: Tab '%s' has a malformed nested field envelope"), *TabKey.ToString());
                    return false;
                }

                const FString SchemaIdStr = SchemaIdVal->AsString();
                FString SchemaError;
                const std::shared_ptr<const GV2ContentCore::FCompiledUiFieldSpec> NestedSchema =
                    GV2ScreenFieldMaterializer::GetCompiledSchema(*PrepareContext, TCHAR_TO_UTF8(*SchemaIdStr), SchemaError);
                if (!NestedSchema)
                {
                    OutError = FString::Printf(TEXT("core:diagnostic.ui_consumer.unknown_schema: Tab '%s' nested field schema '%s' could not be compiled: %s"), *TabKey.ToString(), *SchemaIdStr, *SchemaError);
                    return false;
                }

                FGV2ScreenFieldValue& NestedField = NestedFields.AddDefaulted_GetRef();
                NestedField.FieldId = FName(FieldIdVal->AsKey());
                NestedField.SchemaId = SchemaIdStr;
                NestedField.PreparedValue = InnerValueVal->AsObjectRef();
                NestedField.CompiledSchema = NestedSchema;
            }

            // DUC-11: extend the composition path with this tab's own screen_id
            // before recursing, so a deeper tab container (inside ChildWidget) sees
            // the full ancestor chain and can catch an indirect cycle through it.
            TArray<FString> ChildCompositionChain;
            if (ActiveCompositionChain != nullptr)
            {
                ChildCompositionChain = *ActiveCompositionChain;
            }
            ChildCompositionChain.Add(TabScreenId);

            PreparedItem.ChildScreenPlan = MakeShared<FGV2ScreenMutationPlan>();
            FString ChildPrepareError;
            if (!ChildWidget->PrepareScreenFields(
                    NestedFields,
                    *PreparedItem.ChildScreenPlan,
                    ChildPrepareError,
                    &ChildCompositionChain,
                    PrepareContext))
            {
                OutError = FString::Printf(TEXT("core:diagnostic.ui_mutation.prepare_failed: Tab '%s' nested screen fields failed to prepare: %s"), *TabKey.ToString(), *ChildPrepareError);
                return false;
            }
            PreparedItem.bHasChildPlan = true;
        }

        if (ChildWidget != nullptr)
        {
            if (PrepareContext == nullptr)
            {
                OutError = FString::Printf(
                    TEXT("core:diagnostic.ui_central_style.missing_prepare_context: tab '%s'"),
                    *TabKey.ToString());
                return false;
            }
            if (!GV2CentralStylePreparer::PrepareForSubtree(
                    ChildWidget,
                    *PrepareContext,
                    PreparedItem.CentralStyleTransaction,
                    OutError))
            {
                return false;
            }
        }

        if (ChildWidget != nullptr)
        {
            CandidateWidgetsByKey.Add(TabKey, TStrongObjectPtr<UGV2ScreenWidgetBase>(ChildWidget));
        }

        PreparedTabs.Add(MoveTemp(PreparedItem));
    }

    bTabsPrepareAccepted = true;
    bHasAcceptedRevision = true;
    return true;
}

// GBF-07: rollback_delegate=NestedScreenTabs
bool FGV2TabContainerTabsPropertyConsumer::Commit(UWidget* TargetWidget, FString& OutError)
{
    const TFunction<bool(const FString& PropertyPath)> NoFailureInjector;
    return CommitWithFailureInjector(TargetWidget, OutError, NoFailureInjector, FString());
}

// GBF-07: rollback_boundary=NestedScreenTabs
bool FGV2TabContainerTabsPropertyConsumer::CommitWithFailureInjector(
    UWidget* TargetWidget,
    FString& OutError,
    const TFunction<bool(const FString& PropertyPath)>& FailureInjector,
    const FString& PropertyPath)
{
    // GBF-05: nothing was accepted, so there is nothing to publish. Falling through
    // would call ApplyTabEntries with an empty list, which empties the tab-widget
    // map -- an application of state, and exactly the partial/unintended publication
    // ADR-0041 forbids after a rejected Prepare.
    if (!bHasAcceptedRevision)
    {
        return true;
    }

    UGV2TabContainerWidgetBase* TabContainer = Cast<UGV2TabContainerWidgetBase>(TargetWidget);
    if (!TabContainer && TargetWidget)
    {
        TabContainer = TargetWidget->GetTypedOuter<UGV2TabContainerWidgetBase>();
    }

    // Commit child screen field plans, through the same CommitScreenFields a
    // top-level screen uses (DUC-09).
    for (int32 TabIndex = 0; TabIndex < PreparedTabs.Num(); ++TabIndex)
    {
        FPreparedTabItem& Item = PreparedTabs[TabIndex];
        if (Item.bHasChildPlan && Item.ChildScreenPlan.IsValid() && Item.ScreenWidget != nullptr)
        {
            TFunction<bool(const FString& PropertyPath)> ChildFailureInjector;
            if (FailureInjector)
            {
                const FString ChildPrefix = FString::Printf(
                    TEXT("%s.%s"),
                    *PropertyPath,
                    *Item.Key.ToString());
                ChildFailureInjector = [FailureInjector, ChildPrefix](const FString& ChildPropertyPath)
                {
                    return FailureInjector(FString::Printf(TEXT("%s.%s"), *ChildPrefix, *ChildPropertyPath));
                };
            }
            FString ChildCommitError;
            if (!Item.ScreenWidget->CommitScreenFields(*Item.ChildScreenPlan, ChildCommitError, ChildFailureInjector))
            {
                // GBH-10 (ADR-0041): this tab's own nested screen already self-healed via
                // CommitScreenFields' internal rollback (and, per PAH-01, already folded
                // GGV2UiRollbackFailedDiagnosticCode into ChildCommitError if that self-heal
                // itself failed). Tabs committed earlier in this same call may be reused
                // nested screens already visible with their new value -- ApplyTabEntries
                // below (which is what actually publishes the new tab list/ActiveTab) never
                // runs on this failure path, so none of them may be left on it.
                bool bSiblingRollbackFailed = false;
                for (int32 RollbackIndex = TabIndex - 1; RollbackIndex >= 0; --RollbackIndex)
                {
                    const FPreparedTabItem& CommittedItem = PreparedTabs[RollbackIndex];
                    if (CommittedItem.bHasChildPlan && CommittedItem.ChildScreenPlan.IsValid())
                    {
                        const FGV2UiRollbackResult SiblingRollback = RollbackFieldPlans(CommittedItem.ChildScreenPlan->FieldPlans);
                        bSiblingRollbackFailed |= !SiblingRollback.bRestored;
                    }
                }
                OutError = FString::Printf(
                    TEXT("nested screen fields commit failed for tab '%s': %s"),
                    *Item.Key.ToString(), *ChildCommitError);
                if (bSiblingRollbackFailed && !OutError.Contains(GGV2UiRollbackFailedDiagnosticCode))
                {
                    OutError = FString::Printf(TEXT("%s: %s"), GGV2UiRollbackFailedDiagnosticCode, *OutError);
                }
                return false;
            }
        }

        if (Item.ScreenWidget != nullptr)
        {
            FString StyleError;
            const bool bStyleApplied = GV2ApplyTransaction(
                    Item.CentralStyleTransaction,
                    StyleError);
            if (!bStyleApplied)
            {
                bool bRollbackFailed = false;
                if (Item.bHasChildPlan && Item.ChildScreenPlan.IsValid())
                {
                    const FGV2UiRollbackResult CurrentRollback =
                        RollbackFieldPlans(Item.ChildScreenPlan->FieldPlans);
                    bRollbackFailed |= !CurrentRollback.bRestored;
                }
                for (int32 RollbackIndex = TabIndex - 1; RollbackIndex >= 0; --RollbackIndex)
                {
                    const FPreparedTabItem& CommittedItem = PreparedTabs[RollbackIndex];
                    if (CommittedItem.bHasChildPlan && CommittedItem.ChildScreenPlan.IsValid())
                    {
                        const FGV2UiRollbackResult SiblingRollback =
                            RollbackFieldPlans(CommittedItem.ChildScreenPlan->FieldPlans);
                        bRollbackFailed |= !SiblingRollback.bRestored;
                    }
                }
                OutError = FString::Printf(
                    TEXT("nested screen central style commit failed for tab '%s': %s"),
                    *Item.Key.ToString(),
                    *StyleError);
                if (bRollbackFailed && !OutError.Contains(GGV2UiRollbackFailedDiagnosticCode))
                {
                    OutError = FString::Printf(
                        TEXT("%s: %s"),
                        GGV2UiRollbackFailedDiagnosticCode,
                        *OutError);
                }
                return false;
            }
        }
    }

    // PSC-09B/11 (ADR-0043 D2/D3): the sole physical mutation this Commit publishes --
    // the host receives it through IGV2PreparedTabContainerTarget and reconstructs the
    // USTRUCT array/TMap from this flattened operation in its own sink.
    if (TabContainer != nullptr)
    {
        GV2PresentationApply::FPreparedTabContainerOperation Operation;
        Operation.TargetWidget = TargetWidget;
        Operation.Entries.Reserve(PreparedTabs.Num());
        for (const FPreparedTabItem& Item : PreparedTabs)
        {
            GV2PresentationApply::FPreparedTabEntry Entry;
            Entry.Key = Item.Key;
            Entry.Title = FlattenTextValue(Item.Title);
            Entry.ScreenId = Item.ScreenId;
            Entry.ScreenWidget = Item.ScreenWidget.Get();
            Operation.Entries.Add(MoveTemp(Entry));
        }
        GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
        Transaction.AddTabContainerOperation(MoveTemp(Operation));
        if (!GV2ApplyTransaction(Transaction, OutError))
        {
            return false;
        }
    }

    CandidateWidgetsByKey.Reset();
    return true;
}

void FGV2TabContainerTabsPropertyConsumer::Reset(UWidget* TargetWidget)
{
    if (TargetWidget)
    {
        GV2PresentationApply::FPreparedTabContainerOperation Operation;
        Operation.TargetWidget = TargetWidget;
        Operation.bIsReset = true;
        GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
        Transaction.AddTabContainerOperation(MoveTemp(Operation));
        FString ApplyError;
        GV2ApplyTransaction(Transaction, ApplyError);
    }
    PreparedTabs.Reset();
    CandidateWidgetsByKey.Reset();
}

// --- FGV2PropertyConsumerFactory ---

TSharedPtr<IGV2PropertyConsumer> FGV2PropertyConsumerFactory::CreateConsumer(
    EGV2PreparedUiValueKind Kind,
    EGV2UiCapabilityTargetType TargetType,
    const FString& TargetKind)
{
    switch (Kind)
    {
    case EGV2PreparedUiValueKind::Text:
        return MakeShared<FGV2TextPropertyConsumer>();
    case EGV2PreparedUiValueKind::StableId:
        if (TargetKind == TEXT("resource"))
        {
            return MakeShared<FGV2ImageResourcePropertyConsumer>();
        }
        break;
    case EGV2PreparedUiValueKind::Boolean:
        return MakeShared<FGV2BooleanPropertyConsumer>();
    case EGV2PreparedUiValueKind::Integer:
        return MakeShared<FGV2IntegerPropertyConsumer>();
    case EGV2PreparedUiValueKind::Number:
        return MakeShared<FGV2NumberPropertyConsumer>();
    case EGV2PreparedUiValueKind::String:
        return MakeShared<FGV2StringPropertyConsumer>();
    case EGV2PreparedUiValueKind::Key:
        return MakeShared<FGV2KeyPropertyConsumer>();
    case EGV2PreparedUiValueKind::Binding:
        return MakeShared<FGV2BindingPropertyConsumer>();
    case EGV2PreparedUiValueKind::Array:
        if (TargetType == EGV2UiCapabilityTargetType::CollectionHost)
        {
            return MakeShared<FGV2KeyedCollectionPropertyConsumer>();
        }
        else if (TargetType == EGV2UiCapabilityTargetType::CustomControl)
        {
            return MakeShared<FGV2RichTextSpansPropertyConsumer>();
        }
        else if (TargetType == EGV2UiCapabilityTargetType::NestedScreen)
        {
            return MakeShared<FGV2TabContainerTabsPropertyConsumer>();
        }
        break;
    case EGV2PreparedUiValueKind::Null:
    case EGV2PreparedUiValueKind::Object:
        // Explicitly inapplicable kinds in property consumer factory
        break;
    }
    return nullptr;
}

EGV2PropertyConsumerKindStatus FGV2PropertyConsumerFactory::GetKindHandlingStatus(EGV2PreparedUiValueKind Kind)
{
    switch (Kind)
    {
    case EGV2PreparedUiValueKind::Boolean:
    case EGV2PreparedUiValueKind::Integer:
    case EGV2PreparedUiValueKind::Number:
    case EGV2PreparedUiValueKind::String:
    case EGV2PreparedUiValueKind::Key:
    case EGV2PreparedUiValueKind::Text:
    case EGV2PreparedUiValueKind::StableId:
    case EGV2PreparedUiValueKind::Binding:
    case EGV2PreparedUiValueKind::Array:
        return EGV2PropertyConsumerKindStatus::Supported;

    case EGV2PreparedUiValueKind::Null:
    case EGV2PreparedUiValueKind::Object:
        return EGV2PropertyConsumerKindStatus::Inapplicable;
    }
    return EGV2PropertyConsumerKindStatus::Inapplicable;
}

bool FGV2PropertyConsumerFactory::IsInapplicableKind(EGV2PreparedUiValueKind Kind, FString* OutReason)
{
    if (Kind == EGV2PreparedUiValueKind::Null)
    {
        if (OutReason != nullptr)
        {
            *OutReason = TEXT("Null represents missing or unset presentation data and cannot directly mutate UI widgets.");
        }
        return true;
    }
    if (Kind == EGV2PreparedUiValueKind::Object)
    {
        if (OutReason != nullptr)
        {
            *OutReason = TEXT("Direct Object property consumption is forbidden: composites use flat declared property mappings or KeyedCollection/NestedScreen for composite hierarchies; AddObject is prohibited to prevent key leakage and deep hierarchy drift (see DataDrivenUiComposition ADR/README).");
        }
        return true;
    }
    return false;
}

TArray<FGV2InapplicableKindInfo> FGV2PropertyConsumerFactory::GetInapplicableKinds()
{
    TArray<FGV2InapplicableKindInfo> Result;
    Result.Add({
        EGV2PreparedUiValueKind::Null,
        TEXT("Null represents missing or unset presentation data and cannot directly mutate UI widgets.")
    });
    Result.Add({
        EGV2PreparedUiValueKind::Object,
        TEXT("Direct Object property consumption is forbidden: composites use flat declared property mappings or KeyedCollection/NestedScreen for composite hierarchies; AddObject is prohibited to prevent key leakage and deep hierarchy drift (see DataDrivenUiComposition ADR/README).")
    });
    return Result;
}

bool FGV2PropertyConsumerFactory::ValidateAllKindsHandled(TArray<FString>& OutDiagnostics)
{
    bool bSuccess = true;
    for (uint8 KindIndex = 0; KindIndex < static_cast<uint8>(EGV2PreparedUiValueKind::Count); ++KindIndex)
    {
        const EGV2PreparedUiValueKind Kind = static_cast<EGV2PreparedUiValueKind>(KindIndex);
        const EGV2PropertyConsumerKindStatus Status = GetKindHandlingStatus(Kind);
        if (Status == EGV2PropertyConsumerKindStatus::Supported)
        {
            const EGV2UiCapabilityTargetType TargetType = (Kind == EGV2PreparedUiValueKind::Array)
                ? EGV2UiCapabilityTargetType::CollectionHost
                : EGV2UiCapabilityTargetType::RendererControl;
            const FString TargetKind = (Kind == EGV2PreparedUiValueKind::StableId) ? TEXT("resource") : TEXT("");
            TSharedPtr<IGV2PropertyConsumer> Consumer = CreateConsumer(Kind, TargetType, TargetKind);
            if (!Consumer.IsValid())
            {
                OutDiagnostics.Add(FString::Printf(
                    TEXT("Kind %d is marked Supported but CreateConsumer returned nullptr"),
                    static_cast<int32>(Kind)));
                bSuccess = false;
            }
        }
        else if (Status == EGV2PropertyConsumerKindStatus::Inapplicable)
        {
            FString Reason;
            if (!IsInapplicableKind(Kind, &Reason) || Reason.IsEmpty())
            {
                OutDiagnostics.Add(FString::Printf(
                    TEXT("Kind %d is marked Inapplicable but has no recorded reason"),
                    static_cast<int32>(Kind)));
                bSuccess = false;
            }
        }
    }
    return bSuccess;
}
