#include "GV2PresentationApply/GV2PresentationInteractionSink.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"

UGV2PresentationInteractionSink* UGV2PresentationInteractionSink::Find(
    const UObject* WorldContext)
{
    const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
    UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
    if (GameInstance == nullptr)
    {
        return nullptr;
    }

    TArray<UGV2PresentationInteractionSink*> Sinks =
        GameInstance->GetSubsystemArrayCopy<UGV2PresentationInteractionSink>();
    return Sinks.Num() == 1 ? Sinks[0] : nullptr;
}
