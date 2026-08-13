#include "UI/GV2UiInteractionEmitter.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Runtime/GV2RuntimeSubsystem.h"

EGV2SubmitUiInteractionResult FGV2UiInteractionEmitter::Submit(
    const UObject* WorldContextObject,
    const FGV2UiBindingHandle BindingHandle,
    const TArray<FGV2UiControlValue>& InputValues)
{
    const UWorld* World = WorldContextObject != nullptr ? WorldContextObject->GetWorld() : nullptr;
    const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
    UGV2RuntimeSubsystem* Runtime = GameInstance != nullptr
        ? GameInstance->GetSubsystem<UGV2RuntimeSubsystem>()
        : nullptr;
    return Runtime != nullptr
        ? Runtime->SubmitUiInteraction(BindingHandle, InputValues)
        : EGV2SubmitUiInteractionResult::RuntimeNotReady;
}
