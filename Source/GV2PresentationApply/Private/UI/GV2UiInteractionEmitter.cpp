#include "UI/GV2UiInteractionEmitter.h"

#include "GV2PresentationApply/GV2PresentationInteractionSink.h"

EGV2SubmitUiInteractionResult FGV2UiInteractionEmitter::Submit(
    const UObject* WorldContextObject,
    const FGV2UiBindingHandle BindingHandle,
    const TArray<FGV2UiControlValue>& InputValues)
{
    UGV2PresentationInteractionSink* Sink = UGV2PresentationInteractionSink::Find(WorldContextObject);
    return Sink != nullptr
        ? Sink->SubmitPresentationInteraction(BindingHandle, InputValues)
        : EGV2SubmitUiInteractionResult::RuntimeNotReady;
}
