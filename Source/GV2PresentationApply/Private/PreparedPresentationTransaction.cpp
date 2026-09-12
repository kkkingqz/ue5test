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
    // A process can host multiple game viewports in PIE. The widget's World identifies
    // which one owns this presentation; consulting the process-global viewport first can
    // therefore apply another PIE/editor window's scale during a document Commit. Keep the
    // global client only as a fallback for context-free or not-yet-world-bound widgets.
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

    if (GEngine != nullptr && GEngine->GameViewport != nullptr)
    {
        FVector2D ViewportSize;
        GEngine->GameViewport->GetViewportSize(ViewportSize);
        if (ViewportSize.Y > 0.0f)
        {
            return ViewportSize.Y;
        }
    }

    return FallbackHeight;
}

}
