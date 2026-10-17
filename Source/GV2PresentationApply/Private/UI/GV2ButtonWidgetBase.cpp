#include "UI/GV2ButtonWidgetBase.h"

#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "GV2WidgetTextApply.h"
#include "UI/GV2UiInteractionEmitter.h"
#include "UI/GV2UiCapability.h"

void UGV2ButtonWidgetBase::RefreshPreparedViewportPresentation(float ViewportHeight)
{
    // ADR-0049: a resize changes the surface scale, so it changes the control's physical size
    // by the same curve it changes the type. Both are recomputed from values Prepare already
    // resolved; neither re-reads the Theme.
    EnforceDimensionsForHeight(ViewportHeight);

    if (CurrentTextStyleToken.IsNone() && bHasPreparedDefaultLabelScale)
    {
        FGV2WidgetTextApply::RefreshFont(LabelText, PreparedDefaultLabelScale, ViewportHeight);
        return;
    }
    FGV2WidgetTextApply::RefreshFont(LabelText, CurrentTextViewModel, ViewportHeight);
}

void UGV2ButtonWidgetBase::EnforceDimensionsForHeight(const float ViewportHeight)
{
    if (PreparedVirtualSize.IsNearlyZero())
    {
        return;
    }
    const float Scale = GV2PresentationApply::EvaluatePreparedViewportScale(PreparedSizeScale, ViewportHeight);
    const FIntPoint Dimensions(
        FMath::RoundToInt(PreparedVirtualSize.X * Scale),
        FMath::RoundToInt(PreparedVirtualSize.Y * Scale));

    // Min AND max, deliberately equal: CommonUI's min alone is what let a long label grow one
    // button past its neighbours. Clamping both makes the size the surface's property.
    SetMinDimensions(Dimensions.X, Dimensions.Y);
    SetMaxDimensions(Dimensions.X, Dimensions.Y);
    LastEnforcedDimensions = Dimensions;

    // The label is content inside a fixed box, so it must not paint outside it either.
    if (LabelText != nullptr)
    {
        LabelText->SetClipping(EWidgetClipping::ClipToBounds);
    }
}

void UGV2ButtonWidgetBase::ApplyPreparedDimensions()
{
    EnforceDimensionsForHeight(GV2PresentationApply::ResolveLiveViewportHeight(
        this, PreparedSizeScale.ReferenceViewportHeight));
}

FVector2D UGV2ButtonWidgetBase::GetEnforcedPhysicalSize() const
{
    if (PreparedVirtualSize.IsNearlyZero())
    {
        return FVector2D::ZeroVector;
    }
    const float ViewportHeight = GV2PresentationApply::ResolveLiveViewportHeight(
        this, PreparedSizeScale.ReferenceViewportHeight);
    const float Scale = GV2PresentationApply::EvaluatePreparedViewportScale(PreparedSizeScale, ViewportHeight);
    return FVector2D(
        FMath::RoundToFloat(PreparedVirtualSize.X * Scale),
        FMath::RoundToFloat(PreparedVirtualSize.Y * Scale));
}

bool UGV2ButtonWidgetBase::MeasureLabelOverflow(FString& OutError) const
{
    OutError.Reset();
    if (LabelText == nullptr || PreparedVirtualSize.IsNearlyZero())
    {
        return true;
    }
    if (!FSlateApplication::IsInitialized())
    {
        // No font measure service without Slate; the gate-level test covers this case and a
        // silent pass here is not a claim that the label fits.
        return true;
    }

    const FText LabelValue = LabelText->GetText();
    if (LabelValue.IsEmpty())
    {
        return true;
    }

    // Padding belongs to the style, not to the control: read it off the style CDO rather
    // than duplicating the authored value here.
    FMargin ButtonPadding = FMargin(0.0f);
    FMargin CustomPadding = FMargin(0.0f);
    if (const UCommonButtonStyle* StyleCDO = GetStyleCDO())
    {
        StyleCDO->GetButtonPadding(ButtonPadding);
        StyleCDO->GetCustomPadding(CustomPadding);
    }

    const FVector2D PhysicalSize = GetEnforcedPhysicalSize();
    const float AvailableWidth = PhysicalSize.X
        - ButtonPadding.Left - ButtonPadding.Right
        - CustomPadding.Left - CustomPadding.Right;
    const float AvailableHeight = PhysicalSize.Y
        - ButtonPadding.Top - ButtonPadding.Bottom
        - CustomPadding.Top - CustomPadding.Bottom;

    const TSharedRef<FSlateFontMeasure> FontMeasure =
        FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    const FVector2D Measured(FontMeasure->Measure(LabelValue, LabelText->GetFont()));

    if (Measured.X > AvailableWidth + 0.5f || Measured.Y > AvailableHeight + 0.5f)
    {
        OutError = FString::Printf(
            TEXT("core:diagnostic.ui_button.label_overflow: label '%s' measures %.1fx%.1f but the ")
            TEXT("control's content box is %.1fx%.1f (button %.0fx%.0f less padding)"),
            *LabelValue.ToString(),
            Measured.X, Measured.Y,
            AvailableWidth, AvailableHeight,
            PhysicalSize.X, PhysicalSize.Y);
        return false;
    }
    return true;
}

void UGV2ButtonWidgetBase::RestorePreparedLabelPresentation()
{
    if (LabelText == nullptr)
    {
        return;
    }

    if (!CurrentTextStyleToken.IsNone() && CurrentTextViewModel.bHasResolvedPresentation)
    {
        // CommonUI changes its current text style for every control-state transition and
        // while SetStyle rebuilds the Slate styles. Desired presentation remains the owner
        // of semantic typography, so restore the complete prepared text payload afterwards.
        FGV2WidgetTextApply::Apply(LabelText, CurrentTextViewModel);
        return;
    }

    if (PreparedDefaultLabelStyle != nullptr && bHasPreparedDefaultLabelScale)
    {
        LabelText->SetStyle(PreparedDefaultLabelStyle);
        FGV2WidgetTextApply::RefreshFont(
            LabelText,
            PreparedDefaultLabelScale,
            GV2PresentationApply::ResolveLiveViewportHeight(
                this,
                PreparedDefaultLabelScale.ReferenceViewportHeight));
    }
}

void UGV2ButtonWidgetBase::NativePreConstruct()
{
    Super::NativePreConstruct();
    // PSC-10B: runtime style arrives as FPreparedButtonStyle. Nothing here re-applies it at
    // design time: a button's serialized CommonUI style and its label's serialized style
    // already render on their own. See UGV2SeparatorWidgetBase::NativePreConstruct.
}

void UGV2ButtonWidgetBase::NativeOnCurrentTextStyleChanged()
{
    Super::NativeOnCurrentTextStyleChanged();
    RestorePreparedLabelPresentation();
}

bool UGV2ButtonWidgetBase::ApplyText(const FGV2TextViewModel& InText)
{
    CurrentTextStyleToken = InText.StyleToken;
    CurrentTextViewModel = InText;
    if (InText.NormalizedMarkup.Contains(TEXT("<gv2")))
    {
        return false;
    }
    if (LabelText != nullptr && !FGV2WidgetTextApply::Apply(LabelText, InText))
    {
        return false;
    }
    return true;
}

void UGV2ButtonWidgetBase::SetKey(FName InKey)
{
    GetPropertyHostState().SetKey(InKey);
}

FName UGV2ButtonWidgetBase::GetKey() const
{
    return GetPropertyHostState().GetKey();
}

void UGV2ButtonWidgetBase::SetBindingHandle(const FGV2UiBindingHandle& InBindingHandle)
{
    BindingHandle = InBindingHandle;
    SetIsEnabled(BindingHandle.IsValid());
}

FGV2UiBindingHandle UGV2ButtonWidgetBase::GetBindingHandle() const
{
    return BindingHandle;
}

void UGV2ButtonWidgetBase::SetAutomaticInteractionSubmission(const bool bEnabled)
{
    bAutomaticInteractionSubmission = bEnabled;
}

void UGV2ButtonWidgetBase::ApplyButtonStyleValues(
    TSubclassOf<UCommonButtonStyle> InButtonStyle,
    TSubclassOf<UCommonTextStyle> InDefaultLabelStyle,
    const GV2PresentationApply::FPreparedTextScalePolicy& InDefaultLabelScale,
    const FVector2D& InVirtualSize,
    const GV2PresentationApply::FPreparedViewportScalePolicy& InSizeScale)
{
    PreparedDefaultLabelScale = InDefaultLabelScale;
    PreparedDefaultLabelStyle = InDefaultLabelStyle;
    bHasPreparedDefaultLabelScale = true;
    PreparedVirtualSize = InVirtualSize;
    PreparedSizeScale = InSizeScale;
    if (InButtonStyle != nullptr)
    {
        SetStyle(InButtonStyle);
    }
    // After SetStyle, which is what publishes the style's own min/max dimensions.
    ApplyPreparedDimensions();
    // SetStyle can synchronously publish a CommonUI text-style change. Repeat the restore
    // even when the style class was already current and CommonUI therefore emitted nothing.
    RestorePreparedLabelPresentation();
}

void UGV2ButtonWidgetBase::NativeOnClicked()
{
    Super::NativeOnClicked();

    OnActivated.Broadcast(GetKey());
    if (!bAutomaticInteractionSubmission)
    {
        return;
    }

    const EGV2SubmitUiInteractionResult Result =
        FGV2UiInteractionEmitter::Submit(this, BindingHandle, {});

    OnBindingInvoked.Broadcast(BindingHandle, Result);
}

void UGV2ButtonWidgetBase::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    OutBuilder.AddText(TEXT("text"), FName(TEXT("LabelText")));
    OutBuilder.AddBinding(TEXT("binding"), NAME_None);
    OutBuilder.AddKey(TEXT("key"), NAME_None);
}

bool UGV2ButtonWidgetBase::ApplyPreparedText(
    const GV2PresentationApply::FPreparedTextValue& Value,
    bool /*bIsReset*/,
    FString& OutError)
{
    if (!ApplyText(FGV2TextViewModel::FromPrepared(Value)))
    {
        OutError = TEXT("core:diagnostic.ui_consumer.text_apply_failed: UGV2ButtonWidgetBase::ApplyText rejected the resolved text");
        return false;
    }
    // ADR-0049: the size is already fixed, so this is the moment the label either fits it or
    // does not. Reporting here puts the failure on the same path that rejects every other
    // malformed presentation value, instead of letting the control absorb it geometrically.
    return MeasureLabelOverflow(OutError);
}
