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
enum class ESessionOperationOutcome : uint8
{
    Completed,
    Failed,
    Cancelled,
    Superseded
};
using EGV2SessionOperationOutcome = ESessionOperationOutcome;

UENUM(BlueprintType)
enum class ESessionCancellationResult : uint8
{
    Accepted,
    TooLate,
    Stale
};
using EGV2SessionCancellationResult = ESessionCancellationResult;

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
    FString SeedHex = TEXT("0000000000000000");

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
