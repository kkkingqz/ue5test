#pragma once

#include "Bridge/GV2BridgeRuntimeTypes.h"
#include "Containers/Queue.h"

class FGV2RuntimeIngressQueue
{
public:
    explicit FGV2RuntimeIngressQueue(int32 InCapacity = 256);

    bool TryEnqueue(FGV2UiIngressItem&& Item);
    bool Dequeue(FGV2UiIngressItem& OutItem);
    void Reset();

    int32 Num() const;
    int32 Capacity() const;

private:
    TQueue<FGV2UiIngressItem, EQueueMode::Spsc> Queue;
    int32 QueueCapacity = 0;
    int32 QueueSize = 0;
};
