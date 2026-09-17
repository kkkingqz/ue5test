#include "GV2PresentationApply/PresentationEffectApply.h"

#include "Components/Widget.h"

using namespace GV2PresentationApply;

bool FGV2PresentationEffectApply::Apply(
    EPresentationEffectKind Kind,
    UWidget* Widget,
    float Alpha,
    FGV2PresentationEffectApplyResult& OutResult)
{
    OutResult = FGV2PresentationEffectApplyResult();

    if (!IsInGameThread())
    {
        OutResult.RejectReason = EGV2PresentationEffectApplyReject::OffGameThread;
        OutResult.Error = TEXT("core:diagnostic.presentation_effect_apply.off_game_thread: FGV2PresentationEffectApply::Apply must be called on the Game Thread.");
        return false;
    }

    switch (Kind)
    {
    case EPresentationEffectKind::Transparency:
        if (Widget == nullptr)
        {
            OutResult.RejectReason = EGV2PresentationEffectApplyReject::InvalidWidget;
            OutResult.Error = TEXT("core:diagnostic.presentation_effect_apply.invalid_widget: Transparency requires a widget.");
            return false;
        }
        Widget->SetRenderOpacity(FMath::Clamp(Alpha, 0.0f, 1.0f));
        OutResult.bApplied = true;
        return true;

    case EPresentationEffectKind::Count:
        break;
    }

    OutResult.RejectReason = EGV2PresentationEffectApplyReject::UnsupportedKind;
    OutResult.Error = FString::Printf(
        TEXT("core:diagnostic.presentation_effect_apply.unsupported_kind: no dispatch registered for effect kind %d"),
        static_cast<int32>(Kind));
    return false;
}

EGV2EffectKindStatus FGV2PresentationEffectApply::GetKindHandlingStatus(EPresentationEffectKind Kind)
{
    switch (Kind)
    {
    case EPresentationEffectKind::Transparency:
        return EGV2EffectKindStatus::Supported;
    case EPresentationEffectKind::Count:
        break;
    }
    return EGV2EffectKindStatus::Inapplicable;
}

bool FGV2PresentationEffectApply::IsInapplicableKind(EPresentationEffectKind Kind, FString* OutReason)
{
    // PEP-04: no effect kind is registered as inapplicable-by-design yet. Any kind that
    // reaches here is either Count itself (never a real kind) or one GetKindHandlingStatus
    // fell through to Inapplicable for lack of a case -- both are correctly "not applicable"
    // by this function's own account, below.
    (void)Kind;
    if (OutReason != nullptr)
    {
        *OutReason = TEXT("");
    }
    return false;
}

bool FGV2PresentationEffectApply::ValidateAllEffectKindsHandled(TArray<FString>& OutDiagnostics)
{
    // Symmetric to FGV2PropertyConsumerFactory::ValidateAllKindsHandled: a kind marked
    // Inapplicable by GetKindHandlingStatus's switch is only accepted if IsInapplicableKind
    // -- an INDEPENDENT source of truth -- confirms it with a reason. This is what actually
    // catches a kind added to the enum without a case in GetKindHandlingStatus: such a kind
    // falls through to Inapplicable, but IsInapplicableKind (which knows nothing about that
    // switch) has no reason for it, so the mismatch is reported instead of silently passing.
    // Measured: a temporary enumerator added ahead of Count during PEP-04 development, with
    // no case added anywhere, was reported by this gate (see the PEP-04 commit message).
    // With Count == 0 the loop body never runs; it starts running unchanged the day PEP-08
    // adds the first kind.
    bool bSuccess = true;
    for (uint8 KindIndex = 0; KindIndex < static_cast<uint8>(EPresentationEffectKind::Count); ++KindIndex)
    {
        const EPresentationEffectKind Kind = static_cast<EPresentationEffectKind>(KindIndex);
        const EGV2EffectKindStatus Status = GetKindHandlingStatus(Kind);
        if (Status == EGV2EffectKindStatus::Inapplicable)
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
