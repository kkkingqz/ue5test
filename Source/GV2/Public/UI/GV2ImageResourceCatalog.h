#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "GV2ImageResourceCatalog.generated.h"

UENUM(BlueprintType)
enum class EGV2ImageRenderMode : uint8
{
    FixedAspect,
    NineSlice,
    Tile
};

UENUM(BlueprintType)
enum class EGV2PrimitiveScalePolicy : uint8
{
    Unset = 0,
    FreeStretch,
    Tile,
    NineSlice,
    PreserveAspect
};

inline bool IsScalePolicyCompatible(EGV2PrimitiveScalePolicy Policy, EGV2ImageRenderMode RenderMode)
{
    switch (Policy)
    {
    case EGV2PrimitiveScalePolicy::Unset:
        return false;
    case EGV2PrimitiveScalePolicy::FreeStretch:
        return RenderMode == EGV2ImageRenderMode::Tile;
    case EGV2PrimitiveScalePolicy::Tile:
        return RenderMode == EGV2ImageRenderMode::Tile;
    case EGV2PrimitiveScalePolicy::NineSlice:
        return RenderMode == EGV2ImageRenderMode::NineSlice;
    case EGV2PrimitiveScalePolicy::PreserveAspect:
        return RenderMode == EGV2ImageRenderMode::FixedAspect;
    default:
        return false;
    }
}

USTRUCT(BlueprintType)
struct GV2_API FGV2ImageResourceDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|Resources|Image")
    FString ResourceId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|Resources|Image")
    TSoftObjectPtr<UTexture2D> Texture;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|Resources|Image")
    EGV2ImageRenderMode RenderMode = EGV2ImageRenderMode::FixedAspect;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|Resources|Image", meta = (ClampMin = "0.01", EditCondition = "RenderMode == EGV2ImageRenderMode::FixedAspect", EditConditionHides))
    float FixedAspectRatio = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|Resources|Image", meta = (EditCondition = "RenderMode == EGV2ImageRenderMode::NineSlice", EditConditionHides))
    FMargin NineSliceBorderPixels;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|Resources|Image", meta = (ClampMin = "1.0", EditCondition = "RenderMode == EGV2ImageRenderMode::Tile", EditConditionHides))
    FVector2D TileSize = FVector2D(128.0f, 128.0f);
};

USTRUCT(BlueprintType)
struct GV2_API FGV2ResolvedImageResource
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "GV2|Resources|Image")
    FString ResourceId;

    UPROPERTY(BlueprintReadOnly, Category = "GV2|Resources|Image")
    EGV2ImageRenderMode RenderMode = EGV2ImageRenderMode::FixedAspect;

    UPROPERTY(BlueprintReadOnly, Category = "GV2|Resources|Image")
    float FixedAspectRatio = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "GV2|Resources|Image")
    FSlateBrush Brush;
};

UCLASS(BlueprintType)
class GV2_API UGV2ImageResourceCatalog : public UDataAsset
{
    GENERATED_BODY()

public:
    const TArray<FGV2ImageResourceDefinition>& GetEntries() const
    {
        return Entries;
    }

    bool Validate(FString& OutError) const;
    bool Resolve(const FString& ResourceId, FGV2ResolvedImageResource& OutResource, FString& OutError) const;
    bool BuildFromDirectory(const FString& RootDirectory, FString& OutError);

    // PAH-04B (ADR-0042, INV-P2): scans the fixed `Resources/` tree (project convention,
    // like `GameData`/`Scripts` -- not a configurable root) via the existing
    // BuildFromDirectory(), then scopes the published entries to resources whose own
    // namespace is a member of PackageIds -- this session's resolved package closure.
    // A resource whose namespace isn't in the closure (e.g. Resources/rh/ when this
    // session's closure is core+textsystem+sample) is excluded from the snapshot, the
    // same way that package's schemas/Lua sources are already silently absent from such
    // a session -- not present, not an error. A structurally malformed path (wrong
    // segment count/grammar) still fails the whole build, inside BuildFromDirectory
    // itself, unchanged.
    bool BuildFromPackageClosure(const TArray<FString>& PackageIds, FString& OutError);

    static bool ValidateDefinition(const FGV2ImageResourceDefinition& Definition, FString& OutError);
    static bool ResolveDefinition(const FGV2ImageResourceDefinition& Definition, FGV2ResolvedImageResource& OutResource, FString& OutError);
    static bool TryMakeResourceId(
        const FString& RootDirectory,
        const FString& PngFilename,
        FString& OutResourceId,
        FString& OutError);

    // PAH-04B: session-scoped, not process-lifetime/config-driven -- mirrors
    // GV2ScreenFieldMaterializer::RebuildSchemaCacheForSession's shape (PAH-04A).
    // RebuildForSession is called once per StartSession, before Ready, with this
    // session's resolved package ids (the same set schemas/Lua sources already use);
    // ReleaseForSession is called on EndSession and on a failed StartSession, so no
    // catalog survives past the session that owns it. GetSessionCatalog() never rebuilds
    // lazily -- it returns whatever RebuildForSession last published, or nullptr before
    // any session/after release, exactly like the presentation call sites already
    // null-check for today.
    static bool RebuildForSession(const TArray<FString>& PackageIds, FString& OutError);
    static void ReleaseForSession();
    static UGV2ImageResourceCatalog* GetSessionCatalog();

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FGV2ImageResourceLookupScaling;
    friend class FGV2GraphicsScalingPolicyTest;
#endif

    UPROPERTY(Transient)
    TArray<FGV2ImageResourceDefinition> Entries;

    UPROPERTY(Transient)
    TMap<FString, FGV2ResolvedImageResource> ResolvedById;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTexture2D>> RuntimeTextures;
};
