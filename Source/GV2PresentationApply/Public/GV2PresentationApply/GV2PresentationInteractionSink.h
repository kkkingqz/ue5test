#pragma once

#include "GV2PresentationApply/GV2WidgetTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "GV2PresentationInteractionSink.generated.h"

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

    static UGV2PresentationInteractionSink* Find(const UObject* WorldContext);
};
