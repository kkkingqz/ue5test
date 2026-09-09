#pragma once

#include "Engine/DataAsset.h"
#include "GV2ScreenRegistry.generated.h"

namespace GV2PackageClosure { struct FEntry; }

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
};

// PAH-05 (ADR-0042, INV-P2): one package's declared ownership of one UE content root.
// NormalizedRoot is lowercase with a leading and trailing '/' (e.g. "/game/core/"),
// produced only by UGV2ScreenRegistry::ResolveContentRootOwnershipFromGameData -- never
// hand-built, so normalization stays in exactly one place.
struct GV2_API FGV2ContentRootOwnership
{
    FString NormalizedRoot;
    FString PackageId;
};

// PAH-05: one package's raw, not-yet-normalized "ue_content_roots" declaration --
// BuildContentRootOwnership's input shape, kept separate from FGV2ContentRootOwnership
// (which is already normalized and one-root-per-entry) so the pure validation function
// below needs no filesystem access to test.
struct GV2_API FGV2DeclaredPackageRoots
{
    FString PackageId;
    TArray<FString> Roots;
};

// PAH-02 (ADR-0042, preamble): where a screen may be displayed. Embedded (inside a tab or
// other nested-screen host) carries no layer -- GameShell layers exist only for TopLevel
// placement, and GetLayer() is reachable only through that variant, never as a loose field
// present regardless of Kind. Constructed only via the two factories below.
class GV2_API FGV2ScreenPlacement
{
public:
    // PSC-08: public so UGV2ScreenRegistry::Resolve() can switch over it exhaustively
    // instead of chaining IsEmbedded()/IsTopLevel() booleans -- a third Kind (and its own
    // factory) added here without a matching case in that switch trips its `default:`
    // guard the first time anything resolves a placement of that Kind, rather than being
    // silently folded into whichever boolean branch happens to run.
    enum class EKind : uint8 { Embedded, TopLevel };

    static FGV2ScreenPlacement Embedded() { return FGV2ScreenPlacement(EKind::Embedded, NAME_None); }
    static FGV2ScreenPlacement TopLevel(FName Layer) { return FGV2ScreenPlacement(EKind::TopLevel, Layer); }

    EKind GetKind() const { return Kind; }
    bool IsEmbedded() const { return Kind == EKind::Embedded; }
    bool IsTopLevel() const { return Kind == EKind::TopLevel; }

    // Only valid to call when IsTopLevel(); asserts otherwise. Embedded truly carries no
    // layer -- there is no default/sentinel value a caller could read by mistake.
    FName GetLayer() const
    {
        check(IsTopLevel());
        return Layer;
    }

    FString ToString() const
    {
        return IsEmbedded() ? TEXT("Embedded") : FString::Printf(TEXT("TopLevel(%s)"), *Layer.ToString());
    }

private:
    FGV2ScreenPlacement(EKind InKind, FName InLayer) : Kind(InKind), Layer(InLayer) {}

    EKind Kind;
    FName Layer;
};

// PAH-02: the only thing a resolved screen exposes -- never the authored
// FGV2ScreenRegistryEntry itself, which stays internal to the registry.
struct GV2_API FGV2ResolvedScreenDescriptor
{
    FString ScreenId;
    UClass* WidgetClass = nullptr;
};

enum class EGV2ScreenResolutionError : uint8
{
    UnknownScreenId,
    PlacementMismatch,
};

struct GV2_API FGV2ScreenResolutionRejection
{
    EGV2ScreenResolutionError Code = EGV2ScreenResolutionError::UnknownScreenId;
    FString Message;
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
    // default. PAH-03: an AssetPath under /Game/ whose content root isn't owned by any
    // tracked package layer (see FindOwningPackageForAssetPath) is unowned and rejected,
    // not unconstrained; an AssetPath outside /Game/ entirely (see
    // IsTrustedExternalContentDomain) is trusted by declared domain instead.
    static bool IsAssetAllowedForScreenNamespace(
        const FString& ScreenNamespace,
        const FString& AssetPath,
        const TArray<FString>& PackageLoadOrder,
        const TArray<FGV2ContentRootOwnership>& Ownership);

    // Resolves AssetPath's UE content root against Ownership (PAH-05: data, not a
    // hardcoded table): returns the package_id whose declared root AssetPath falls under,
    // or an empty string if no entry's root matches. Only meaningful for a /Game/
    // AssetPath -- see IsTrustedExternalContentDomain for anything else.
    static FString FindOwningPackageForAssetPath(
        const FString& AssetPath,
        const TArray<FGV2ContentRootOwnership>& Ownership);

    // PAH-03: GV2's package closure and layering rule (ADR-0042, INV-P2) governs the
    // project's own content under /Game/ only -- mods.lock.json5 has no notion of engine
    // or plugin content roots for it to own. Content mounted outside /Game/ entirely
    // (engine-shipped, or an enabled plugin's own content root, e.g. CommonUI) therefore
    // carries no project-package ownership for the rule to check, and is trusted by
    // declared domain rather than by an ownership lookup that returns nothing. Without
    // this, rejecting an unowned /Game/ root by default would also reject any legitimate
    // reference to engine-shipped or plugin content.
    static bool IsTrustedExternalContentDomain(const FString& AssetPath);

    // PSC-02 (ADR-0043 D1/D5): pure projection of ClosureEntries (the caller's single
    // already-resolved package set) into package_id ordered by load_index -- no
    // discovery of its own. ClosureEntries empty means the caller couldn't resolve a
    // package set at all, not "scan GameData/ instead".
    static TArray<FString> GetPackageLoadOrderFromGameData(const TArray<GV2PackageClosure::FEntry>& ClosureEntries);

    // PAH-05 (ADR-0042, INV-P2): reads every closure package's own GameData/<id>/
    // package.json5 "ue_content_roots" array -- a UE-only field the portable host never
    // parses into FPackageDescriptor, so it can't affect ComputePackageFingerprint (0F).
    // Absence of the field means the package declares zero UE content roots (valid, e.g.
    // a package with no widget/resource assets of its own). Every declared root is
    // normalized (FGV2ContentRootOwnership::NormalizedRoot) before comparison. Two
    // packages whose roots are equal, or where one is a prefix of the other, is a build
    // error -- overlap is rejected outright, never resolved by a longest-prefix-wins rule.
    // Delegates the actual validation to BuildContentRootOwnership below (no filesystem
    // access in that function -- only here, gathering PackageDeclaredRoots to feed it).
    // PSC-02: ClosureEntries is the caller's single already-resolved package set -- this
    // function no longer discovers it itself, only reads each entry's own
    // "ue_content_roots" field (not package-set discovery -- see .cpp comment).
    static bool ResolveContentRootOwnershipFromGameData(
        const TArray<GV2PackageClosure::FEntry>& ClosureEntries,
        TArray<FGV2ContentRootOwnership>& OutOwnership,
        FString& OutError);

    // PAH-05: pure -- no filesystem access, so a test can exercise the overlap rule (and
    // a synthetic fourth package's own root) directly, without a real GameData/ package
    // set. Normalizes every declared root and validates no two different packages' roots
    // are equal or one a prefix of the other; on any overlap, OutOwnership is cleared and
    // OutError names both conflicting roots and packages.
    static bool BuildContentRootOwnership(
        const TArray<FGV2DeclaredPackageRoots>& PackageDeclaredRoots,
        TArray<FGV2ContentRootOwnership>& OutOwnership,
        FString& OutError);

    // PAH-02: performs every authoring-time check exactly once -- screen_id format,
    // duplicates, WidgetClass load/inheritance/non-abstract, layer name validity, and
    // package ownership of the widget asset (folds in the former, production-dead
    // Validate()). Must succeed before Resolve() can return anything but
    // UnknownScreenId. Idempotent: safe to call again (e.g. before a fresh Resolve() in
    // a standalone test that only loaded the DataAsset), rebuilding from Entries each time.
    // PSC-02 (ADR-0043 D1/D5): ClosureEntries is the caller's single already-resolved
    // package set -- Build() performs no package discovery of its own.
    bool Build(const TArray<GV2PackageClosure::FEntry>& ClosureEntries, FString& OutError);

    // PAH-02: the only way to get a screen's class. A screen registered for one Placement
    // is rejected, not silently handed out, when asked for a different one -- Placement
    // is not optional and there is no overload that omits it.
    bool Resolve(
        const FString& ScreenId,
        const FGV2ScreenPlacement& Placement,
        FGV2ResolvedScreenDescriptor& OutDescriptor,
        FGV2ScreenResolutionRejection& OutRejection) const;

    // PSC-04 (ADR-0043 D1): every resolved screen's own identity (screen_id, its already-
    // loaded WidgetClass's path) for presentation_hash -- not FGV2ScreenRegistryEntry (the
    // raw, unvalidated authoring row validate_screen_registry_entry_encapsulation.py keeps
    // out of production code), and not usable to obtain a class outside Resolve()'s checks.
    // Deterministic (sorted by screen_id) so the hash doesn't depend on TMap iteration order.
    TArray<TPair<FString, FString>> GetResolvedScreenIdentities() const;

private:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GV2|UI|Screen Registry", meta = (AllowPrivateAccess = "true"))
    TArray<FGV2ScreenRegistryEntry> Entries;

    struct FResolvedScreen
    {
        UClass* WidgetClass = nullptr;
        FName Layer;
    };
    TMap<FString, FResolvedScreen> ResolvedByScreenId;
    bool bBuilt = false;
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
