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

    // DCA-18: decision is a position comparison in PackageLoadOrder (index i holds the
    // package_id whose load_index is i, e.g. GetPackageLoadOrderFromGameData()'s result or
    // GameData/mods.lock.json5 read directly) -- not a hand-written per-namespace branch
    // ladder. A ScreenNamespace absent from PackageLoadOrder is rejected, never allowed by
    // default; an AssetPath whose content root isn't owned by any tracked package layer
    // (see FindOwningPackageForAssetPath) carries no layering constraint and is allowed.
    static bool IsAssetAllowedForScreenNamespace(
        const FString& ScreenNamespace,
        const FString& AssetPath,
        const TArray<FString>& PackageLoadOrder);

    // Resolves the package_id -> content-root naming convention (not itself an ordering
    // rule): returns the package_id that owns AssetPath's UE content root, or an empty
    // string if AssetPath's root isn't owned by any tracked package layer.
    static FString FindOwningPackageForAssetPath(const FString& AssetPath);

    // Reads GameData/mods.lock.json5 (via the same GV2ContentHostSupport package discovery
    // the runtime subsystem already uses to resolve its repository roots) and returns
    // package_id ordered by load_index; empty on discovery failure.
    static TArray<FString> GetPackageLoadOrderFromGameData();

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

    static const UGV2ScreenRegistry* GetConfiguredRegistry()
    {
        const UGV2ScreenRegistrySettings* Settings = GetDefault<UGV2ScreenRegistrySettings>();
        if (Settings != nullptr && !Settings->RegistryAsset.IsNull())
        {
            return Settings->RegistryAsset.LoadSynchronous();
        }
        return nullptr;
    }
};
