#pragma once

#include "Engine/DataAsset.h"
#include "GV2ScreenRegistry.generated.h"

class UGV2ScreenWidgetBase;
class UGV2GameShellWidgetBase;

USTRUCT(BlueprintType)
struct GV2_API FGV2ScreenRegistryEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Screen Registry")
    FString ScreenId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Screen Registry")
    TSoftClassPtr<UGV2ScreenWidgetBase> WidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Screen Registry")
    FName Layer = TEXT("location_content");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Screen Registry")
    bool bSingleton = true;
};

UCLASS(BlueprintType)
class GV2_API UGV2ScreenRegistry : public UDataAsset
{
    GENERATED_BODY()

public:
    static const FName LayerEmbedded;

    static bool IsValidLayer(FName Layer);
    static bool IsLayerAllowedForTopLevel(FName Layer);
    static bool IsLayerAllowedForEmbedded(FName Layer);
    static bool IsAssetAllowedForScreenNamespace(const FString& ScreenNamespace, const FString& AssetPath);

    bool Validate(FString& OutError) const;

    const TArray<FGV2ScreenRegistryEntry>& GetEntries() const
    {
        return Entries;
    }

    const FGV2ScreenRegistryEntry* FindEntry(const FString& ScreenId) const;

private:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Screen Registry", meta = (AllowPrivateAccess = "true"))
    TArray<FGV2ScreenRegistryEntry> Entries;
};

UCLASS(Config = Game, DefaultConfig)
class GV2_API UGV2ScreenRegistrySettings : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, Category = "GV2|UI|Screen Registry")
    TSoftObjectPtr<UGV2ScreenRegistry> RegistryAsset;

    UPROPERTY(Config, EditAnywhere, Category = "GV2|UI|Screen Registry")
    TSoftClassPtr<UGV2GameShellWidgetBase> GameShellClass;
};
