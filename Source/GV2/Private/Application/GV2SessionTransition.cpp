#include "Application/GV2SessionTransition.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"

DEFINE_LOG_CATEGORY_STATIC(LogGV2SessionTransition, Log, All);

bool FGV2SessionTransitionOracle::CanTransitionSessionState(
    const EGV2SessionState CurrentState,
    const EGV2SessionState TargetState,
    const ESessionTransitionKind TransitionKind)
{
    if (CurrentState == TargetState)
    {
        return false;
    }

    switch (CurrentState)
    {
    case EGV2SessionState::None:
        if (TargetState == EGV2SessionState::Creating)
        {
            switch (TransitionKind)
            {
            case ESessionTransitionKind::Menu:
            case ESessionTransitionKind::NewGame:
            case ESessionTransitionKind::LoadSave:
                return true;
            case ESessionTransitionKind::Shutdown:
                return false;
            }
        }
        if (TargetState == EGV2SessionState::Destroyed)
        {
            switch (TransitionKind)
            {
            case ESessionTransitionKind::Shutdown:
                return true;
            case ESessionTransitionKind::Menu:
            case ESessionTransitionKind::NewGame:
            case ESessionTransitionKind::LoadSave:
                return false;
            }
        }
        if (TargetState == EGV2SessionState::Failed)
        {
            switch (TransitionKind)
            {
            case ESessionTransitionKind::Menu:
            case ESessionTransitionKind::NewGame:
            case ESessionTransitionKind::LoadSave:
            case ESessionTransitionKind::Shutdown:
                return true;
            }
        }
        return false;

    case EGV2SessionState::Creating:
        if (TargetState == EGV2SessionState::Registering)
        {
            switch (TransitionKind)
            {
            case ESessionTransitionKind::Menu:
            case ESessionTransitionKind::NewGame:
            case ESessionTransitionKind::LoadSave:
                return true;
            case ESessionTransitionKind::Shutdown:
                return false;
            }
        }
        if (TargetState == EGV2SessionState::Stopping)
        {
            switch (TransitionKind)
            {
            case ESessionTransitionKind::Menu:
            case ESessionTransitionKind::NewGame:
            case ESessionTransitionKind::LoadSave:
            case ESessionTransitionKind::Shutdown:
                return true;
            }
        }
        return false;

    case EGV2SessionState::Registering:
        if (TargetState == EGV2SessionState::BuildingState)
        {
            switch (TransitionKind)
            {
            case ESessionTransitionKind::Menu:
            case ESessionTransitionKind::NewGame:
            case ESessionTransitionKind::LoadSave:
                return true;
            case ESessionTransitionKind::Shutdown:
                return false;
            }
        }
        if (TargetState == EGV2SessionState::Stopping)
        {
            switch (TransitionKind)
            {
            case ESessionTransitionKind::Menu:
            case ESessionTransitionKind::NewGame:
            case ESessionTransitionKind::LoadSave:
            case ESessionTransitionKind::Shutdown:
                return true;
            }
        }
        return false;

    case EGV2SessionState::BuildingState:
        if (TargetState == EGV2SessionState::RestoringInstances)
        {
            switch (TransitionKind)
            {
            case ESessionTransitionKind::Menu:
            case ESessionTransitionKind::NewGame:
            case ESessionTransitionKind::LoadSave:
                return true;
            case ESessionTransitionKind::Shutdown:
                return false;
            }
        }
        if (TargetState == EGV2SessionState::Stopping)
        {
            switch (TransitionKind)
            {
            case ESessionTransitionKind::Menu:
            case ESessionTransitionKind::NewGame:
            case ESessionTransitionKind::LoadSave:
            case ESessionTransitionKind::Shutdown:
                return true;
            }
        }
        return false;

    case EGV2SessionState::RestoringInstances:
        if (TargetState == EGV2SessionState::Starting)
        {
            switch (TransitionKind)
            {
            case ESessionTransitionKind::Menu:
            case ESessionTransitionKind::NewGame:
            case ESessionTransitionKind::LoadSave:
                return true;
            case ESessionTransitionKind::Shutdown:
                return false;
            }
        }
        if (TargetState == EGV2SessionState::Stopping)
        {
            switch (TransitionKind)
            {
            case ESessionTransitionKind::Menu:
            case ESessionTransitionKind::NewGame:
            case ESessionTransitionKind::LoadSave:
            case ESessionTransitionKind::Shutdown:
                return true;
            }
        }
        return false;

    case EGV2SessionState::Starting:
        if (TargetState == EGV2SessionState::PreparingPresentation)
        {
            switch (TransitionKind)
            {
            case ESessionTransitionKind::Menu:
            case ESessionTransitionKind::NewGame:
            case ESessionTransitionKind::LoadSave:
                return true;
            case ESessionTransitionKind::Shutdown:
                return false;
            }
        }
        if (TargetState == EGV2SessionState::Stopping)
        {
            switch (TransitionKind)
            {
            case ESessionTransitionKind::Menu:
            case ESessionTransitionKind::NewGame:
            case ESessionTransitionKind::LoadSave:
            case ESessionTransitionKind::Shutdown:
                return true;
            }
        }
        return false;

    case EGV2SessionState::PreparingPresentation:
        if (TargetState == EGV2SessionState::Ready)
        {
            switch (TransitionKind)
            {
            case ESessionTransitionKind::Menu:
            case ESessionTransitionKind::NewGame:
            case ESessionTransitionKind::LoadSave:
                return true;
            case ESessionTransitionKind::Shutdown:
                return false;
            }
        }
        if (TargetState == EGV2SessionState::Stopping)
        {
            switch (TransitionKind)
            {
            case ESessionTransitionKind::Menu:
            case ESessionTransitionKind::NewGame:
            case ESessionTransitionKind::LoadSave:
            case ESessionTransitionKind::Shutdown:
                return true;
            }
        }
        return false;

    case EGV2SessionState::Ready:
        if (TargetState == EGV2SessionState::Creating)
        {
            switch (TransitionKind)
            {
            case ESessionTransitionKind::Menu:
            case ESessionTransitionKind::NewGame:
            case ESessionTransitionKind::LoadSave:
                return true;
            case ESessionTransitionKind::Shutdown:
                return false;
            }
        }
        if (TargetState == EGV2SessionState::Stopping)
        {
            switch (TransitionKind)
            {
            case ESessionTransitionKind::Menu:
            case ESessionTransitionKind::NewGame:
            case ESessionTransitionKind::LoadSave:
            case ESessionTransitionKind::Shutdown:
                return true;
            }
        }
        return false;

    case EGV2SessionState::Stopping:
        if (TargetState == EGV2SessionState::None || TargetState == EGV2SessionState::Failed || TargetState == EGV2SessionState::Destroyed)
        {
            switch (TransitionKind)
            {
            case ESessionTransitionKind::Menu:
            case ESessionTransitionKind::NewGame:
            case ESessionTransitionKind::LoadSave:
            case ESessionTransitionKind::Shutdown:
                return true;
            }
        }
        return false;

    case EGV2SessionState::Failed:
        if (TargetState == EGV2SessionState::None || TargetState == EGV2SessionState::Destroyed)
        {
            switch (TransitionKind)
            {
            case ESessionTransitionKind::Menu:
            case ESessionTransitionKind::NewGame:
            case ESessionTransitionKind::LoadSave:
            case ESessionTransitionKind::Shutdown:
                return true;
            }
        }
        if (TargetState == EGV2SessionState::Creating)
        {
            switch (TransitionKind)
            {
            case ESessionTransitionKind::Menu:
            case ESessionTransitionKind::NewGame:
            case ESessionTransitionKind::LoadSave:
                return true;
            case ESessionTransitionKind::Shutdown:
                return false;
            }
        }
        return false;

    case EGV2SessionState::Destroyed:
        if (TargetState == EGV2SessionState::None)
        {
            switch (TransitionKind)
            {
            case ESessionTransitionKind::Menu:
            case ESessionTransitionKind::NewGame:
            case ESessionTransitionKind::LoadSave:
            case ESessionTransitionKind::Shutdown:
                return true;
            }
        }
        if (TargetState == EGV2SessionState::Creating)
        {
            switch (TransitionKind)
            {
            case ESessionTransitionKind::Menu:
            case ESessionTransitionKind::NewGame:
            case ESessionTransitionKind::LoadSave:
                return true;
            case ESessionTransitionKind::Shutdown:
                return false;
            }
        }
        return false;
    }

    return false;
}

bool FGV2SessionTransitionOracle::CanTransitionApplicationState(
    const EGV2ApplicationState CurrentState,
    const EGV2ApplicationState TargetState)
{
    if (CurrentState == TargetState)
    {
        return false;
    }

    switch (CurrentState)
    {
    case EGV2ApplicationState::Uninitialized:
        return TargetState == EGV2ApplicationState::Bootstrapping
            || TargetState == EGV2ApplicationState::MenuActive
            || TargetState == EGV2ApplicationState::GameActive
            || TargetState == EGV2ApplicationState::Failed
            || TargetState == EGV2ApplicationState::Terminated;

    case EGV2ApplicationState::Bootstrapping:
        return TargetState == EGV2ApplicationState::MenuActive
            || TargetState == EGV2ApplicationState::GameActive
            || TargetState == EGV2ApplicationState::Transitioning
            || TargetState == EGV2ApplicationState::Failed
            || TargetState == EGV2ApplicationState::ShuttingDown
            || TargetState == EGV2ApplicationState::Uninitialized;

    case EGV2ApplicationState::MenuActive:
        return TargetState == EGV2ApplicationState::Bootstrapping
            || TargetState == EGV2ApplicationState::GameActive
            || TargetState == EGV2ApplicationState::Transitioning
            || TargetState == EGV2ApplicationState::ShuttingDown
            || TargetState == EGV2ApplicationState::Failed
            || TargetState == EGV2ApplicationState::Uninitialized;

    case EGV2ApplicationState::GameActive:
        return TargetState == EGV2ApplicationState::Bootstrapping
            || TargetState == EGV2ApplicationState::MenuActive
            || TargetState == EGV2ApplicationState::Transitioning
            || TargetState == EGV2ApplicationState::ShuttingDown
            || TargetState == EGV2ApplicationState::Failed
            || TargetState == EGV2ApplicationState::Uninitialized;

    case EGV2ApplicationState::Transitioning:
        return TargetState == EGV2ApplicationState::MenuActive
            || TargetState == EGV2ApplicationState::GameActive
            || TargetState == EGV2ApplicationState::ShuttingDown
            || TargetState == EGV2ApplicationState::Failed
            || TargetState == EGV2ApplicationState::Uninitialized;

    case EGV2ApplicationState::ShuttingDown:
        return TargetState == EGV2ApplicationState::Terminated
            || TargetState == EGV2ApplicationState::Uninitialized;

    case EGV2ApplicationState::Failed:
        return TargetState == EGV2ApplicationState::Bootstrapping
            || TargetState == EGV2ApplicationState::MenuActive
            || TargetState == EGV2ApplicationState::ShuttingDown
            || TargetState == EGV2ApplicationState::Uninitialized;

    case EGV2ApplicationState::Terminated:
        return TargetState == EGV2ApplicationState::Uninitialized;
    }

    return false;
}

bool TryTransitionSessionState(
    FGV2SessionStatus& InOutStatus,
    const EGV2SessionState TargetState,
    const ESessionTransitionKind TransitionKind,
    FString* OutError)
{
    if (InOutStatus.SessionState == TargetState)
    {
        return true;
    }

    if (FGV2SessionTransitionOracle::CanTransitionSessionState(InOutStatus.SessionState, TargetState, TransitionKind))
    {
        InOutStatus.SessionState = TargetState;
        return true;
    }

    const FString Error = FString::Printf(
        TEXT("Invalid session state transition from %d to %d for kind %s"),
        static_cast<int32>(InOutStatus.SessionState),
        static_cast<int32>(TargetState),
        LexToString(TransitionKind));
    if (OutError != nullptr)
    {
        *OutError = Error;
    }
    UE_LOG(LogGV2SessionTransition, Error, TEXT("%s"), *Error);
    return false;
}

bool TryTransitionApplicationState(
    FGV2SessionStatus& InOutStatus,
    const EGV2ApplicationState TargetState,
    FString* OutError)
{
    if (InOutStatus.ApplicationState == TargetState)
    {
        return true;
    }

    if (FGV2SessionTransitionOracle::CanTransitionApplicationState(InOutStatus.ApplicationState, TargetState))
    {
        InOutStatus.ApplicationState = TargetState;
        return true;
    }

    const FString Error = FString::Printf(
        TEXT("Invalid application state transition from %d to %d"),
        static_cast<int32>(InOutStatus.ApplicationState),
        static_cast<int32>(TargetState));
    if (OutError != nullptr)
    {
        *OutError = Error;
    }
    UE_LOG(LogGV2SessionTransition, Error, TEXT("%s"), *Error);
    return false;
}

FGV2SessionTransitionPolicy::FGV2SessionTransitionPolicy(const int32 InMaxRetainedOutcomes)
    : MaxRetainedOutcomes(InMaxRetainedOutcomes)
{
}

uint64 FGV2SessionTransitionPolicy::EnqueueRequest(
    const FSessionStartDescriptor& Descriptor,
    bool& bOutJoined,
    uint64& OutJoinedOpId)
{
    if (ActiveOperation.IsSet() && ActiveOperation->Descriptor.IsEquivalentTo(Descriptor))
    {
        bOutJoined = true;
        OutJoinedOpId = ActiveOperation->OperationId;
        return OutJoinedOpId;
    }
    if (PendingSlot.IsSet() && PendingSlot->Descriptor.IsEquivalentTo(Descriptor))
    {
        bOutJoined = true;
        OutJoinedOpId = PendingSlot->OperationId;
        return OutJoinedOpId;
    }
    if (PendingSlot.IsSet())
    {
        RecordOutcome(PendingSlot->OperationId, ESessionNonFailureOutcome::Superseded);
        PendingSlot.Reset();
    }

    bOutJoined = false;
    const uint64 OpId = AllocateOperationId();
    OutJoinedOpId = OpId;

    FSessionOperationRecord Record;
    Record.OperationId = OpId;
    Record.Descriptor = Descriptor;
    Record.Kind = ToTransitionKind(Descriptor.Mode);
    Record.bCancellationRequested = false;
    Record.bCommitted = false;

    PendingSlot = MoveTemp(Record);
    return OpId;
}

uint64 FGV2SessionTransitionPolicy::EnqueueShutdown(
    bool& bOutJoined,
    uint64& OutJoinedOpId)
{
    if (PendingSlot.IsSet() && PendingSlot->Kind == ESessionTransitionKind::Shutdown)
    {
        bOutJoined = true;
        OutJoinedOpId = PendingSlot->OperationId;
        return OutJoinedOpId;
    }

    if (PendingSlot.IsSet())
    {
        RecordOutcome(PendingSlot->OperationId, ESessionNonFailureOutcome::Superseded);
        PendingSlot.Reset();
    }

    if (ActiveOperation.IsSet() && !ActiveOperation->bCommitted)
    {
        ActiveOperation->bCancellationRequested = true;
    }

    bOutJoined = false;
    const uint64 OpId = AllocateOperationId();
    OutJoinedOpId = OpId;

    FSessionOperationRecord Record;
    Record.OperationId = OpId;
    Record.Kind = ESessionTransitionKind::Shutdown;
    Record.bCancellationRequested = false;
    Record.bCommitted = false;

    PendingSlot = MoveTemp(Record);
    return OpId;
}

ESessionCancellationResult FGV2SessionTransitionPolicy::CancelRequest(const uint64 OperationId)
{
    if (PendingSlot.IsSet() && PendingSlot->OperationId == OperationId)
    {
        PendingSlot.Reset();
        RecordOutcome(OperationId, ESessionNonFailureOutcome::Cancelled);
        return ESessionCancellationResult::Accepted;
    }

    if (ActiveOperation.IsSet() && ActiveOperation->OperationId == OperationId)
    {
        if (ActiveOperation->bCommitted)
        {
            return ESessionCancellationResult::TooLate;
        }
        ActiveOperation->bCancellationRequested = true;
        return ESessionCancellationResult::Accepted;
    }

    return ESessionCancellationResult::Stale;
}

TOptional<FGV2SessionOperationResult> FGV2SessionTransitionPolicy::GetOutcome(const uint64 OperationId) const
{
    if (const FGV2SessionOperationResult* Found = OperationOutcomes.Find(OperationId))
    {
        return *Found;
    }
    return TOptional<FGV2SessionOperationResult>();
}

bool FGV2SessionTransitionPolicy::IsOperationKnown(const uint64 OperationId) const
{
    return OperationId > 0 && OperationId < NextOperationId;
}

bool FGV2SessionTransitionPolicy::IsOperationEvicted(const uint64 OperationId) const
{
    if (OperationId == 0 || OperationId >= NextOperationId)
    {
        return false;
    }
    if (OperationOutcomes.Contains(OperationId))
    {
        return false;
    }
    if (InProgressOperations.Contains(OperationId))
    {
        return false;
    }
    return true;
}

ESessionOperationQueryStatus FGV2SessionTransitionPolicy::QueryOutcome(
    const uint64 OperationId,
    FGV2SessionOperationResult* OutResult) const
{
    if (OperationId == 0 || OperationId >= NextOperationId)
    {
        return ESessionOperationQueryStatus::Unknown;
    }

    if (const FGV2SessionOperationResult* Found = OperationOutcomes.Find(OperationId))
    {
        if (OutResult)
        {
            *OutResult = *Found;
        }
        return ESessionOperationQueryStatus::Found;
    }

    if (InProgressOperations.Contains(OperationId))
    {
        return ESessionOperationQueryStatus::InProgress;
    }

    return ESessionOperationQueryStatus::Evicted;
}

TOptional<FSessionOperationRecord> FGV2SessionTransitionPolicy::DequeuePendingOperation()
{
    if (!PendingSlot.IsSet())
    {
        return TOptional<FSessionOperationRecord>();
    }
    ActiveOperation = MoveTemp(*PendingSlot);
    PendingSlot.Reset();
    return ActiveOperation;
}

void FGV2SessionTransitionPolicy::RecordResultInternal(const uint64 OperationId, FGV2SessionOperationResult Result)
{
    InProgressOperations.Remove(OperationId);

    if (OperationOutcomes.Contains(OperationId))
    {
        OperationOutcomes[OperationId] = MoveTemp(Result);
        return;
    }

    if (MaxRetainedOutcomes > 0 && OperationOutcomes.Num() >= MaxRetainedOutcomes)
    {
        uint64 EarliestId = TNumericLimits<uint64>::Max();
        for (const auto& Pair : OperationOutcomes)
        {
            if (Pair.Key < EarliestId)
            {
                EarliestId = Pair.Key;
            }
        }
        if (EarliestId != TNumericLimits<uint64>::Max())
        {
            OperationOutcomes.Remove(EarliestId);
            HighestEvictedOperationId = FMath::Max(HighestEvictedOperationId, EarliestId);
        }
    }

    OperationOutcomes.Add(OperationId, MoveTemp(Result));
}

void FGV2SessionTransitionPolicy::RecordOutcome(const uint64 OperationId, const ESessionNonFailureOutcome Outcome)
{
    RecordResultInternal(OperationId, FGV2SessionOperationResult::MakeSuccess(Outcome));
    if (ActiveOperation.IsSet() && ActiveOperation->OperationId == OperationId)
    {
        ActiveOperation.Reset();
    }
}

void FGV2SessionTransitionPolicy::RecordFailure(const uint64 OperationId, const FGV2OperationFault& Fault)
{
    checkf(Fault.IsSet(), TEXT("RecordFailure requires an initialized fault with non-empty Code"));
    RecordResultInternal(OperationId, FGV2SessionOperationResult::MakeFailure(Fault));
    if (ActiveOperation.IsSet() && ActiveOperation->OperationId == OperationId)
    {
        ActiveOperation.Reset();
    }
}

void FGV2SessionTransitionPolicy::RecordFailure(const uint64 OperationId, const GV2RuntimeCore::FRuntimeFault& Fault)
{
    checkf(!Fault.Code.empty(), TEXT("RecordFailure requires non-empty runtime fault code"));
    const FGV2OperationFault OpFault(UTF8_TO_TCHAR(Fault.Code.c_str()), UTF8_TO_TCHAR(Fault.Message.c_str()));
    RecordFailure(OperationId, OpFault);
}

uint64 FGV2SessionTransitionPolicy::AllocateOperationId()
{
    const uint64 OpId = NextOperationId++;
    InProgressOperations.Add(OpId);
    return OpId;
}
