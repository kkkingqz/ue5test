#pragma once

#include "GV2PresentationApply/GV2WidgetTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "GV2PresentationInteractionSink.generated.h"

class UUserWidget;

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

    static UGV2PresentationInteractionSink* Find(const UObject* WorldContext);
};
