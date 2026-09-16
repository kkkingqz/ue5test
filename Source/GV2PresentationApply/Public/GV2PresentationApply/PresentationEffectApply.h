#pragma once

#include "CoreMinimal.h"

// PEP-04 (ADR-0043 D2/D4): the effect-side counterpart to FGV2PresentationApply
// (PreparedPresentationTransaction.h). This header cannot name FPresentationEffect
// (GV2RuntimeCore) or any other portable/content-authority type -- GV2PresentationApply.Build.cs
// denies GV2RuntimeCore outright, the same guarantee ADR-0043 D2 gives the property pipeline.
// Whoever reads a queued FPresentationEffect and resolves its target to a live widget lives in
// GV2 (which links both GV2RuntimeCore and GV2PresentationApply); this module only ever sees an
// already-resolved effect kind, never the DTO it came from.
//
// PEP-04 introduces the mechanism with zero supported kinds: EPresentationEffectKind::Count is
// 0. The first real kind (opacity fade) is PEP-08's job, matching the plan's own rule that a
// change set does not combine introducing a mechanism with introducing its first consumer.
// Exhaustiveness for the empty set is proven now, by construction and by a temporary mutation
// during development (see the PEP-04 commit message for how); it holds unchanged once PEP-08
// adds the first case.

namespace GV2PresentationApply
{
// PEP-04: closed enumeration of effect kinds the Game-Thread executor below can dispatch.
// Symmetric to EGV2PreparedUiValueKind (GV2PropertyConsumers.h): a plain enum with a Count
// sentinel, walked by a completeness gate rather than a compile-time variant, because the set
// of effect kinds grows with future plan tasks the same way the value-kind set once did.
enum class EPresentationEffectKind : uint8
{
    Count = 0
};

// Mirrors EGV2PropertyConsumerKindStatus (GV2PropertyConsumers.h).
enum class EGV2EffectKindStatus : uint8
{
    Supported,
    Inapplicable
};

// A rejection always carries which of the two guarantees below fired -- never a bare bool.
// OffGameThread and UnsupportedKind are the only ways Apply can fail: there is no third
// failure mode because the function touches nothing before these two checks run.
enum class EGV2PresentationEffectApplyReject : uint8
{
    None,
    OffGameThread,
    UnsupportedKind,
};

struct GV2PRESENTATIONAPPLY_API FGV2PresentationEffectApplyResult
{
    bool bApplied = false;
    EGV2PresentationEffectApplyReject RejectReason = EGV2PresentationEffectApplyReject::None;
    FString Error;
};
}

// PEP-04: THE production entry point for executing one already-resolved effect kind. Its
// signature carries no widget, snapshot, or gameplay-state reference -- Apply structurally
// cannot mutate anything this module has no type to name, the same guarantee ADR-0043 D2
// gives FGV2PresentationApply::Apply. A rejected effect therefore leaves snapshot and
// gameplay state unchanged by construction, not by discipline this function has to uphold.
class GV2PRESENTATIONAPPLY_API FGV2PresentationEffectApply
{
public:
    static bool Apply(
        GV2PresentationApply::EPresentationEffectKind Kind,
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
