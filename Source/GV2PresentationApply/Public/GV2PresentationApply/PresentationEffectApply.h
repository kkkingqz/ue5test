#pragma once

#include "CoreMinimal.h"

class UWidget;

// PEP-04 (ADR-0043 D2/D4): the effect-side counterpart to FGV2PresentationApply
// (PreparedPresentationTransaction.h). This header cannot name FPresentationEffect
// (GV2RuntimeCore) or any other portable/content-authority type -- GV2PresentationApply.Build.cs
// denies GV2RuntimeCore outright, the same guarantee ADR-0043 D2 gives the property pipeline.
// Whoever reads a queued FPresentationEffect and resolves its target to a live widget lives in
// GV2 (which links both GV2RuntimeCore and GV2PresentationApply); this module only ever sees an
// already-resolved effect kind, never the DTO it came from.
//
// PEP-08 (opacity fade) is the first real kind and the first thing to extend Apply's own
// signature -- with a UWidget*, not a snapshot or gameplay-state reference. That is not a
// weakening of PEP-04's own guarantee: the guarantee came from the module's Build.cs denying
// GV2RuntimeCore/GV2ContentCore/GV2RuntimeCore-adjacent authority modules outright, and this
// module already links UMG (GV2PresentationApply.Build.cs) -- it already knows what a UWidget
// is, the same way FGV2PresentationApply::Apply (PreparedPresentationTransaction.h) always
// has. Widget and Alpha are both plain, structurally inert values with no path back to a
// snapshot or gameplay type; only GV2RuntimeCore/GV2ContentCore types were ever forbidden.

namespace GV2PresentationApply
{
// PEP-04/08: closed enumeration of effect kinds the Game-Thread executor below can dispatch.
// Symmetric to EGV2PreparedUiValueKind (GV2PropertyConsumers.h): a plain enum with a Count
// sentinel, walked by a completeness gate rather than a compile-time variant, because the set
// of effect kinds grows with future plan tasks the same way the value-kind set once did.
enum class EPresentationEffectKind : uint8
{
    // PEP-08: continuous 0<->1 UWidget::RenderOpacity fade -- appear/leave for a host-local
    // hover overlay. Not itself a queued one-shot FPresentationEffect kind (the queue --
    // PEP-03/07 -- only ever carries the hover open/close SIGNAL); this is ticked directly,
    // once per frame, by whoever owns the fade state machine (GV2RichTextWidgetBase), each
    // tick computing the next Alpha and calling Apply with it.
    Transparency,
    Count
};

// Mirrors EGV2PropertyConsumerKindStatus (GV2PropertyConsumers.h).
enum class EGV2EffectKindStatus : uint8
{
    Supported,
    Inapplicable
};

// A rejection always carries which of the guarantees below fired -- never a bare bool.
// InvalidWidget is PEP-08's own addition: a null Widget is a real, distinct failure mode a
// kind needing one can hit, never conflated with an unsupported Kind or an off-thread call.
enum class EGV2PresentationEffectApplyReject : uint8
{
    None,
    OffGameThread,
    UnsupportedKind,
    InvalidWidget,
};

struct GV2PRESENTATIONAPPLY_API FGV2PresentationEffectApplyResult
{
    bool bApplied = false;
    EGV2PresentationEffectApplyReject RejectReason = EGV2PresentationEffectApplyReject::None;
    FString Error;
};
}

// PEP-04/08: THE production entry point for executing one already-resolved effect kind. Its
// signature carries a widget and a plain value (Alpha), never a snapshot or gameplay-state
// reference -- Apply structurally cannot mutate anything this module has no type to name, the
// same guarantee ADR-0043 D2 gives FGV2PresentationApply::Apply. A rejected effect therefore
// leaves snapshot and gameplay state unchanged by construction, not by discipline this
// function has to uphold.
class GV2PRESENTATIONAPPLY_API FGV2PresentationEffectApply
{
public:
    static bool Apply(
        GV2PresentationApply::EPresentationEffectKind Kind,
        UWidget* Widget,
        float Alpha,
        GV2PresentationApply::FGV2PresentationEffectApplyResult& OutResult);

    static GV2PresentationApply::EGV2EffectKindStatus GetKindHandlingStatus(
        GV2PresentationApply::EPresentationEffectKind Kind);

    // Independent of GetKindHandlingStatus's own switch: a kind is inapplicable only if it is
    // named HERE, with a reason. GetKindHandlingStatus's switch falls back to Inapplicable for
    // any value it has no case for -- including a newly added kind nobody has updated the
    // switch for yet -- so this second, separate source of truth is what lets the gate below
    // tell "deliberately inapplicable" apart from "case forgotten". Mirrors
    // FGV2PropertyConsumerFactory::IsInapplicableKind (GV2PropertyConsumers.h).
    static bool IsInapplicableKind(GV2PresentationApply::EPresentationEffectKind Kind, FString* OutReason = nullptr);

    // Completeness gate: every value of EPresentationEffectKind is either Supported with a
    // working dispatch branch in Apply, or independently confirmed Inapplicable by
    // IsInapplicableKind with a reason -- never Inapplicable by GetKindHandlingStatus's
    // fallback alone. Mirrors FGV2PropertyConsumerFactory::ValidateAllKindsHandled
    // (GV2PropertyConsumers.h).
    static bool ValidateAllEffectKindsHandled(TArray<FString>& OutDiagnostics);
};
