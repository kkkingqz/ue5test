#pragma once

#include "CoreMinimal.h"
#include "GV2PresentationApply/GV2WidgetTypes.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"
#include "Curves/CurveFloat.h"
#include "Styling/SlateTypes.h"

#include <memory>

namespace GV2ContentCore { struct FCompiledUiFieldSpec; }
class FGV2PreparedUiObject;
class UCommonTextStyle;

#include "GV2BridgeTypes.generated.h"

UENUM(BlueprintType)
enum class EGV2ApplicationState : uint8
{
    Uninitialized,
    Bootstrapping,
    MenuActive,
    GameActive,
    Transitioning,
    ShuttingDown,
    Failed,
    Terminated
};

UENUM(BlueprintType)
enum class EGV2SessionState : uint8
{
    None,
    Creating,
    Registering,
    BuildingState,
    RestoringInstances,
    Starting,
    PreparingPresentation,
    Ready,
    Failed,
    Stopping,
    Destroyed
};

UENUM(BlueprintType)
enum class ESessionStartMode : uint8
{
    Menu,
    NewGame,
    LoadSave
};
using EGV2SessionStartMode = ESessionStartMode;

UENUM(BlueprintType)
enum class ESessionNonFailureOutcome : uint8
{
    Completed,
    Cancelled,
    Superseded
};
using EGV2SessionNonFailureOutcome = ESessionNonFailureOutcome;

UENUM(BlueprintType)
enum class ESessionOperationOutcome : uint8
{
    Completed,
    Failed,
    Cancelled,
    Superseded
};
using EGV2SessionOperationOutcome = ESessionOperationOutcome;

#define GV2_SESSION_FAULT_CODES(OP) \
    OP(RepositoryNotReady) \
    OP(InvalidSessionDescriptor) \
    OP(SessionNotReady) \
    OP(InvalidSaveSlotId) \
    OP(SaveSlotStorageUnavailable) \
    OP(SaveSlotNotFound) \
    OP(SaveSlotUnreadable) \
    OP(RepositoryVersionChanged) \
    OP(LuaRuntimeSourceMissing) \
    OP(LuaRuntimeSourceInvalid) \
    OP(UiSchemaNotReady) \
    OP(ScreenRegistryNotReady) \
    OP(ImageCatalogNotReady) \
    OP(ThemeNotReady) \
    OP(SessionCandidateBuildFailed) \
    OP(InitialPresentationMissing) \
    OP(InitialPresentationInvalid) \
    OP(InitialPresentationApplyFailed) \
    OP(InitialPresentationCommitFailed) \
    OP(PublishReadyFailed) \
    OP(SessionShutdown) \
    OP(NoPendingStartContext) \
    OP(LuaExecutionFailed) \
    OP(RuntimeFault)

enum class EGV2SessionFaultCode : uint8
{
#define GV2_EXPAND_FAULT_CODE_ENUM(Name) Name,
    GV2_SESSION_FAULT_CODES(GV2_EXPAND_FAULT_CODE_ENUM)
#undef GV2_EXPAND_FAULT_CODE_ENUM
};

struct GV2_API FGV2SessionFaultCodes
{
#define GV2_EXPAND_FAULT_CODE_MEMBER(Name) inline static const FString Name = TEXT(#Name);
    GV2_SESSION_FAULT_CODES(GV2_EXPAND_FAULT_CODE_MEMBER)
#undef GV2_EXPAND_FAULT_CODE_MEMBER

    static FString ToString(EGV2SessionFaultCode Code)
    {
        switch (Code)
        {
#define GV2_EXPAND_FAULT_CODE_CASE(Name) case EGV2SessionFaultCode::Name: return Name;
            GV2_SESSION_FAULT_CODES(GV2_EXPAND_FAULT_CODE_CASE)
#undef GV2_EXPAND_FAULT_CODE_CASE
        }
        checkNoEntry();
        return RuntimeFault;
    }

    static TArray<EGV2SessionFaultCode> GetAllDeclaredFaultKinds()
    {
        return {
#define GV2_EXPAND_FAULT_KIND_ARRAY(Name) EGV2SessionFaultCode::Name,
            GV2_SESSION_FAULT_CODES(GV2_EXPAND_FAULT_KIND_ARRAY)
#undef GV2_EXPAND_FAULT_KIND_ARRAY
        };
    }

    static TArray<FString> GetAllDeclaredFaultCodes()
    {
        TArray<FString> Codes;
        for (const EGV2SessionFaultCode Kind : GetAllDeclaredFaultKinds())
        {
            Codes.Add(ToString(Kind));
        }
        return Codes;
    }

    static bool TryParse(const FString& Code, EGV2SessionFaultCode& OutCode)
    {
#define GV2_EXPAND_FAULT_CODE_PARSE(Name) if (Code == Name) { OutCode = EGV2SessionFaultCode::Name; return true; }
        GV2_SESSION_FAULT_CODES(GV2_EXPAND_FAULT_CODE_PARSE)
#undef GV2_EXPAND_FAULT_CODE_PARSE
        return false;
    }

    static bool IsDeclared(const FString& Code)
    {
        EGV2SessionFaultCode Ignored = EGV2SessionFaultCode::RuntimeFault;
        return TryParse(Code, Ignored);
    }
};

USTRUCT(BlueprintType)
struct GV2_API FGV2OperationFault
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "GV2|Runtime")
    FString Code;

    UPROPERTY(BlueprintReadOnly, Category = "GV2|Runtime")
    FString Message;

    // Original subsystem/Lua diagnostic code when Code == RuntimeFault.
    UPROPERTY(BlueprintReadOnly, Category = "GV2|Runtime")
    FString CauseCode;

    bool IsSet() const { return !Code.IsEmpty(); }

    static FGV2OperationFault None()
    {
        return FGV2OperationFault();
    }
};
using FOperationFault = FGV2OperationFault;

// Non-optional construction token for the Failed branch. Unlike the public DTO,
// this type cannot be default-constructed and its top-level code comes only from
// the declared enum catalog.
class GV2_API FGV2RequiredOperationFault final
{
public:
    explicit FGV2RequiredOperationFault(
        EGV2SessionFaultCode InCode,
        FString InMessage,
        FString InCauseCode = FString())
    {
        Fault.Code = FGV2SessionFaultCodes::ToString(InCode);
        Fault.Message = MoveTemp(InMessage);
        Fault.CauseCode = MoveTemp(InCauseCode);
    }

    const FGV2OperationFault& ToDto() const { return Fault; }

private:
    FGV2OperationFault Fault;
};

USTRUCT(BlueprintType)
struct GV2_API FGV2SessionOperationResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "GV2|Runtime")
    ESessionOperationOutcome Outcome = ESessionOperationOutcome::Completed;

    UPROPERTY(BlueprintReadOnly, Category = "GV2|Runtime")
    FGV2OperationFault Fault;

    bool IsCompleted() const { return Outcome == ESessionOperationOutcome::Completed; }
    bool IsFailed() const { return Outcome == ESessionOperationOutcome::Failed; }
    bool IsCancelled() const { return Outcome == ESessionOperationOutcome::Cancelled; }
    bool IsSuperseded() const { return Outcome == ESessionOperationOutcome::Superseded; }

    static FGV2SessionOperationResult MakeSuccess(ESessionNonFailureOutcome NonFailure = ESessionNonFailureOutcome::Completed)
    {
        FGV2SessionOperationResult Result;
        switch (NonFailure)
        {
        case ESessionNonFailureOutcome::Completed:
            Result.Outcome = ESessionOperationOutcome::Completed;
            break;
        case ESessionNonFailureOutcome::Cancelled:
            Result.Outcome = ESessionOperationOutcome::Cancelled;
            break;
        case ESessionNonFailureOutcome::Superseded:
            Result.Outcome = ESessionOperationOutcome::Superseded;
            break;
        }
        return Result;
    }

    static FGV2SessionOperationResult MakeFailure(const FGV2RequiredOperationFault& InFault)
    {
        FGV2SessionOperationResult Result;
        Result.Outcome = ESessionOperationOutcome::Failed;
        Result.Fault = InFault.ToDto();
        return Result;
    }

    bool operator==(ESessionOperationOutcome InOutcome) const { return Outcome == InOutcome; }
    bool operator!=(ESessionOperationOutcome InOutcome) const { return Outcome != InOutcome; }
    bool operator==(const FGV2SessionOperationResult& Other) const
    {
        return Outcome == Other.Outcome
            && Fault.Code == Other.Fault.Code
            && Fault.CauseCode == Other.Fault.CauseCode;
    }
    bool operator!=(const FGV2SessionOperationResult& Other) const
    {
        return !(*this == Other);
    }
};
using FSessionOperationResult = FGV2SessionOperationResult;

inline bool operator==(ESessionOperationOutcome Lhs, const FGV2SessionOperationResult& Rhs)
{
    return Lhs == Rhs.Outcome;
}
inline bool operator!=(ESessionOperationOutcome Lhs, const FGV2SessionOperationResult& Rhs)
{
    return Lhs != Rhs.Outcome;
}

inline FString LexToString(const FGV2SessionOperationResult& Res)
{
    const FString FaultText = Res.Fault.CauseCode.IsEmpty()
        ? FString::Printf(TEXT("%s: %s"), *Res.Fault.Code, *Res.Fault.Message)
        : FString::Printf(TEXT("%s[%s]: %s"), *Res.Fault.Code, *Res.Fault.CauseCode, *Res.Fault.Message);
    return FString::Printf(
        TEXT("%s (Fault: %s)"),
        *UEnum::GetValueAsString(Res.Outcome),
        *FaultText);
}

inline FString LexToString(const FGV2OperationFault& Fault)
{
    return Fault.CauseCode.IsEmpty()
        ? FString::Printf(TEXT("%s: %s"), *Fault.Code, *Fault.Message)
        : FString::Printf(TEXT("%s[%s]: %s"), *Fault.Code, *Fault.CauseCode, *Fault.Message);
}

UENUM(BlueprintType)
enum class ESessionOperationQueryStatus : uint8
{
    Unknown,
    InProgress,
    Found,
    Evicted
};
using EGV2SessionOperationQueryStatus = ESessionOperationQueryStatus;

inline FString LexToString(ESessionOperationQueryStatus Status)
{
    switch (Status)
    {
    case ESessionOperationQueryStatus::Unknown:
        return TEXT("Unknown");
    case ESessionOperationQueryStatus::InProgress:
        return TEXT("InProgress");
    case ESessionOperationQueryStatus::Found:
        return TEXT("Found");
    case ESessionOperationQueryStatus::Evicted:
        return TEXT("Evicted");
    }
    return TEXT("Unknown");
}

UENUM(BlueprintType)
enum class ESessionCancellationResult : uint8
{
    Accepted,
    TooLate,
    Stale
};
using EGV2SessionCancellationResult = ESessionCancellationResult;

UENUM(BlueprintType)
enum class EGV2SaveSlotRevision : uint8
{
    Current,
    Previous
};
using ESaveSlotRevision = EGV2SaveSlotRevision;

USTRUCT(BlueprintType)
struct GV2_API FSessionStartDescriptor
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|Lifecycle")
    ESessionStartMode Mode = ESessionStartMode::NewGame;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|Lifecycle")
    FString SaveSlotId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|Lifecycle")
    FString SaveSlotRevision = TEXT("Current");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|Lifecycle")
    FString RepositoryVersion;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|Lifecycle")
    FString RepositoryContentHash;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|Lifecycle")
    FString SeedHex;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|Lifecycle")
    FString Reason;

    bool operator==(const FSessionStartDescriptor& Other) const
    {
        return Mode == Other.Mode
            && SaveSlotId == Other.SaveSlotId
            && SaveSlotRevision == Other.SaveSlotRevision
            && RepositoryVersion == Other.RepositoryVersion
            && RepositoryContentHash == Other.RepositoryContentHash
            && SeedHex == Other.SeedHex
            && Reason == Other.Reason;
    }

    bool operator!=(const FSessionStartDescriptor& Other) const
    {
        return !(*this == Other);
    }

    bool IsEquivalentTo(const FSessionStartDescriptor& Other) const
    {
        return Mode == Other.Mode
            && SaveSlotId == Other.SaveSlotId
            && SaveSlotRevision == Other.SaveSlotRevision
            && RepositoryVersion == Other.RepositoryVersion
            && RepositoryContentHash == Other.RepositoryContentHash
            && SeedHex == Other.SeedHex;
    }

    static FString GenerateFreshSeedHex()
    {
        const FGuid Guid = FGuid::NewGuid();
        const uint64 Cycles = FPlatformTime::Cycles64();
        const uint64 High = static_cast<uint64>(Guid.A) ^ (static_cast<uint64>(Guid.C) << 32);
        const uint64 Low = static_cast<uint64>(Guid.B) | (static_cast<uint64>(Guid.D) << 32);
        const uint64 Seed = (High ^ Low) ^ Cycles;
        return FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(Seed));
    }

    static bool IsValidSeedHex(const FString& InSeedHex)
    {
        if (InSeedHex.Len() != 16)
        {
            return false;
        }
        for (TCHAR Ch : InSeedHex)
        {
            const bool bIsDigit = Ch >= TEXT('0') && Ch <= TEXT('9');
            const bool bIsLowerHex = Ch >= TEXT('a') && Ch <= TEXT('f');
            if (!bIsDigit && !bIsLowerHex)
            {
                return false;
            }
        }
        return true;
    }

    bool IsValid(FString* OutError = nullptr) const
    {
        if (Mode == ESessionStartMode::LoadSave)
        {
            if (SaveSlotId.IsEmpty())
            {
                if (OutError != nullptr)
                {
                    *OutError = TEXT("SaveSlotId must not be empty for LoadSave mode.");
                }
                return false;
            }
            if (SaveSlotRevision != TEXT("Current") && SaveSlotRevision != TEXT("Previous"))
            {
                if (OutError != nullptr)
                {
                    *OutError = TEXT("SaveSlotRevision must be 'Current' or 'Previous'.");
                }
                return false;
            }
            if (!SeedHex.IsEmpty())
            {
                if (OutError != nullptr)
                {
                    *OutError = TEXT("SeedHex must be empty for LoadSave mode (restored from save).");
                }
                return false;
            }
            return true;
        }
        if (!IsValidSeedHex(SeedHex))
        {
            if (OutError != nullptr)
            {
                *OutError = TEXT("SeedHex must be exactly 16 lowercase hex characters.");
            }
            return false;
        }
        return true;
    }

    bool Validate(FString* OutError = nullptr) const
    {
        return IsValid(OutError);
    }
};
using FGV2SessionStartDescriptor = FSessionStartDescriptor;


USTRUCT(BlueprintType)
struct GV2_API FGV2ScreenFieldValue
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Screen")
    FName FieldId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Screen")
    FString SchemaId;

    // UPP-27: materialized candidate value and its compiled schema, built once by
    // GV2ScreenFieldMaterializer::BuildFields from the raw Lua value and consumed by
    // UGV2ScreenWidgetBase's Prepare/Commit. Not UPROPERTY -- neither type is
    // UHT-reflectable, and this payload is transient view-model data, never
    // saved/replicated/Blueprint-authored the way FieldId/SchemaId above are.
    TSharedPtr<const FGV2PreparedUiObject> PreparedValue;
    std::shared_ptr<const GV2ContentCore::FCompiledUiFieldSpec> CompiledSchema;
};

USTRUCT(BlueprintType)
struct GV2_API FGV2ScreenInstanceViewModel
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Screen")
    FName Layer = TEXT("location_content");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Screen")
    FName InstanceKey = TEXT("main");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Screen")
    FString ScreenId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Screen")
    TArray<FGV2ScreenFieldValue> Fields;
};

USTRUCT(BlueprintType)
struct GV2_API FGV2UiDocumentViewModel
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Document")
    FString UiInstanceId = TEXT("ui@default");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Document")
    int64 Revision = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|UI|Document")
    bool bHasRoute = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Document")
    FGV2ScreenInstanceViewModel Route;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Document")
    TArray<FGV2ScreenInstanceViewModel> Overlays;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GV2|UI|Document")
    TArray<FGV2ScreenInstanceViewModel> Modals;

    TArray<FGV2ScreenInstanceViewModel> GetAllScreenInstances() const
    {
        TArray<FGV2ScreenInstanceViewModel> Result;
        if (bHasRoute)
        {
            Result.Add(Route);
        }
        Result.Append(Overlays);
        Result.Append(Modals);
        return Result;
    }
};

USTRUCT(BlueprintType)
struct GV2_API FGV2SessionStatus
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|Runtime")
    EGV2ApplicationState ApplicationState = EGV2ApplicationState::Uninitialized;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|Runtime")
    EGV2SessionState SessionState = EGV2SessionState::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|Runtime")
    bool bIsReady = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|Runtime")
    int32 SessionGeneration = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GV2|Runtime")
    int64 RepositoryVersion = 0;
};
