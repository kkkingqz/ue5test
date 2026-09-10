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
