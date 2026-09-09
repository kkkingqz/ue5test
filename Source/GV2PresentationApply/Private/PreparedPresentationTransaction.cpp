#include "GV2PresentationApply/PreparedPresentationTransaction.h"

#include "CommonRichTextBlock.h"
#include "CommonTextBlock.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/Widget.h"
#include "Curves/RichCurve.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Math/UnrealMathUtility.h"

namespace GV2PresentationApply
{
namespace
{
// PSC-10A: standard "overloaded lambda set" idiom -- TVariant's Visit() dispatches to
// whichever operator() matches the alternative's concrete type. If a new alternative is
// added to FGV2PreparedOperationVariant without a corresponding lambda here, overload
// resolution finds no match and the build fails -- the compiler IS the exhaustiveness
// gate, not a `default:` branch or a hand-maintained kind list.
template <typename... Ts>
struct TOverloaded : Ts...
{
    using Ts::operator()...;
};
template <typename... Ts>
TOverloaded(Ts...) -> TOverloaded<Ts...>;
}

EGV2PreparedOperationKind GetPreparedOperationKind(const FGV2PreparedOperationVariant& Operation)
{
    return static_cast<EGV2PreparedOperationKind>(Operation.GetIndex());
}

// PSC-10A: mirrors UGV2UiTheme::EvaluateTextScale's own curve-eval-with-fallback-lerp
// math exactly (same breakpoints: 720/1080/1440/2160), against the resolved policy's
// OWN curve instead of a live Theme object.
// PSC-10B: the curve evaluation itself, so EvaluatePreparedFontSize and every non-font
// consumer of the same scale share ONE implementation rather than two copies that can drift.
float EvaluatePreparedViewportScale(const FPreparedViewportScalePolicy& Policy, float ViewportHeight)
{
    if (ViewportHeight <= 0.0f)
    {
        return 1.0f;
    }
    const FRichCurve* Curve = Policy.ScaleCurve.GetRichCurveConst();
    if (Curve != nullptr && Curve->GetNumKeys() > 0)
    {
        return FMath::Max(0.1f, Curve->Eval(ViewportHeight));
    }
    if (ViewportHeight < 1080.0f)
    {
        const float Alpha = FMath::Clamp((ViewportHeight - 720.0f) / (1080.0f - 720.0f), 0.0f, 1.0f);
        return FMath::Lerp(0.85f, 1.0f, Alpha);
    }
    const float Alpha = FMath::Clamp((ViewportHeight - 1080.0f) / (2160.0f - 1080.0f), 0.0f, 1.0f);
    return FMath::Lerp(1.0f, 1.60f, Alpha);
}

float EvaluatePreparedFontSize(const FPreparedTextScalePolicy& Policy, float ViewportHeight)
{
    FPreparedViewportScalePolicy ScalePolicy;
    ScalePolicy.ScaleCurve = Policy.ScaleCurve;
    ScalePolicy.ReferenceViewportHeight = Policy.ReferenceViewportHeight;
    const float ScaledSize = Policy.BaseFontSize * EvaluatePreparedViewportScale(ScalePolicy, ViewportHeight);
    return FMath::Max(Policy.MinReadableFontSize, ScaledSize);
}

// PSC-10A: mirrors UGV2TextPipeline::GetViewportHeight's own live-query logic exactly,
// minus the Theme-sourced fallback (now a parameter, itself already resolved in Prepare).
float ResolveLiveViewportHeight(const UWidget* ContextWidget, float FallbackHeight)
{
    if (GEngine != nullptr && GEngine->GameViewport != nullptr)
    {
        FVector2D ViewportSize;
        GEngine->GameViewport->GetViewportSize(ViewportSize);
        if (ViewportSize.Y > 0.0f)
        {
            return ViewportSize.Y;
        }
    }

    if (ContextWidget != nullptr)
    {
        if (const UWorld* World = ContextWidget->GetWorld())
        {
            if (const UGameViewportClient* ViewportClient = World->GetGameViewport())
            {
                FVector2D ViewportSize;
                ViewportClient->GetViewportSize(ViewportSize);
                if (ViewportSize.Y > 0.0f)
                {
                    return ViewportSize.Y;
                }
            }
        }
    }

    return FallbackHeight;
}

bool Apply(const FGV2PreparedPresentationTransaction& Transaction, FString& OutError)
{
    OutError.Reset();
    bool bFailed = false;

    for (const FGV2PreparedOperationVariant& Operation : Transaction.GetOperations())
    {
        if (bFailed)
        {
            break;
        }

        Visit(TOverloaded{
            [&bFailed, &OutError](const FPreparedImageResourceOperation& Op)
            {
                UImage* Widget = Op.TargetWidget.Get();
                if (Widget == nullptr)
                {
                    OutError = TEXT("Image resource operation's target widget is no longer valid.");
                    bFailed = true;
                    return;
                }
                Widget->SetBrush(Op.Brush);
                Widget->SetDesiredSizeOverride(Op.Brush.ImageSize);
            },
            // ImageHost/Number/Integer/String/Key/Binding/RichTextSpans/Text/
            // KeyedCollection/TabContainer operations carry no target this module can
            // Cast to (see each struct's own doc comment) -- entirely
            // GV2LegacyPresentationApplyAdapter territory, nothing to do here.
            [](const FPreparedImageHostOperation&) {},
            [](const FPreparedBooleanOperation& Op)
            {
                UWidget* Widget = Op.TargetWidget.Get();
                if (Widget == nullptr)
                {
                    return;
                }
                switch (Op.Target)
                {
                case EPreparedBooleanTarget::WidgetEnabled:
                    Widget->SetIsEnabled(Op.Value);
                    break;
                case EPreparedBooleanTarget::CheckBoxChecked:
                    if (UCheckBox* CheckBox = Cast<UCheckBox>(Widget))
                    {
                        CheckBox->SetIsChecked(Op.Value);
                    }
                    break;
                case EPreparedBooleanTarget::EditableTextReadOnly:
                    if (UEditableTextBox* EditableBox = Cast<UEditableTextBox>(Widget))
                    {
                        EditableBox->SetIsReadOnly(Op.Value);
                    }
                    break;
                case EPreparedBooleanTarget::RequiresLegacyAdapter:
                    // Left for GV2LegacyPresentationApplyAdapter -- see this operation's
                    // own enum doc comment.
                    break;
                }
            },
            [](const FPreparedEditableTextValueOperation& Op)
            {
                if (UEditableTextBox* Widget = Op.TargetWidget.Get())
                {
                    Widget->SetText(Op.Value);
                }
            },
            [](const FPreparedProgressBarOperation& Op)
            {
                if (UProgressBar* Widget = Op.TargetWidget.Get())
                {
                    Widget->SetPercent(Op.Percent);
                }
            },
            [](const FPreparedNumberOperation&) {},
            [](const FPreparedIntegerOperation&) {},
            [](const FPreparedStringOperation&) {},
            [](const FPreparedKeyOperation&) {},
            [](const FPreparedBindingOperation&) {},
            [](const FPreparedRichTextSpansOperation&) {},
            [](const FPreparedTextOperation&) {},
            [](const FPreparedKeyedCollectionOperation&) {},
            [](const FPreparedTabContainerOperation&) {},
            [](const FPreparedPlainTextOperation& Op)
            {
                UCommonTextBlock* Widget = Op.TargetWidget.Get();
                if (Widget == nullptr)
                {
                    return;
                }
                Widget->SetStyle(Op.Style);
                Widget->SetText(Op.Text);

                // PSC-10A: pure function of the resolved policy and CURRENT geometry,
                // queried live here -- no Theme/config lookup.
                const float ViewportHeight = ResolveLiveViewportHeight(Widget, Op.ScalePolicy.ReferenceViewportHeight);
                const float ScaledFontSize = EvaluatePreparedFontSize(Op.ScalePolicy, ViewportHeight);

                // Apply DPI-aware scaled font size without wiping out the widget's
                // FontObject/typeface.
                FSlateFontInfo FontInfo = Widget->GetFont();
                if (!FMath::IsNearlyEqual(FontInfo.Size, ScaledFontSize, 0.01f))
                {
                    FontInfo.Size = ScaledFontSize;
                    Widget->SetFont(FontInfo);
                }
            },
            [](const FPreparedRichTextRenderOperation& Op)
            {
                UCommonRichTextBlock* Widget = Op.TargetWidget.Get();
                if (Widget == nullptr)
                {
                    return;
                }
                if (Op.Style != nullptr)
                {
                    Widget->SetStyle(Op.Style);
                }
                if (Op.bHasDefaultStyle)
                {
                    // Pure function of the resolved policy and current geometry.
                    FTextBlockStyle FinalStyle = Op.DefaultStyle;
                    const float ViewportHeight = ResolveLiveViewportHeight(Widget, Op.ScalePolicy.ReferenceViewportHeight);
                    FinalStyle.SetFontSize(EvaluatePreparedFontSize(Op.ScalePolicy, ViewportHeight));
                    Widget->SetDefaultTextStyle(FinalStyle);
                }
                Widget->SetText(FText::FromString(Op.Markup));
            },
            [](const FPreparedTextHintOperation& Op)
            {
                if (UEditableTextBox* Widget = Op.TargetWidget.Get())
                {
                    Widget->SetHintText(Op.Text);
                }
            },
            [](const FPreparedCentralStyleOperation&)
            {
                // PSC-10B: every central-style role targets a GV2-owned widget base today,
                // which this module cannot Cast to by construction (Build.cs denylist), so
                // GV2LegacyPresentationApplyAdapter performs the physical write from the
                // same operation -- not a second resolution. When PSC-12 moves those
                // UCLASSes down, the write moves here with them and this branch stops
                // being empty.
            }
        }, Operation);
    }

    if (bFailed)
    {
        return false;
    }
    return true;
}
}
