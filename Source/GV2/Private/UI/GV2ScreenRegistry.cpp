#include "UI/GV2ScreenRegistry.h"

#include "Bridge/GV2StableIdUE.h"
#include "Misc/Paths.h"
#include "UI/GV2GameShellWidgetBase.h"
#include "UI/GV2ScreenWidgetBase.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

#include <filesystem>
#include <optional>
#include <vector>

const FName UGV2ScreenRegistry::LayerEmbedded = TEXT("embedded");

bool UGV2ScreenRegistry::IsValidLayer(FName Layer)
{
    return Layer == LayerEmbedded || UGV2GameShellWidgetBase::IsValidLayerName(Layer);
}

bool UGV2ScreenRegistry::IsLayerAllowedForTopLevel(FName Layer)
{
    return Layer != LayerEmbedded && UGV2GameShellWidgetBase::IsValidLayerName(Layer);
}

bool UGV2ScreenRegistry::IsLayerAllowedForEmbedded(FName Layer)
{
    return Layer == LayerEmbedded;
}

FString UGV2ScreenRegistry::FindOwningPackageForAssetPath(const FString& AssetPath)
{
    // Naming convention between a package_id and the UE content root its UI assets live
    // under (not itself an ordering rule -- mods.lock.json5 has no notion of /Game/ paths).
    static const TPair<const TCHAR*, const TCHAR*> ContentRootOwners[] = {
        {TEXT("/game/core/"), TEXT("core")},
        {TEXT("/game/ui/"), TEXT("core")},
        {TEXT("/game/textsystem/"), TEXT("textsystem")},
        {TEXT("/game/rh/"), TEXT("rh")},
    };
    const FString LowerPath = AssetPath.ToLower();
    for (const TPair<const TCHAR*, const TCHAR*>& Owner : ContentRootOwners)
    {
        if (LowerPath.StartsWith(Owner.Key))
        {
            return Owner.Value;
        }
    }
    return FString();
}

TArray<FString> UGV2ScreenRegistry::GetPackageLoadOrderFromGameData()
{
    const FString GameDataDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData"));
    const std::string GameDataDirUtf8 = TCHAR_TO_UTF8(*GameDataDir);
    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    const std::optional<std::vector<GV2ContentCore::FPackageDescriptor>> Descriptors =
        GV2ContentHostSupport::DiscoverPackagesFromContainer(
            std::filesystem::path(GameDataDirUtf8),
            Diagnostics);
    if (!Descriptors.has_value())
    {
        return {};
    }

    TArray<FString> PackageLoadOrder;
    PackageLoadOrder.SetNum(Descriptors->size());
    for (const GV2ContentCore::FPackageDescriptor& Descriptor : *Descriptors)
    {
        const int32 LoadIndex = static_cast<int32>(Descriptor.GetLoadIndex());
        if (PackageLoadOrder.IsValidIndex(LoadIndex))
        {
            PackageLoadOrder[LoadIndex] = UTF8_TO_TCHAR(Descriptor.GetPackageId().c_str());
        }
    }
    return PackageLoadOrder;
}

bool UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(
    const FString& ScreenNamespace,
    const FString& AssetPath,
    const TArray<FString>& PackageLoadOrder)
{
    const FString LowerNamespace = ScreenNamespace.ToLower();
    const int32 ScreenPackageIndex = PackageLoadOrder.IndexOfByPredicate(
        [&LowerNamespace](const FString& PackageId)
        {
            return PackageId.ToLower() == LowerNamespace;
        });
    if (ScreenPackageIndex == INDEX_NONE)
    {
        // A namespace absent from the pinned closure is rejected, not allowed by default.
        return false;
    }

    const FString OwningPackage = FindOwningPackageForAssetPath(AssetPath);
    if (OwningPackage.IsEmpty())
    {
        // The asset's content root isn't owned by any tracked package layer, so no
        // layer-ownership constraint applies to it.
        return true;
    }

    const int32 AssetPackageIndex = PackageLoadOrder.IndexOfByPredicate(
        [&OwningPackage](const FString& PackageId)
        {
            return PackageId.Equals(OwningPackage, ESearchCase::IgnoreCase);
        });
    if (AssetPackageIndex == INDEX_NONE)
    {
        return true;
    }

    // A screen may reference its own layer or a lower one, never a higher one.
    return AssetPackageIndex <= ScreenPackageIndex;
}

const FGV2ScreenRegistryEntry* UGV2ScreenRegistry::FindEntry(const FString& ScreenId) const
{
    return Entries.FindByPredicate([&ScreenId](const FGV2ScreenRegistryEntry& Entry)
    {
        return Entry.ScreenId == ScreenId;
    });
}

bool UGV2ScreenRegistry::Validate(FString& OutError) const
{
    if (Entries.IsEmpty())
    {
        OutError = TEXT("Screen Registry contains no entries");
        return false;
    }

    const TArray<FString> PackageLoadOrder = GetPackageLoadOrderFromGameData();
    if (PackageLoadOrder.IsEmpty())
    {
        OutError = TEXT("Screen Registry could not resolve the package load order from GameData/mods.lock.json5");
        return false;
    }

    TSet<FString> SeenScreenIds;
    for (const FGV2ScreenRegistryEntry& Entry : Entries)
    {
        if (!GV2StableIdUE::IsOfKind(Entry.ScreenId, "screen"))
        {
            OutError = FString::Printf(TEXT("Invalid screen_id: '%s'"), *Entry.ScreenId);
            return false;
        }

        if (SeenScreenIds.Contains(Entry.ScreenId))
        {
            OutError = FString::Printf(TEXT("Duplicate screen_id: '%s'"), *Entry.ScreenId);
            return false;
        }
        SeenScreenIds.Add(Entry.ScreenId);

        if (!IsValidLayer(Entry.Layer))
        {
            OutError = FString::Printf(TEXT("Unknown layer '%s' for screen_id '%s'"), *Entry.Layer.ToString(), *Entry.ScreenId);
            return false;
        }

        if (Entry.WidgetClass.IsNull())
        {
            OutError = FString::Printf(TEXT("Null WidgetClass for screen_id '%s'"), *Entry.ScreenId);
            return false;
        }

        int32 ColonIdx = INDEX_NONE;
        if (Entry.ScreenId.FindChar(TEXT(':'), ColonIdx))
        {
            const FString Namespace = Entry.ScreenId.Left(ColonIdx);
            const FString AssetPath = Entry.WidgetClass.ToSoftObjectPath().ToString();
            if (!IsAssetAllowedForScreenNamespace(Namespace, AssetPath, PackageLoadOrder))
            {
                OutError = FString::Printf(
                    TEXT("Screen '%s' in namespace '%s' violates layer ownership by referencing higher layer asset '%s'"),
                    *Entry.ScreenId,
                    *Namespace,
                    *AssetPath);
                return false;
            }
        }
    }

    return true;
}
