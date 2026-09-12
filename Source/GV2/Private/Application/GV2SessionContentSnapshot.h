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

struct FGV2ResolvedImageCatalog
{
    TStrongObjectPtr<UGV2ImageResourceCatalog> Catalog;

    bool Resolve(const FString& ResourceId, FGV2ResolvedImageResource& OutResource, FString& OutError) const;
    TArray<FString> GetResourceIds() const;
};

struct FGV2ResolvedUiTheme
{
    TStrongObjectPtr<UGV2UiTheme> Theme;

    // PSC-10B: the UE-native core-minimal Theme, pinned here at snapshot build so a text id
    // the authored Theme does not carry still resolves during Prepare WITHOUT any runtime
    // code reaching UGV2UiTheme::GetCoreMinimalTheme() itself. Before this task the fallback
    // lived inside UGV2TextPipeline::Resolve as a process-global lookup on the Commit-facing
    // side; the values are the same, the reach is not.
    TStrongObjectPtr<UGV2UiTheme> FallbackTheme;

    // PSC-10B (ADR-0043 D1): the hover popover renderer class, loaded ONCE here. Prepare and
    // the hover path both read this already-loaded class, so neither performs a synchronous
    // load -- previously GV2CentralStylePreparer called LoadSynchronous() for every rich text
    // widget on every reconcile, and the hover tooltip called it again per popover.
    TStrongObjectPtr<UClass> RichTextPopoverClass;
};

// PSC-04 (ADR-0043 D1, BootstrapAndSessionLifecycle.md "Целевое правило"): one immutable,
// coordinator-owned aggregate of everything a session's presentation depends on -- built
// once, entirely, before the Lua VM exists, from the SAME FResolvedPackageSet the
// repository and Lua sources came from (FGV2SessionContentCandidate::Build below).
//
// It is genuinely, independently resolved rather than borrowing process-global owners.
// Ready publishes it atomically; every semantic presentation Prepare reads through the
// context below.
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
        const GV2ContentHostSupport::FResolvedPackageSet& ResolvedPackageSet,
        const TArray<FGV2SchemaPackageRoot>& SchemaPackageRoots,
        std::vector<GV2RuntimeCore::FRuntimeSource> LuaSources,
        FGV2SessionContentSnapshot& OutSnapshot,
        GV2RuntimeCore::FRuntimeFault& OutFault);

    // Called once, after FRuntimeSession::Start(...) succeeds using OutSnapshot's own
    // LuaSources -- folds in the post-VM script_set_hash and computes session_content_id,
    // the one hash spanning repository + package + script + presentation identity.
    static void FinalizeScriptIdentity(FGV2SessionContentSnapshot& Snapshot, const std::string& ScriptSetHash);
};

// The only production semantic-Prepare path to resolved presentation content. It borrows
// one immutable session snapshot. Cold-start recovery is a separate native surface and
// never constructs this context.
class FGV2PresentationPrepareContext
{
public:
    explicit FGV2PresentationPrepareContext(const FGV2SessionContentSnapshot& InSnapshot)
        : Snapshot(InSnapshot)
    {
    }

    // PSC-11 (ADR-0043 D4): every authority read goes through one of these accessors, so
    // counting here counts them all. The claim it supports is directional and measurable --
    // Prepare reads the snapshot, and the window around the single Apply facade reads it
    // zero times -- and it stays SECONDARY evidence: the module graph is what makes an
    // authority unreachable from Apply, this only shows the runtime agrees.
    static int32 ConsumeAuthorityAccessCount()
    {
        const int32 Count = AuthorityAccessCount;
        AuthorityAccessCount = 0;
        return Count;
    }

    const FGV2ResolvedUiTheme& GetTheme() const { ++AuthorityAccessCount; return Snapshot.GetTheme(); }
    UClass* GetGameShellClass() const { ++AuthorityAccessCount; return Snapshot.GetGameShellClass(); }
    const FGV2UiSchemaCache& GetSchemaCache() const { ++AuthorityAccessCount; return Snapshot.GetSchemaCache(); }

    bool ResolveScreen(
        const FString& ScreenId,
        const FGV2ScreenPlacement& Placement,
        FGV2ResolvedScreenDescriptor& OutDescriptor,
        FGV2ScreenResolutionRejection& OutRejection) const
    {
        ++AuthorityAccessCount;
        return Snapshot.GetScreenRegistry().Resolve(ScreenId, Placement, OutDescriptor, OutRejection);
    }

    bool ResolveResource(const FString& ResourceId, FGV2ResolvedImageResource& OutResource, FString& OutError) const
    {
        ++AuthorityAccessCount;
        return Snapshot.GetImageCatalog().Resolve(ResourceId, OutResource, OutError);
    }

    TArray<FString> GetResourceIds() const
    {
        ++AuthorityAccessCount;
        return Snapshot.GetImageCatalog().GetResourceIds();
    }

private:
    static inline int32 AuthorityAccessCount = 0;

    const FGV2SessionContentSnapshot& Snapshot;
};
