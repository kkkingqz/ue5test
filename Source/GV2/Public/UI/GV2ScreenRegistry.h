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
};

// PAH-02 (ADR-0042, preamble): where a screen may be displayed. Embedded (inside a tab or
// other nested-screen host) carries no layer -- GameShell layers exist only for TopLevel
// placement, and GetLayer() is reachable only through that variant, never as a loose field
// present regardless of Kind. Constructed only via the two factories below.
class GV2_API FGV2ScreenPlacement
{
public:
    static FGV2ScreenPlacement Embedded() { return FGV2ScreenPlacement(EKind::Embedded, NAME_None); }
    static FGV2ScreenPlacement TopLevel(FName Layer) { return FGV2ScreenPlacement(EKind::TopLevel, Layer); }

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
    enum class EKind : uint8 { Embedded, TopLevel };
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
        const TArray<FString>& PackageLoadOrder);

    // Resolves the package_id -> content-root naming convention (not itself an ordering
    // rule): returns the package_id that owns AssetPath's UE content root, or an empty
    // string if AssetPath's root isn't owned by any tracked package layer. Only meaningful
    // for a /Game/ AssetPath -- see IsTrustedExternalContentDomain for anything else.
    static FString FindOwningPackageForAssetPath(const FString& AssetPath);

    // PAH-03: GV2's package closure and layering rule (ADR-0042, INV-P2) governs the
    // project's own content under /Game/ only -- mods.lock.json5 has no notion of engine
    // or plugin content roots for it to own. Content mounted outside /Game/ entirely
    // (engine-shipped, or an enabled plugin's own content root, e.g. CommonUI) therefore
    // carries no project-package ownership for the rule to check, and is trusted by
    // declared domain rather than by an ownership lookup that returns nothing. Without
    // this, rejecting an unowned /Game/ root by default would also reject any legitimate
    // reference to engine-shipped or plugin content.
    static bool IsTrustedExternalContentDomain(const FString& AssetPath);

    // Reads GameData/mods.lock.json5 (via the same GV2ContentHostSupport package discovery
    // the runtime subsystem already uses to resolve its repository roots) and returns
    // package_id ordered by load_index; empty on discovery failure.
    static TArray<FString> GetPackageLoadOrderFromGameData();

    // PAH-02: performs every authoring-time check exactly once -- screen_id format,
    // duplicates, WidgetClass load/inheritance/non-abstract, layer name validity, and
    // package ownership of the widget asset (folds in the former, production-dead
    // Validate()). Must succeed before Resolve() can return anything but
    // UnknownScreenId. Idempotent: safe to call again (e.g. before a fresh Resolve() in
    // a standalone test that only loaded the DataAsset), rebuilding from Entries each time.
    bool Build(FString& OutError);

    // PAH-02: the only way to get a screen's class. A screen registered for one Placement
    // is rejected, not silently handed out, when asked for a different one -- Placement
    // is not optional and there is no overload that omits it.
    bool Resolve(
        const FString& ScreenId,
        const FGV2ScreenPlacement& Placement,
        FGV2ResolvedScreenDescriptor& OutDescriptor,
        FGV2ScreenResolutionRejection& OutRejection) const;

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

    // Non-const: callers need to invoke the registry's own Build() (e.g. a standalone
    // test that loads the DataAsset directly, without going through the runtime
    // subsystem's own LoadScreenRegistry()) before Resolve() will return anything.
    static UGV2ScreenRegistry* GetConfiguredRegistry()
    {
        const UGV2ScreenRegistrySettings* Settings = GetDefault<UGV2ScreenRegistrySettings>();
        if (Settings != nullptr && !Settings->RegistryAsset.IsNull())
        {
            return Settings->RegistryAsset.LoadSynchronous();
        }
        return nullptr;
    }
};
