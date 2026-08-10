#include "Bridge/GV2RuntimeIngressQueue.h"

FGV2RuntimeIngressQueue::FGV2RuntimeIngressQueue(const int32 InCapacity)
    : QueueCapacity(FMath::Max(0, InCapacity))
{
}

bool FGV2RuntimeIngressQueue::TryEnqueue(FGV2UiIngressItem&& Item)
{
    check(IsInGameThread());

    if (QueueSize >= QueueCapacity)
    {
        return false;
    }

    Queue.Enqueue(MoveTemp(Item));
    ++QueueSize;
    return true;
}

bool FGV2RuntimeIngressQueue::Dequeue(FGV2UiIngressItem& OutItem)
{
    check(IsInGameThread());

    if (!Queue.Dequeue(OutItem))
    {
        return false;
    }

    --QueueSize;
    return true;
}

void FGV2RuntimeIngressQueue::Reset()
{
    check(IsInGameThread());

    FGV2UiIngressItem DiscardedItem;
    while (Queue.Dequeue(DiscardedItem))
    {
    }
    QueueSize = 0;
}

int32 FGV2RuntimeIngressQueue::Num() const
{
    return QueueSize;
}

int32 FGV2RuntimeIngressQueue::Capacity() const
{
    return QueueCapacity;
}
