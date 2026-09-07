#include "UI/GV2ScreenRegistry.h"

#include "Application/GV2PackageClosure.h"
#include "Bridge/GV2StableIdUE.h"
#include "UI/GV2GameShellWidgetBase.h"
#include "UI/GV2ScreenWidgetBase.h"

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

bool UGV2ScreenRegistry::IsTrustedExternalContentDomain(const FString& AssetPath)
{
    return !AssetPath.StartsWith(TEXT("/game/"), ESearchCase::IgnoreCase);
}

TArray<FString> UGV2ScreenRegistry::GetPackageLoadOrderFromGameData()
{
    TArray<FString> PackageLoadOrder;
    for (const GV2PackageClosure::FEntry& Entry : GV2PackageClosure::DiscoverFromGameData())
    {
        PackageLoadOrder.Add(Entry.PackageId);
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
        // PAH-03: a /Game/ asset whose root isn't owned by any tracked package layer is
        // unowned, not unconstrained -- reject it. Content outside /Game/ entirely has no
        // project-package ownership to violate and is trusted by declared domain instead
        // (the old unconditional `return true` here covered both cases alike).
        return IsTrustedExternalContentDomain(AssetPath);
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

bool UGV2ScreenRegistry::Build(FString& OutError)
{
    ResolvedByScreenId.Reset();
    bBuilt = false;

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

    TMap<FString, FResolvedScreen> Built;
    for (const FGV2ScreenRegistryEntry& Entry : Entries)
    {
        if (!GV2StableIdUE::IsOfKind(Entry.ScreenId, "screen"))
        {
            OutError = FString::Printf(TEXT("Invalid screen_id: '%s'"), *Entry.ScreenId);
            return false;
        }

        if (Built.Contains(Entry.ScreenId))
        {
            OutError = FString::Printf(TEXT("Duplicate screen_id: '%s'"), *Entry.ScreenId);
            return false;
        }

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

        // PAH-02: class load/inheritance/abstractness check folded in here from the
        // runtime subsystem's own former LoadScreenRegistry() loop -- one builder, one
        // pass, instead of two separate validation loops over the same Entries.
        UClass* WidgetClass = Entry.WidgetClass.LoadSynchronous();
        if (WidgetClass == nullptr
            || !WidgetClass->IsChildOf(UGV2ScreenWidgetBase::StaticClass())
            || WidgetClass->HasAnyClassFlags(CLASS_Abstract))
        {
            OutError = FString::Printf(
                TEXT("Screen Registry class is missing, abstract, or has the wrong parent: screen_id='%s' class='%s'"),
                *Entry.ScreenId,
                *Entry.WidgetClass.ToSoftObjectPath().ToString());
            return false;
        }

        int32 ColonIdx = INDEX_NONE;
        if (Entry.ScreenId.FindChar(TEXT(':'), ColonIdx))
        {
            const FString Namespace = Entry.ScreenId.Left(ColonIdx);
            const FString AssetPath = Entry.WidgetClass.ToSoftObjectPath().ToString();
            if (!IsAssetAllowedForScreenNamespace(Namespace, AssetPath, PackageLoadOrder))
            {
                // PAH-03: two distinct rejection reasons share this branch -- tell them
                // apart for the diagnostic instead of reporting the layer-violation
                // wording for both. Cheap to re-derive: FindOwningPackageForAssetPath does
                // no I/O, and IsAssetAllowedForScreenNamespace already computed the same
                // value internally.
                const FString OwningPackage = FindOwningPackageForAssetPath(AssetPath);
                OutError = OwningPackage.IsEmpty()
                    ? FString::Printf(
                        TEXT("core:diagnostic.ui_screen_registry.unowned_asset_root: screen '%s' references asset '%s' whose content root is not owned by any package in the load closure"),
                        *Entry.ScreenId,
                        *AssetPath)
                    : FString::Printf(
                        TEXT("core:diagnostic.ui_screen_registry.higher_layer_asset: screen '%s' in namespace '%s' violates layer ownership by referencing higher layer asset '%s' (owned by '%s')"),
                        *Entry.ScreenId,
                        *Namespace,
                        *AssetPath,
                        *OwningPackage);
                return false;
            }
        }

        Built.Add(Entry.ScreenId, FResolvedScreen{WidgetClass, Entry.Layer});
    }

    ResolvedByScreenId = MoveTemp(Built);
    bBuilt = true;
    return true;
}

bool UGV2ScreenRegistry::Resolve(
    const FString& ScreenId,
    const FGV2ScreenPlacement& Placement,
    FGV2ResolvedScreenDescriptor& OutDescriptor,
    FGV2ScreenResolutionRejection& OutRejection) const
{
    const FResolvedScreen* Found = bBuilt ? ResolvedByScreenId.Find(ScreenId) : nullptr;
    if (Found == nullptr)
    {
        OutRejection.Code = EGV2ScreenResolutionError::UnknownScreenId;
        OutRejection.Message = FString::Printf(
            TEXT("core:diagnostic.ui_screen_registry.unknown_screen_id: '%s'"), *ScreenId);
        return false;
    }

    // PAH-02: IsLayerAllowedForEmbedded/IsLayerAllowedForTopLevel were declared, tested,
    // and never called by any production path -- this is that call. A screen registered
    // for one Placement is rejected when resolved for the other, and TopLevel additionally
    // requires the exact requested layer to match the registered one.
    const bool bPlacementMatches = Placement.IsEmbedded()
        ? IsLayerAllowedForEmbedded(Found->Layer)
        : IsLayerAllowedForTopLevel(Found->Layer) && Found->Layer == Placement.GetLayer();
    if (!bPlacementMatches)
    {
        OutRejection.Code = EGV2ScreenResolutionError::PlacementMismatch;
        OutRejection.Message = FString::Printf(
            TEXT("core:diagnostic.ui_screen_registry.placement_mismatch: screen '%s' is registered for layer '%s', requested as %s"),
            *ScreenId, *Found->Layer.ToString(), *Placement.ToString());
        return false;
    }

    OutDescriptor.ScreenId = ScreenId;
    OutDescriptor.WidgetClass = Found->WidgetClass;
    return true;
}
