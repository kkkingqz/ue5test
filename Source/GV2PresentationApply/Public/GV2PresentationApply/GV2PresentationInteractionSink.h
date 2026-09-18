#pragma once

#include "GV2PresentationApply/GV2WidgetTypes.h"
#include "Misc/TVariant.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "GV2PresentationInteractionSink.generated.h"

class UUserWidget;

// PEP-09 (ADR-0048): the two kinds of exit, distinguished by TYPE, not by an orthogonal
// "accepts input" flag any caller could set inconsistently. FGV2StaleHostLocalDeparture
// carries no field at all for input acceptance -- there is nothing to set wrong, which is
// what "interactive stale is unrepresentable" means concretely. A departure being
// self-dismissing is what makes it interactive; that is intrinsic to the alternative
// itself, not a property attached to it.
struct GV2PRESENTATIONAPPLY_API FGV2StaleHostLocalDeparture
{
};

struct GV2PRESENTATIONAPPLY_API FGV2SelfDismissingHostLocalDeparture
{
};

using FGV2HostLocalDepartureState = TVariant<FGV2StaleHostLocalDeparture, FGV2SelfDismissingHostLocalDeparture>;

// PEP-09: the ONE place "does this departure kind accept input" is decided -- a two-way
// IsType check, not a stored bool, so the answer can never disagree with which alternative
// the state actually is.
inline bool GV2HostLocalDepartureAcceptsInput(const FGV2HostLocalDepartureState& Departure)
{
    return Departure.IsType<FGV2SelfDismissingHostLocalDeparture>();
}

// Upward interaction ingress is deliberately separate from presentation Apply.  Widget
// lifecycle code can submit an opaque handle or a user-selected tab, while prepared
// operations still have no callback or authority reference in their payload.
UCLASS(Abstract)
class GV2PRESENTATIONAPPLY_API UGV2PresentationInteractionSink : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual EGV2SubmitUiInteractionResult SubmitPresentationInteraction(
        FGV2UiBindingHandle BindingHandle,
        const TArray<FGV2UiControlValue>& InputValues)
    {
        return EGV2SubmitUiInteractionResult::RuntimeNotReady;
    }

    virtual void NotifyActiveTab(const FString& ContainerPath, const FString& TabKey) {}

    // PEP-06B: opens Widget (an already-prepared, already-styled screen instance -- e.g.
    // FGV2RichTextSpanViewModel::Hover.ScreenWidget) as a host-local participant of
    // whichever layer the runtime side reserves for floating/anchored content.
    // GV2PresentationApply has no visibility into layer names or the Game Shell (PSC-09A,
    // ADR-0043 D2) -- only the runtime override knows which layer and how to reach the
    // reconciler. Returns the instance key CloseHoverOverlay needs; default is a no-op
    // failure, matching every other virtual on this sink.
    // PEP-08: DurationSeconds is content-declared (FGV2RichTextHoverViewModel::Duration,
    // PEP-06C) -- named here, not hidden inside Args, so the override can put it in the
    // published effect's own args.duration_ms alongside the participant key.
    virtual bool OpenHoverOverlay(UUserWidget* Widget, float DurationSeconds, FName& OutInstanceKey, FString& OutError)
    {
        OutError = TEXT("core:diagnostic.ui_consumer.hover_overlay_unavailable: no active interaction sink");
        return false;
    }

    virtual void CloseHoverOverlay(FName InstanceKey) {}

    // PEP-09 (ADR-0048): names which of the two exits InstanceKey is undergoing, the moment
    // that is DECIDED -- Stale as soon as reconciliation removes the span/screen this hover
    // depended on (before any fade even reflects it), SelfDismissal as soon as the cursor
    // leaves. Gates input immediately, independent of the physical detach the fade may still
    // be playing out; a discarded/late call changes nothing already gated. Default is a
    // no-op, matching every other virtual on this sink.
    virtual void SetHoverOverlayDeparture(FName InstanceKey, const FGV2HostLocalDepartureState& Departure) {}

    static UGV2PresentationInteractionSink* Find(const UObject* WorldContext);
};
