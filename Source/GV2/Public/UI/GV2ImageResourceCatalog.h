#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
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

    static bool ValidateDefinition(const FGV2ImageResourceDefinition& Definition, FString& OutError);
    static bool ResolveDefinition(const FGV2ImageResourceDefinition& Definition, FGV2ResolvedImageResource& OutResource, FString& OutError);
    static bool TryMakeResourceId(
        const FString& RootDirectory,
        const FString& PngFilename,
        FString& OutResourceId,
        FString& OutError);

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FGV2ImageResourceLookupScaling;
#endif

    UPROPERTY(Transient)
    TArray<FGV2ImageResourceDefinition> Entries;

    UPROPERTY(Transient)
    TMap<FString, FGV2ResolvedImageResource> ResolvedById;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTexture2D>> RuntimeTextures;
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "GV2 Image Resource Catalog"))
class GV2_API UGV2ImageResourceCatalogSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    virtual FName GetCategoryName() const override;

    static UGV2ImageResourceCatalog* GetConfiguredCatalog();
    static bool RebuildConfiguredCatalog(FString& OutError);

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Catalog")
    FString ResourceRootDirectory = TEXT("Resources");
};
