#pragma once

#include "GV2ContentCore/RepositorySnapshot.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2ScreenRegistry.h"
#include "UI/GV2UiSchemaCache.h"
#include "UI/GV2UiTheme.h"
#include "UObject/StrongObjectPtr.h"

#include <vector>

// TStrongObjectPtr<T> instantiates its constructor bodies (including the implicit default
// constructor of any struct embedding one) wherever FGV2SessionContentSnapshot's own
// constructor is instantiated -- e.g. GV2SessionCoordinator.cpp's MakeUnique<...>() -- so T
// must be a complete type here, not merely forward-declared.

// PSC-04 (ADR-0043 D1): the resolved Screen Registry this session's own candidate built and
// owns (TStrongObjectPtr) -- not a pointer borrowed from UGV2RuntimeSubsystem's own member
// or from UGV2ScreenRegistrySettings::GetConfiguredRegistry()'s independent access path.
struct FGV2ResolvedScreenRegistry
{
    TStrongObjectPtr<UGV2ScreenRegistry> Registry;

    bool Resolve(
        const FString& ScreenId,
        const FGV2ScreenPlacement& Placement,
        FGV2ResolvedScreenDescriptor& OutDescriptor,
        FGV2ScreenResolutionRejection& OutRejection) const;
};

struct FGV2ResolvedImageCatalog
{
    TStrongObjectPtr<UGV2ImageResourceCatalog> Catalog;

    bool Resolve(const FString& ResourceId, FGV2ResolvedImageResource& OutResource, FString& OutError) const;
};

struct FGV2ResolvedUiTheme
{
    TStrongObjectPtr<UGV2UiTheme> Theme;
};

// PSC-04 (ADR-0043 D1, BootstrapAndSessionLifecycle.md "Целевое правило"): one immutable,
// coordinator-owned aggregate of everything a session's presentation depends on -- built
// once, entirely, before the Lua VM exists, from the SAME FResolvedPackageSet the
// repository and Lua sources came from (FGV2SessionContentCandidate::Build below).
//
// PSC-04's own scope is that this object exists and is genuinely, independently resolved --
// not an aggregate of pointers into whichever owner already held these values. Production
// presentation code does not read this snapshot yet: PSC-05 publishes it atomically with
// Ready, PSC-06 wires FGV2PresentationPrepareContext to read it and retires the legacy
// accessors (UGV2RuntimeSubsystem's own ScreenRegistry member, the schema-cache/image-
// catalog session globals, UGV2UiThemeSettings::GetConfiguredTheme()) -- those remain
// untouched and keep serving every current production call site until then.
class FGV2SessionContentSnapshot
{
public:
    const GV2ContentCore::FRepositoryReadHandle& GetRepository() const { return Repository; }
    const TArray<FString>& GetOrderedPackageIds() const { return OrderedPackageIds; }
    const std::vector<GV2RuntimeCore::FRuntimeSource>& GetLuaSources() const { return LuaSources; }
    const FString& GetScriptSetHash() const { return ScriptSetHash; }
    const FGV2UiSchemaCache& GetSchemaCache() const
    {
        check(SchemaCache.IsValid());
        return *SchemaCache;
    }
    const FGV2ResolvedScreenRegistry& GetScreenRegistry() const { return ScreenRegistry; }
    const FGV2ResolvedImageCatalog& GetImageCatalog() const { return ImageCatalog; }
    const FGV2ResolvedUiTheme& GetTheme() const { return Theme; }
    UClass* GetGameShellClass() const { return GameShellClass.Get(); }

    const FString& GetRepositoryContentHash() const { return RepositoryContentHash; }
    const FString& GetPackageSetFingerprint() const { return PackageSetFingerprint; }
    const FString& GetPresentationHash() const { return PresentationHash; }
    const FString& GetSessionContentId() const { return SessionContentId; }

private:
    friend class FGV2SessionContentCandidate;

    GV2ContentCore::FRepositoryReadHandle Repository;
    TArray<FString> OrderedPackageIds;
    std::vector<GV2RuntimeCore::FRuntimeSource> LuaSources;
    FString ScriptSetHash;
    TSharedPtr<FGV2UiSchemaCache> SchemaCache;
    FGV2ResolvedScreenRegistry ScreenRegistry;
    FGV2ResolvedImageCatalog ImageCatalog;
    FGV2ResolvedUiTheme Theme;
    TStrongObjectPtr<UClass> GameShellClass;

    FString RepositoryContentHash;
    FString PackageSetFingerprint;
    FString PresentationHash;
    FString SessionContentId;
};

// PSC-04: the only place a FGV2SessionContentSnapshot is constructed. Two-phase (Build then
// FinalizeScriptIdentity) because script_set_hash is only known after
// FRuntimeSession::Start parses the module chain -- everything else is resolved and can
// fail closed before the Lua VM is created, matching BootstrapAndSessionLifecycle.md's
// "Ошибка построения любой его части... запрещает создание Lua VM".
class FGV2SessionContentCandidate
{
public:
    // Resolves everything except ScriptSetHash/SessionContentId. LuaSources is consumed
    // (moved) into OutSnapshot on success. OutFault names which stage failed
    // (UiSchemaNotReady/ScreenRegistryNotReady/ImageCatalogNotReady/ThemeNotReady) -- the
    // caller must not create the Lua VM when this returns false.
    static bool Build(
        const GV2ContentCore::FRepositoryReadHandle& PinnedRepository,
        const GV2ContentHostSupport::FResolvedPackageSet* ResolvedPackageSet,
        const TArray<FGV2SchemaPackageRoot>& SchemaPackageRoots,
        std::vector<GV2RuntimeCore::FRuntimeSource> LuaSources,
        FGV2SessionContentSnapshot& OutSnapshot,
        GV2RuntimeCore::FRuntimeFault& OutFault);

    // Called once, after FRuntimeSession::Start(...) succeeds using OutSnapshot's own
    // LuaSources -- folds in the post-VM script_set_hash and computes session_content_id,
    // the one hash spanning repository + package + script + presentation identity.
    static void FinalizeScriptIdentity(FGV2SessionContentSnapshot& Snapshot, const std::string& ScriptSetHash);
};
