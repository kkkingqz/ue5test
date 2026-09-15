#include "UI/GV2ScreenRegistry.h"
#include "UI/GV2PresentationAuthorityProbe.h"

#include "Application/GV2PackageClosure.h"
#include "Bridge/GV2StableIdUE.h"
#include "Misc/Paths.h"
#include "UI/GV2GameShellWidgetBase.h"
#include "UI/GV2ScreenWidgetBase.h"

namespace
{
// PAH-05: lowercase, leading and trailing '/' -- the one place a declared
// "ue_content_roots" entry (or a legacy caller's raw AssetPath) is normalized before
// comparison.
FString NormalizeContentRoot(const FString& RawRoot)
{
    FString Root = RawRoot.ToLower();
    if (!Root.StartsWith(TEXT("/")))
    {
        Root = TEXT("/") + Root;
    }
    if (!Root.EndsWith(TEXT("/")))
    {
        Root += TEXT("/");
    }
    return Root;
}
}

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

FString UGV2ScreenRegistry::FindOwningPackageForAssetPath(
    const FString& AssetPath,
    const TArray<FGV2ContentRootOwnership>& Ownership)
{
    const FString LowerPath = AssetPath.ToLower();
    for (const FGV2ContentRootOwnership& Entry : Ownership)
    {
        if (LowerPath.StartsWith(Entry.NormalizedRoot))
        {
            return Entry.PackageId;
        }
    }
    return FString();
}

bool UGV2ScreenRegistry::BuildContentRootOwnership(
    const TArray<FGV2DeclaredPackageRoots>& PackageDeclaredRoots,
    TArray<FGV2ContentRootOwnership>& OutOwnership,
    FString& OutError)
{
    OutOwnership.Reset();
    for (const FGV2DeclaredPackageRoots& PackageRoots : PackageDeclaredRoots)
    {
        for (const FString& RawRoot : PackageRoots.Roots)
        {
            const FString Normalized = NormalizeContentRoot(RawRoot);
            for (const FGV2ContentRootOwnership& Existing : OutOwnership)
            {
                // PAH-05: overlap is rejected outright, in either direction -- never
                // resolved by preferring the more specific (longest-prefix) root.
                if (!Existing.PackageId.Equals(PackageRoots.PackageId, ESearchCase::IgnoreCase)
                    && (Normalized.StartsWith(Existing.NormalizedRoot) || Existing.NormalizedRoot.StartsWith(Normalized)))
                {
                    OutError = FString::Printf(
                        TEXT("content root '%s' (package '%s') overlaps '%s' (package '%s')"),
                        *RawRoot,
                        *PackageRoots.PackageId,
                        *Existing.NormalizedRoot,
                        *Existing.PackageId);
                    OutOwnership.Reset();
                    return false;
                }
            }
            OutOwnership.Add(FGV2ContentRootOwnership{Normalized, PackageRoots.PackageId});
        }
    }
    return true;
}

// PSC-02 (ADR-0043 D1/D5), SAC-02: ClosureEntries is supplied by the caller's ALREADY-resolved
// package set (from GV2ContentHostSupport::FResolvedPackageSet) and carries each package's
// declared UeContentRoots in memory -- no disk access or manifest re-reading occurs here.
// PAH-04: pre_ready_discovery callers=UGV2ScreenRegistry::CompileResolvedRegistry
// Called from CompileResolvedRegistry during FGV2SessionContentCandidate::Build.
// Under ADR-0044, candidate preparation executes while a prior Ready session may remain active;
// resolving content root ownership is candidate-scoped before commit-to-replace.
bool UGV2ScreenRegistry::ResolveContentRootOwnershipFromGameData(
    const TArray<GV2PackageClosure::FEntry>& ClosureEntries,
    TArray<FGV2ContentRootOwnership>& OutOwnership,
    FString& OutError)
{
    OutOwnership.Reset();
    TArray<FGV2DeclaredPackageRoots> PackageDeclaredRoots;
    PackageDeclaredRoots.Reserve(ClosureEntries.Num());
    for (const GV2PackageClosure::FEntry& PackageEntry : ClosureEntries)
    {
        PackageDeclaredRoots.Add(FGV2DeclaredPackageRoots{PackageEntry.PackageId, PackageEntry.UeContentRoots});
    }
    return BuildContentRootOwnership(PackageDeclaredRoots, OutOwnership, OutError);
}

bool UGV2ScreenRegistry::IsTrustedExternalContentDomain(const FString& AssetPath)
{
    return !AssetPath.StartsWith(TEXT("/game/"), ESearchCase::IgnoreCase);
}

// PSC-02: pure projection of the caller's already-resolved closure order -- see
// ResolveContentRootOwnershipFromGameData's comment above for why Build() no longer
// discovers the package set itself.
TArray<FString> UGV2ScreenRegistry::GetPackageLoadOrderFromGameData(const TArray<GV2PackageClosure::FEntry>& ClosureEntries)
{
    TArray<FString> PackageLoadOrder;
    PackageLoadOrder.Reserve(ClosureEntries.Num());
    for (const GV2PackageClosure::FEntry& Entry : ClosureEntries)
    {
        PackageLoadOrder.Add(Entry.PackageId);
    }
    return PackageLoadOrder;
}

// PAH-08: phase=prepare -- called only from Build(), below, which compiles the
// authoring registry once before any session presents anything.
bool UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(
    const FString& ScreenNamespace,
    const FString& AssetPath,
    const TArray<FString>& PackageLoadOrder,
    const TArray<FGV2ContentRootOwnership>& Ownership)
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

    const FString OwningPackage = FindOwningPackageForAssetPath(AssetPath, Ownership);
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

// PAH-08: phase=prepare -- compiles the authoring registry once, into an isolated
// FGV2ResolvedScreenRegistry value; package order and content-root ownership are validated,
// and the authoring DataAsset is not mutated.
bool UGV2ScreenRegistry::CompileResolvedRegistry(
    const TArray<GV2PackageClosure::FEntry>& ClosureEntries,
    FGV2ResolvedScreenRegistry& OutRegistry,
    FString& OutError) const
{
    OutError.Reset();

    if (Entries.IsEmpty())
    {
        OutError = TEXT("Screen Registry contains no entries");
        return false;
    }

    const TArray<FString> PackageLoadOrder = GetPackageLoadOrderFromGameData(ClosureEntries);
    if (PackageLoadOrder.IsEmpty())
    {
        OutError = TEXT("Screen Registry could not resolve the package load order from the session's resolved package set");
        return false;
    }

    TArray<FGV2ContentRootOwnership> Ownership;
    FString OwnershipError;
    if (!ResolveContentRootOwnershipFromGameData(ClosureEntries, Ownership, OwnershipError))
    {
        OutError = FString::Printf(
            TEXT("core:diagnostic.ui_screen_registry.content_root_ownership_conflict: %s"),
            *OwnershipError);
        return false;
    }

    TMap<FString, FGV2ResolvedScreenRegistry::FResolvedScreenRow> LocalRows;
    for (const FGV2ScreenRegistryEntry& Entry : Entries)
    {
        if (!GV2StableIdUE::IsOfKind(Entry.ScreenId, "screen"))
        {
            OutError = FString::Printf(TEXT("Invalid screen_id: '%s'"), *Entry.ScreenId);
            return false;
        }

        if (LocalRows.Contains(Entry.ScreenId))
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
            if (!IsAssetAllowedForScreenNamespace(Namespace, AssetPath, PackageLoadOrder, Ownership))
            {
                // PAH-03: two distinct rejection reasons share this branch -- tell them
                // apart for the diagnostic instead of reporting the layer-violation
                // wording for both. Cheap to re-derive: FindOwningPackageForAssetPath does
                // no I/O, and IsAssetAllowedForScreenNamespace already computed the same
                // value internally.
                const FString OwningPackage = FindOwningPackageForAssetPath(AssetPath, Ownership);
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

        LocalRows.Add(Entry.ScreenId, FGV2ResolvedScreenRegistry::FResolvedScreenRow{
            TStrongObjectPtr<UClass>(WidgetClass),
            Entry.Layer
        });
    }

    OutRegistry.Rows = MoveTemp(LocalRows);
    return true;
}

// PAH-08: phase=authority
bool FGV2ResolvedScreenRegistry::Resolve(
    const FString& ScreenId,
    const FGV2ScreenPlacement& Placement,
    FGV2ResolvedScreenDescriptor& OutDescriptor,
    FGV2ScreenResolutionRejection& OutRejection) const
{
    GV2_NOTE_AUTHORITY_RESOLVE();
    const FResolvedScreenRow* Found = Rows.Find(ScreenId);
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
    // PSC-08: exhaustive switch over FGV2ScreenPlacement::EKind, not an IsEmbedded()
    // ternary -- a third Kind with no case here trips the `default:` guard instead of
    // silently taking whichever branch a boolean happened to select.
    bool bPlacementMatches = false;
    switch (Placement.GetKind())
    {
    case FGV2ScreenPlacement::EKind::Embedded:
        bPlacementMatches = UGV2ScreenRegistry::IsLayerAllowedForEmbedded(Found->Layer);
        break;
    case FGV2ScreenPlacement::EKind::TopLevel:
        bPlacementMatches = UGV2ScreenRegistry::IsLayerAllowedForTopLevel(Found->Layer) && Found->Layer == Placement.GetLayer();
        break;
    default:
        checkf(false, TEXT("Unhandled FGV2ScreenPlacement::EKind value"));
        bPlacementMatches = false;
        break;
    }
    if (!bPlacementMatches)
    {
        OutRejection.Code = EGV2ScreenResolutionError::PlacementMismatch;
        OutRejection.Message = FString::Printf(
            TEXT("core:diagnostic.ui_screen_registry.placement_mismatch: screen '%s' is registered for layer '%s', requested as %s"),
            *ScreenId, *Found->Layer.ToString(), *Placement.ToString());
        return false;
    }

    OutDescriptor.ScreenId = ScreenId;
    OutDescriptor.WidgetClass = Found->WidgetClass.Get();
    return true;
}

TArray<TPair<FString, FString>> FGV2ResolvedScreenRegistry::GetResolvedScreenIdentities() const
{
    TArray<TPair<FString, FString>> Identities;
    Identities.Reserve(Rows.Num());
    for (const auto& [ScreenId, Row] : Rows)
    {
        const FString ClassPath = Row.WidgetClass.IsValid() ? Row.WidgetClass->GetPathName() : FString();
        Identities.Emplace(ScreenId, ClassPath);
    }
    Identities.Sort([](const TPair<FString, FString>& A, const TPair<FString, FString>& B)
    {
        return A.Key < B.Key;
    });
    return Identities;
}
