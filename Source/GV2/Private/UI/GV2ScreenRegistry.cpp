#include "UI/GV2ScreenRegistry.h"
#include "UI/GV2PresentationAuthorityProbe.h"

#include "Application/GV2PackageClosure.h"
#include "Bridge/GV2StableIdUE.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"
#include "Misc/FileHelper.h"
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

// PAH-05: reads one package's own GameData/<id>/package.json5 "ue_content_roots" array.
// UE-only field -- GV2ContentHostSupport::DiscoverPackageFromDirectory (the portable
// parser) never looks for it, so it can't reach FPackageDescriptor or
// ComputePackageFingerprint (0F). Absence of the field is valid: zero declared roots,
// not an error -- a package can own no UE-side widget/resource content at all (e.g.
// "sample" today).
// PAH-04: pre_ready_discovery -- only called from ResolveContentRootOwnershipFromGameData(),
// only called from Build(), only called from LoadScreenRegistry(), only called from
// Initialize(), before any session exists.
bool ReadUeContentRootsForPackage(const FString& PackageGameDataDir, TArray<FString>& OutRoots, FString& OutError)
{
    OutRoots.Reset();
    const FString ManifestPath = FPaths::Combine(PackageGameDataDir, TEXT("package.json5"));
    FString Content;
    if (!FFileHelper::LoadFileToString(Content, *ManifestPath))
    {
        OutError = FString::Printf(TEXT("could not read '%s'"), *ManifestPath);
        return false;
    }

    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    const std::optional<GV2ContentCore::FParsedDocument> Parsed = GV2ContentCore::ParseJson5Document(
        TCHAR_TO_UTF8(*Content), GV2ContentCore::FParseLimits{}, Diagnostics);
    if (!Parsed.has_value() || !Diagnostics.empty() || !Parsed->GetRootValue().IsObject())
    {
        OutError = FString::Printf(TEXT("'%s' could not be parsed as a JSON5 object"), *ManifestPath);
        return false;
    }

    const GV2ContentCore::FValue* RootsField = Parsed->GetRootValue().FindField("ue_content_roots");
    if (RootsField == nullptr)
    {
        return true;
    }
    if (!RootsField->IsArray())
    {
        OutError = FString::Printf(TEXT("'%s': 'ue_content_roots' must be an array of strings"), *ManifestPath);
        return false;
    }
    for (const GV2ContentCore::FValue& Item : RootsField->AsArray())
    {
        if (!Item.IsString())
        {
            OutError = FString::Printf(TEXT("'%s': 'ue_content_roots' entries must be strings"), *ManifestPath);
            return false;
        }
        OutRoots.Add(UTF8_TO_TCHAR(Item.AsString().c_str()));
    }
    return true;
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

// PSC-02 (ADR-0043 D1/D5): ClosureEntries is supplied by the caller's ALREADY-resolved
// package set (UGV2RuntimeSubsystem::Initialize, from GV2ContentHostSupport::
// ResolvePackageSet{FromContainer,FromDirectories}) -- this function no longer discovers
// the package set itself (PAH-R3: a second, independent discovery of the same closure is
// a second authority, even when it returns the same order today). It still reads each
// entry's own "ue_content_roots" field directly, which is not package-set discovery: the
// portable descriptor parser deliberately never looks at that field (0F), so it cannot be
// obtained any other way once the package set is already resolved.
// PAH-04: pre_ready_discovery -- only called from Build(), only called from
// LoadScreenRegistry(), only called from Initialize(), before any session exists.
bool UGV2ScreenRegistry::ResolveContentRootOwnershipFromGameData(
    const TArray<GV2PackageClosure::FEntry>& ClosureEntries,
    TArray<FGV2ContentRootOwnership>& OutOwnership,
    FString& OutError)
{
    OutOwnership.Reset();
    TArray<FGV2DeclaredPackageRoots> PackageDeclaredRoots;
    for (const GV2PackageClosure::FEntry& PackageEntry : ClosureEntries)
    {
        TArray<FString> DeclaredRoots;
        FString ReadError;
        if (!ReadUeContentRootsForPackage(PackageEntry.RootDirectory, DeclaredRoots, ReadError))
        {
            OutError = FString::Printf(TEXT("package '%s': %s"), *PackageEntry.PackageId, *ReadError);
            return false;
        }
        PackageDeclaredRoots.Add(FGV2DeclaredPackageRoots{PackageEntry.PackageId, MoveTemp(DeclaredRoots)});
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

// PAH-08: phase=prepare -- compiles the authoring registry once, before a
// session presents anything; this is where package order and content-root
// ownership are read, and the only place they are.
// PSC-02 (ADR-0043 D1/D5): ClosureEntries comes from the caller's single resolved package
// set (UGV2RuntimeSubsystem::Initialize) -- Build() itself performs no package discovery.
bool UGV2ScreenRegistry::Build(const TArray<GV2PackageClosure::FEntry>& ClosureEntries, FString& OutError)
{
    ResolvedByScreenId.Reset();
    bBuilt = false;

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

        Built.Add(Entry.ScreenId, FResolvedScreen{WidgetClass, Entry.Layer});
    }

    ResolvedByScreenId = MoveTemp(Built);
    bBuilt = true;
    return true;
}

// PAH-08: phase=authority
bool UGV2ScreenRegistry::Resolve(
    const FString& ScreenId,
    const FGV2ScreenPlacement& Placement,
    FGV2ResolvedScreenDescriptor& OutDescriptor,
    FGV2ScreenResolutionRejection& OutRejection) const
{
    GV2_NOTE_AUTHORITY_RESOLVE();
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
