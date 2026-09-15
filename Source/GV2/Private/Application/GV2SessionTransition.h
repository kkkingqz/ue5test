#pragma once

#include "CoreMinimal.h"
#include "Bridge/GV2BridgeTypes.h"

// Closed set of session transition kinds (CFC-07)
enum class ESessionTransitionKind : uint8
{
    Menu,
    NewGame,
    LoadSave,
    Shutdown
};

inline constexpr uint8 SessionTransitionKindCount = 4;
static_assert(static_cast<uint8>(ESessionTransitionKind::Shutdown) + 1 == SessionTransitionKindCount,
    "SessionTransitionKindCount must match the number of ESessionTransitionKind enumerators.");

inline constexpr ESessionTransitionKind AllSessionTransitionKinds[SessionTransitionKindCount] = {
    ESessionTransitionKind::Menu,
    ESessionTransitionKind::NewGame,
    ESessionTransitionKind::LoadSave,
    ESessionTransitionKind::Shutdown,
};

inline const TCHAR* LexToString(ESessionTransitionKind Kind)
{
    switch (Kind)
    {
    case ESessionTransitionKind::Menu:
        return TEXT("Menu");
    case ESessionTransitionKind::NewGame:
        return TEXT("NewGame");
    case ESessionTransitionKind::LoadSave:
        return TEXT("LoadSave");
    case ESessionTransitionKind::Shutdown:
        return TEXT("Shutdown");
    }
}

inline ESessionTransitionKind ToTransitionKind(ESessionStartMode Mode)
{
    switch (Mode)
    {
    case ESessionStartMode::Menu:
        return ESessionTransitionKind::Menu;
    case ESessionStartMode::NewGame:
        return ESessionTransitionKind::NewGame;
    case ESessionStartMode::LoadSave:
        return ESessionTransitionKind::LoadSave;
    }
}

// Independent transition oracle matrix for session & application states
struct GV2_API FGV2SessionTransitionOracle
{
    static bool CanTransitionSessionState(
        EGV2SessionState CurrentState,
        EGV2SessionState TargetState,
        ESessionTransitionKind TransitionKind);

    static bool CanTransitionApplicationState(
        EGV2ApplicationState CurrentState,
        EGV2ApplicationState TargetState);
};

// Safe state mutation routines checking oracle
bool TryTransitionSessionState(
    FGV2SessionStatus& InOutStatus,
    EGV2SessionState TargetState,
    ESessionTransitionKind TransitionKind,
    FString* OutError = nullptr);

bool TryTransitionApplicationState(
    FGV2SessionStatus& InOutStatus,
    EGV2ApplicationState TargetState,
    FString* OutError = nullptr);

struct FSessionOperationRecord
{
    uint64 OperationId = 0;
    FSessionStartDescriptor Descriptor;
    ESessionTransitionKind Kind = ESessionTransitionKind::NewGame;
    bool bCancellationRequested = false;
    bool bCommitted = false; // true once BeginReplace has completed
};

namespace GV2RuntimeCore { struct FRuntimeFault; }

class GV2_API FGV2SessionTransitionPolicy
{
public:
    FGV2SessionTransitionPolicy() = default;

    // Returns operation ID and whether request was joined with existing
    uint64 EnqueueRequest(const FSessionStartDescriptor& Descriptor, bool& bOutJoined, uint64& OutJoinedOpId);

    // Shutdown priority
    uint64 EnqueueShutdown(bool& bOutJoined, uint64& OutJoinedOpId);
    uint64 EnqueueShutdown()
    {
        bool bJoined = false;
        uint64 JoinedOpId = 0;
        return EnqueueShutdown(bJoined, JoinedOpId);
    }

    ESessionCancellationResult CancelRequest(uint64 OperationId);

    TOptional<FGV2SessionOperationResult> GetOutcome(uint64 OperationId) const;

    bool HasPendingOperation() const { return PendingSlot.IsSet(); }
    const TOptional<FSessionOperationRecord>& GetPendingOperation() const { return PendingSlot; }
    TOptional<FSessionOperationRecord> DequeuePendingOperation();

    const TOptional<FSessionOperationRecord>& GetActiveOperation() const { return ActiveOperation; }
    TOptional<FSessionOperationRecord>& GetActiveOperation() { return ActiveOperation; }

    void SetActiveOperation(FSessionOperationRecord InOp) { ActiveOperation = MoveTemp(InOp); }
    void ClearActiveOperation() { ActiveOperation.Reset(); }

    void RecordOutcome(uint64 OperationId, ESessionNonFailureOutcome Outcome);
    void RecordOutcome(uint64 OperationId, ESessionOperationOutcome Outcome) = delete;
    void RecordFailure(uint64 OperationId, const FGV2OperationFault& Fault);
    void RecordFailure(uint64 OperationId, const GV2RuntimeCore::FRuntimeFault& Fault);

    uint64 AllocateOperationId();

    void Reset();

private:
    uint64 NextOperationId = 1;
    TOptional<FSessionOperationRecord> ActiveOperation;
    TOptional<FSessionOperationRecord> PendingSlot;
    TMap<uint64, FGV2SessionOperationResult> OperationOutcomes;
};
