#include "Application/GV2SessionContentSnapshot.h"

#include "Application/GV2PackageClosure.h"
#include "GV2ContentCore/CanonicalHash.h"
#include "GV2ContentHostSupport/ModsLock.h"
#include "UI/GV2GameShellWidgetBase.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2ScreenRegistry.h"
#include "UI/GV2UiTheme.h"

namespace
{
std::string SnapshotToUtf8(const FString& Value)
{
    const FTCHARToUTF8 Converted(*Value);
    return std::string(Converted.Get(), Converted.Length());
}
} // namespace

// PAH-08: phase=prepare -- a passthrough to UGV2ScreenRegistry::Resolve (the actual
// authority, itself phase=authority). Not yet called from any production path -- PSC-06
// wires FGV2PresentationPrepareContext to reach this only from Prepare.
bool FGV2ResolvedScreenRegistry::Resolve(
    const FString& ScreenId,
    const FGV2ScreenPlacement& Placement,
    FGV2ResolvedScreenDescriptor& OutDescriptor,
    FGV2ScreenResolutionRejection& OutRejection) const
{
    if (!Registry.IsValid())
    {
        OutRejection.Code = EGV2ScreenResolutionError::UnknownScreenId;
        OutRejection.Message = TEXT("core:diagnostic.ui_screen_registry.no_snapshot");
        return false;
    }
    return Registry->Resolve(ScreenId, Placement, OutDescriptor, OutRejection);
}

// PAH-08: phase=prepare -- a passthrough to UGV2ImageResourceCatalog::Resolve. Not yet
// called from any production path -- PSC-06 wires FGV2PresentationPrepareContext to reach
// this only from Prepare.
bool FGV2ResolvedImageCatalog::Resolve(
    const FString& ResourceId,
    FGV2ResolvedImageResource& OutResource,
    FString& OutError) const
{
    if (!Catalog.IsValid())
    {
        OutError = TEXT("no image catalog in session content snapshot");
        return false;
    }
    return Catalog->Resolve(ResourceId, OutResource, OutError);
}

// PAH-04: pre_ready_discovery -- only called from StartSession() (directly, or via
// FGV2SessionCoordinator::StartSession before this session's Status.bIsReady is ever set
// true. The fallback ResolvePackageSetFromDirectories call inside mirrors
// LoadPortableRuntimeSources's own pre-Ready fallback discovery for a caller with no
// ResolvedPackageSet of its own.
bool FGV2SessionContentCandidate::Build(
    const GV2ContentCore::FRepositoryReadHandle& PinnedRepository,
    const GV2ContentHostSupport::FResolvedPackageSet* ResolvedPackageSet,
    const TArray<FGV2SchemaPackageRoot>& SchemaPackageRoots,
    std::vector<GV2RuntimeCore::FRuntimeSource> LuaSources,
    FGV2SessionContentSnapshot& OutSnapshot,
    GV2RuntimeCore::FRuntimeFault& OutFault)
{
    OutSnapshot = FGV2SessionContentSnapshot();

    OutSnapshot.Repository = PinnedRepository;
    OutSnapshot.RepositoryContentHash = UTF8_TO_TCHAR(PinnedRepository.GetContentHash().c_str());

    // A caller with no ResolvedPackageSet of its own (mostly tests -- mirrors
    // LoadPortableRuntimeSources's own fallback) still gets a genuine one here, resolved
    // from the same SchemaPackageRoots LoadPortableRuntimeSources already derived --
    // OrderedPackageIds/package_set_fingerprint/Screen Registry closure below all read from
    // this ONE local, never an empty/partial fallback for a reason unrelated to the
    // caller's actual package set.
    std::optional<GV2ContentHostSupport::FResolvedPackageSet> FallbackResolvedSet;
    if (ResolvedPackageSet == nullptr)
    {
        std::vector<std::filesystem::path> FallbackRoots;
        FallbackRoots.reserve(SchemaPackageRoots.Num());
        for (const FGV2SchemaPackageRoot& Root : SchemaPackageRoots)
        {
            FallbackRoots.emplace_back(TCHAR_TO_UTF8(*Root.RootDirectory));
        }
        std::vector<GV2ContentCore::FDiagnostic> FallbackDiagnostics;
        FallbackResolvedSet = GV2ContentHostSupport::ResolvePackageSetFromDirectories(FallbackRoots, FallbackDiagnostics);
    }
    const GV2ContentHostSupport::FResolvedPackageSet* EffectiveSet = ResolvedPackageSet != nullptr
        ? ResolvedPackageSet
        : (FallbackResolvedSet.has_value() ? &*FallbackResolvedSet : nullptr);

    OutSnapshot.OrderedPackageIds.Reset();
    if (EffectiveSet != nullptr)
    {
        OutSnapshot.OrderedPackageIds.Reserve(static_cast<int32>(EffectiveSet->OrderedSources.size()));
        for (const GV2ContentHostSupport::FResolvedPackageSource& Source : EffectiveSet->OrderedSources)
        {
            OutSnapshot.OrderedPackageIds.Add(UTF8_TO_TCHAR(Source.Descriptor.GetPackageId().c_str()));
        }

        // package_set_fingerprint: ordered {package_id, fingerprint} -- reuses PSC-03's
        // per-package ComputePackageFingerprint, so any semantic manifest field change (any
        // package, known or not to FPackageDescriptor) changes this too.
        std::vector<std::pair<std::string, GV2ContentCore::FValue>> Fields;
        for (const GV2ContentHostSupport::FResolvedPackageSource& Source : EffectiveSet->OrderedSources)
        {
            const std::string Fingerprint = GV2ContentHostSupport::ComputePackageFingerprint(
                Source.Descriptor, Source.CanonicalManifestHash);
            Fields.emplace_back(Source.Descriptor.GetPackageId(), GV2ContentCore::FValue::MakeString(Fingerprint));
        }
        OutSnapshot.PackageSetFingerprint = UTF8_TO_TCHAR(
            GV2ContentCore::ComputeCanonicalHash(GV2ContentCore::FValue::MakeObject(std::move(Fields))).c_str());
    }
    else
    {
        for (const FGV2SchemaPackageRoot& Root : SchemaPackageRoots)
        {
            OutSnapshot.OrderedPackageIds.Add(Root.PackageId);
        }
    }

    OutSnapshot.LuaSources = std::move(LuaSources);

    // Eagerly compiled UI schemas (PSC-04): unknown/invalid schema is a bootstrap failure,
    // never a lazy post-Ready fallback discovered only when some screen first uses it.
    OutSnapshot.SchemaCache = MakeShared<FGV2UiSchemaCache>(SchemaPackageRoots);
    FString SchemaError;
    if (!OutSnapshot.SchemaCache->CompileAll(SchemaError))
    {
        OutFault = {"UiSchemaNotReady", SnapshotToUtf8(SchemaError)};
        return false;
    }

    // Screen Registry -- this candidate resolves it from its own ClosureEntries (derived
    // from ResolvedPackageSet, the same input the repository/Lua/schemas above used), not
    // by reading UGV2RuntimeSubsystem's own GameInstance-lifetime member. The underlying
    // UGV2ScreenRegistry DataAsset is necessarily the one configured
    // UGV2ScreenRegistrySettings::RegistryAsset soft-references (a content-authored
    // singleton, not something a session constructs fresh) -- Build() is idempotent and
    // safe to call again against the same object with the same ClosureEntries.
    const UGV2ScreenRegistrySettings* RegistrySettings = GetDefault<UGV2ScreenRegistrySettings>();
    UGV2ScreenRegistry* RegistryAsset = RegistrySettings != nullptr
        ? RegistrySettings->RegistryAsset.LoadSynchronous()
        : nullptr;
    if (RegistryAsset == nullptr)
    {
        OutFault = {"ScreenRegistryNotReady", "UGV2ScreenRegistrySettings has no configured RegistryAsset."};
        return false;
    }
    // ClosureEntries is a pure projection (PSC-02) of the same EffectiveSet used above --
    // when the caller supplied one, or the fallback resolved from SchemaPackageRoots
    // otherwise; falls back to the raw SchemaPackageRoots directly only if that fallback
    // resolution itself failed, so Build() below still has a real closure to work with.
    TArray<GV2PackageClosure::FEntry> ClosureEntries;
    if (EffectiveSet != nullptr)
    {
        ClosureEntries = GV2PackageClosure::FromResolvedPackageSet(*EffectiveSet);
    }
    else
    {
        ClosureEntries.Reserve(SchemaPackageRoots.Num());
        for (const FGV2SchemaPackageRoot& SchemaRoot : SchemaPackageRoots)
        {
            ClosureEntries.Add(GV2PackageClosure::FEntry{SchemaRoot.PackageId, SchemaRoot.RootDirectory});
        }
    }
    FString RegistryError;
    if (!RegistryAsset->Build(ClosureEntries, RegistryError))
    {
        OutFault = {"ScreenRegistryNotReady", SnapshotToUtf8(RegistryError)};
        return false;
    }
    OutSnapshot.ScreenRegistry.Registry = TStrongObjectPtr<UGV2ScreenRegistry>(RegistryAsset);

    // Image Catalog -- a fresh transient instance this candidate owns, mirroring
    // UGV2ImageResourceCatalog::RebuildForSession's own construction (NewObject +
    // BuildFromPackageClosure) but pinned here instead of the session-lifetime static.
    UGV2ImageResourceCatalog* CatalogInstance = NewObject<UGV2ImageResourceCatalog>(GetTransientPackage());
    FString CatalogError;
    if (!CatalogInstance->BuildFromPackageClosure(OutSnapshot.OrderedPackageIds, CatalogError))
    {
        OutFault = {"ImageCatalogNotReady", SnapshotToUtf8(CatalogError)};
        return false;
    }
    OutSnapshot.ImageCatalog.Catalog = TStrongObjectPtr<UGV2ImageResourceCatalog>(CatalogInstance);

    // Theme -- resolved once here (settings/DataAsset are legitimate bootstrap input per
    // BootstrapAndSessionLifecycle.md's target rule), pinned for this snapshot's lifetime.
    UGV2UiTheme* ResolvedTheme = UGV2UiThemeSettings::GetConfiguredTheme();
    if (ResolvedTheme == nullptr)
    {
        OutFault = {"ThemeNotReady", "No configured or fallback Theme could be resolved."};
        return false;
    }
    OutSnapshot.Theme.Theme = TStrongObjectPtr<UGV2UiTheme>(ResolvedTheme);

    UClass* GameShellClass = RegistrySettings != nullptr
        ? RegistrySettings->GameShellClass.LoadSynchronous()
        : nullptr;
    OutSnapshot.GameShellClass = TStrongObjectPtr<UClass>(GameShellClass);

    // presentation_hash: resolved screens' (screen_id, widget class path), resolved
    // resources' (resource_id, texture soft path, render mode), Theme asset path, GameShell
    // class path -- every field the Done bullet names, all from already-resolved
    // identities, never raw authoring rows.
    std::vector<std::pair<std::string, GV2ContentCore::FValue>> PresentationFields;

    std::vector<GV2ContentCore::FValue> ScreensArray;
    for (const TPair<FString, FString>& Identity : RegistryAsset->GetResolvedScreenIdentities())
    {
        std::vector<std::pair<std::string, GV2ContentCore::FValue>> ScreenFields;
        ScreenFields.emplace_back("screen_id", GV2ContentCore::FValue::MakeString(SnapshotToUtf8(Identity.Key)));
        ScreenFields.emplace_back("widget_class", GV2ContentCore::FValue::MakeString(SnapshotToUtf8(Identity.Value)));
        ScreensArray.push_back(GV2ContentCore::FValue::MakeObject(std::move(ScreenFields)));
    }
    PresentationFields.emplace_back("screens", GV2ContentCore::FValue::MakeArray(std::move(ScreensArray)));

    std::vector<GV2ContentCore::FValue> ResourcesArray;
    TArray<FGV2ImageResourceDefinition> ResourceEntries = CatalogInstance->GetEntries();
    ResourceEntries.Sort([](const FGV2ImageResourceDefinition& A, const FGV2ImageResourceDefinition& B)
    {
        return A.ResourceId < B.ResourceId;
    });
    for (const FGV2ImageResourceDefinition& Entry : ResourceEntries)
    {
        std::vector<std::pair<std::string, GV2ContentCore::FValue>> ResourceFields;
        ResourceFields.emplace_back("resource_id", GV2ContentCore::FValue::MakeString(SnapshotToUtf8(Entry.ResourceId)));
        ResourceFields.emplace_back("texture", GV2ContentCore::FValue::MakeString(SnapshotToUtf8(Entry.Texture.ToString())));
        ResourceFields.emplace_back(
            "render_mode", GV2ContentCore::FValue::MakeInteger(static_cast<std::int64_t>(Entry.RenderMode)));
        ResourcesArray.push_back(GV2ContentCore::FValue::MakeObject(std::move(ResourceFields)));
    }
    PresentationFields.emplace_back("resources", GV2ContentCore::FValue::MakeArray(std::move(ResourcesArray)));

    PresentationFields.emplace_back("theme", GV2ContentCore::FValue::MakeString(SnapshotToUtf8(ResolvedTheme->GetPathName())));
    PresentationFields.emplace_back(
        "game_shell_class",
        GV2ContentCore::FValue::MakeString(
            GameShellClass != nullptr ? SnapshotToUtf8(GameShellClass->GetPathName()) : std::string()));

    OutSnapshot.PresentationHash = UTF8_TO_TCHAR(
        GV2ContentCore::ComputeCanonicalHash(GV2ContentCore::FValue::MakeObject(std::move(PresentationFields))).c_str());

    return true;
}

void FGV2SessionContentCandidate::FinalizeScriptIdentity(
    FGV2SessionContentSnapshot& Snapshot,
    const std::string& ScriptSetHash)
{
    Snapshot.ScriptSetHash = UTF8_TO_TCHAR(ScriptSetHash.c_str());

    // session_content_id: canonically combines repository/package/script/presentation
    // identities -- the one identity that only exists once all four are known.
    std::vector<std::pair<std::string, GV2ContentCore::FValue>> Fields;
    Fields.emplace_back("repository_content_hash", GV2ContentCore::FValue::MakeString(SnapshotToUtf8(Snapshot.RepositoryContentHash)));
    Fields.emplace_back("package_set_fingerprint", GV2ContentCore::FValue::MakeString(SnapshotToUtf8(Snapshot.PackageSetFingerprint)));
    Fields.emplace_back("script_set_hash", GV2ContentCore::FValue::MakeString(ScriptSetHash));
    Fields.emplace_back("presentation_hash", GV2ContentCore::FValue::MakeString(SnapshotToUtf8(Snapshot.PresentationHash)));
    Snapshot.SessionContentId = UTF8_TO_TCHAR(
        GV2ContentCore::ComputeCanonicalHash(GV2ContentCore::FValue::MakeObject(std::move(Fields))).c_str());
}
