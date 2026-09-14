#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "GV2PresentationApply/GV2ImageTypes.h"
#include "Styling/SlateBrush.h"
#include "GV2ImageResourceCatalog.generated.h"

UENUM(BlueprintType)
enum class EGV2ImageRenderMode : uint8
{
    FixedAspect,
    NineSlice,
    Tile
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

// PSC-07 (ADR-0043 D1, PAH-R5): one enabled package's own resource root -- the concrete
// filesystem directory BuildFromPackageResourceRoots recurses into for THIS package and
// no other. PackageId both seeds the produced resource_id's namespace and backs a
// secondary ownership check inside TryMakeResourceIdForPackage -- traversal being scoped
// to this directory already makes a foreign namespace physically unreachable, but the
// check is kept anyway, since a namespace filter is explicitly not allowed to be the
// ONLY defense (see BuildFromPackageResourceRoots's own doc comment).
struct GV2_API FGV2ImagePackageResourceRoot
{
    FString PackageId;
    FString ResourceRoot;
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

    // PSC-07 (ADR-0043 D1, PAH-R5): the real scoped-traversal build. Recurses into
    // exactly the directories listed in PackageResourceRoots -- one root per enabled
    // package -- and nothing else: a package whose root is absent from this list is
    // never listed, opened, or decoded, whether or not it exists on disk, and however
    // malformed its content is. This function itself has no notion of `Resources/` being
    // a canonical project convention -- it opens only what it is told to -- so a caller
    // (production or test) fully controls what gets scanned by controlling this list.
    bool BuildFromPackageResourceRoots(const TArray<FGV2ImagePackageResourceRoot>& PackageResourceRoots, FString& OutError);

    // PSC-07 (ADR-0043 D1, PAH-R5): production convenience wrapper -- derives one
    // FGV2ImagePackageResourceRoot per PackageId under the fixed `Resources/` project
    // convention (like `GameData`/`Scripts` -- not a configurable root) and delegates to
    // BuildFromPackageResourceRoots(). A package outside PackageIds -- this session's
    // resolved package closure -- never has its own Resources/<PackageId>/ directory
    // listed, opened, or decoded at all (replacing the former PAH-04B behaviour of
    // scanning the whole `Resources/` tree first and filtering the resulting entries by
    // namespace afterward, which is exactly the PAH-R5 finding this closes: a corrupt or
    // malformed file belonging to a disabled package could fail the whole build before
    // the closure filter ever ran). A resource whose package isn't in the closure is
    // absent from the snapshot the same way that package's schemas/Lua sources already
    // are -- not present, not a build error.
    bool BuildFromPackageClosure(const TArray<FString>& PackageIds, FString& OutError);

    static bool ValidateDefinition(const FGV2ImageResourceDefinition& Definition, FString& OutError);
    static bool ResolveDefinition(const FGV2ImageResourceDefinition& Definition, FGV2ResolvedImageResource& OutResource, FString& OutError);
    static bool TryMakeResourceId(
        const FString& RootDirectory,
        const FString& PngFilename,
        FString& OutResourceId,
        FString& OutError);

    // PSC-07: same grammar as TryMakeResourceId, but relative to one package's OWN
    // resource root (PackageResourceRoot = ".../Resources/<PackageId>") instead of the
    // shared `Resources/` parent -- so the path below it is "resource/<path>.png"
    // (namespace is not a path segment here: it is PackageId, known from which root is
    // being scanned, not parsed out of untrusted path text). Fails the same way
    // TryMakeResourceId does for a structurally malformed path, and additionally rejects
    // a produced id whose namespace segment doesn't match PackageId -- defense-in-depth,
    // not the primary protection (BuildFromPackageResourceRoots's own directory scoping
    // is what actually keeps a foreign package unreachable).
    static bool TryMakeResourceIdForPackage(
        const FString& PackageId,
        const FString& PackageResourceRoot,
        const FString& PngFilename,
        FString& OutResourceId,
        FString& OutError);

    // PSC-10C: the process-global session catalog (RebuildForSession/ReleaseForSession/
    // GetSessionCatalog, PAH-04B) is GONE. It was a second content authority for the same
    // session: FGV2SessionContentCandidate::Build already constructs this exact catalog --
    // the same BuildFromPackageClosure over the same closure package ids -- and pins it in
    // FGV2ResolvedImageCatalog, which every Prepare reads through
    // FGV2PresentationPrepareContext::ResolveResource. The global existed only so widget
    // code could resolve a resource without a snapshot, which is precisely what this task
    // removes; its build-failure semantics are unchanged because the candidate's own build
    // fails with the same ImageCatalogNotReady fault, earlier, on the same inputs.

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FGV2ImageResourceLookupScaling;
    friend class FGV2GraphicsScalingPolicyTest;
    friend class FGV2ImageResourceCatalogTestAccess;
#endif

    UPROPERTY(Transient)
    TArray<FGV2ImageResourceDefinition> Entries;

    UPROPERTY(Transient)
    TMap<FString, FGV2ResolvedImageResource> ResolvedById;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTexture2D>> RuntimeTextures;
};
