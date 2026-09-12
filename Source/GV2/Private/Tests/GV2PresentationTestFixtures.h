#pragma once

#include "Application/GV2FilesystemContentSourceProvider.h"
#include "Application/GV2SessionContentSnapshot.h"
#include "Bridge/GV2BridgeTypes.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"
#include "Misc/Paths.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "Application/GV2PackageClosure.h"
#include "UI/GV2RichTextPopoverWidgetBase.h"
#include "UI/GV2UiTheme.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace GV2PresentationTestFixtures
{
// CFC-02A: Scoped RAII owner for a test GameInstance and World.
// Guarantees that GameInstance->Shutdown() and GameInstance->RemoveFromRoot() are called
// on any exit from the scope, eliminating leaks of rooted game instances and world contexts.
class FScopedTestWorldContext final
{
public:
    FScopedTestWorldContext()
    {
        check(GEngine != nullptr);
        GameInstance = NewObject<UGameInstance>(GEngine);
        check(GameInstance != nullptr);
        GameInstance->AddToRoot();
        GameInstance->InitializeStandalone();
        World = GameInstance->GetWorld();
        check(World != nullptr);
    }

    ~FScopedTestWorldContext()
    {
        Teardown();
    }

    FScopedTestWorldContext(const FScopedTestWorldContext&) = delete;
    FScopedTestWorldContext& operator=(const FScopedTestWorldContext&) = delete;

    void Teardown()
    {
        if (GameInstance != nullptr)
        {
            if (World != nullptr)
            {
                if (GEngine != nullptr)
                {
                    GEngine->DestroyWorldContext(World);
                }
                World->DestroyWorld(false);
                World = nullptr;
            }
            GameInstance->Shutdown();
            GameInstance->RemoveFromRoot();
            GameInstance = nullptr;
        }
    }

    UWorld* GetWorld() const { return World; }
    UGameInstance* GetGameInstance() const { return GameInstance; }

    template <typename TWidget>
    TWidget* CreateTestWidget(UClass* Class = TWidget::StaticClass()) const
    {
        check(World != nullptr);
        return CreateWidget<TWidget>(World, Class);
    }

private:
    UGameInstance* GameInstance = nullptr;
    UWorld* World = nullptr;
};

// PSC-11: tests carry an FString of their own; the single Apply facade returns a result
// struct. This adapts one to the other for test call sites only -- production has exactly
// one way to apply a transaction and does not go through here.
inline bool ApplyPreparedTransaction(
    const GV2PresentationApply::FGV2PreparedPresentationTransaction& Transaction,
    FString& OutError)
{
    FGV2PresentationApplyResult Result;
    const bool bApplied = FGV2PresentationApply::Apply(Transaction, Result);
    OutError = Result.Error;
    return bApplied;
}

// A compact, test-only owner for the same immutable presentation snapshot that
// production Prepare receives from FGV2SessionCoordinator. Tests below the
// coordinator boundary use it instead of reviving process-global Theme/Registry
// access or accepting a missing-context failure as a false positive.
class FPrepareContextFixture final
{
public:
    // PSC-10B: the popover renderer class is resolved ONCE, at snapshot build. A test that
    // needs a session whose Theme declares no popover renderer must therefore make that true
    // BEFORE the snapshot is built -- mutating the Theme asset afterwards no longer reaches
    // anything, which is exactly the property the move to the snapshot bought.
    struct FWithoutRichTextPopoverRenderer
    {
        explicit FWithoutRichTextPopoverRenderer(UGV2UiTheme* InTheme)
            : Theme(InTheme)
        {
            if (Theme != nullptr)
            {
                Saved = Theme->RichTextPopoverClass;
                Theme->RichTextPopoverClass = nullptr;
            }
        }
        // Restores on ANY exit, including an early return from a failing assertion -- a
        // plain assignment at the end of the block leaves the shared Theme asset broken for
        // every later test when the block exits early.
        ~FWithoutRichTextPopoverRenderer()
        {
            if (Theme != nullptr)
            {
                Theme->RichTextPopoverClass = Saved;
            }
        }
        FWithoutRichTextPopoverRenderer(const FWithoutRichTextPopoverRenderer&) = delete;
        FWithoutRichTextPopoverRenderer& operator=(const FWithoutRichTextPopoverRenderer&) = delete;

    private:
        UGV2UiTheme* Theme = nullptr;
        TSoftClassPtr<UGV2RichTextPopoverWidgetBase> Saved;
    };

    bool Initialize(FString& OutError)
    {
        OutError.Reset();
        if (Context.IsValid())
        {
            return true;
        }

        const FString GameDataDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData"));
        const std::vector<std::filesystem::path> PackageRoots = {
            std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, TEXT("core")))),
            std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, TEXT("textsystem")))),
            std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, TEXT("rh")))),
        };

        std::vector<GV2ContentCore::FDiagnostic> ResolveDiagnostics;
        const std::optional<GV2ContentHostSupport::FResolvedPackageSet> ResolvedSet =
            GV2ContentHostSupport::ResolvePackageSetFromDirectories(PackageRoots, ResolveDiagnostics);
        if (!ResolvedSet.has_value())
        {
            OutError = TEXT("Unable to resolve the core+textsystem+rh test package set.");
            return false;
        }

        const GV2ContentCore::FBuildResult RepositoryBuild =
            BuildGV2RepositoryFromResolvedPackageSet(*ResolvedSet);
        if (!RepositoryBuild.IsSuccess())
        {
            OutError = TEXT("Unable to build the test repository snapshot.");
            return false;
        }

        TArray<FGV2SchemaPackageRoot> SchemaRoots;
        SchemaRoots.Reserve(static_cast<int32>(ResolvedSet->OrderedSources.size()));
        for (const GV2ContentHostSupport::FResolvedPackageSource& Source : ResolvedSet->OrderedSources)
        {
            SchemaRoots.Add(FGV2SchemaPackageRoot{
                UTF8_TO_TCHAR(Source.Descriptor.GetPackageId().c_str()),
                UTF8_TO_TCHAR(Source.Root.string().c_str())});
        }

        GV2RuntimeCore::FRuntimeFault Fault;
        if (!FGV2SessionContentCandidate::Build(
                RepositoryBuild.GetCandidate().GetReadHandle(),
                *ResolvedSet,
                SchemaRoots,
                {},
                Snapshot,
                Fault))
        {
            OutError = FString::Printf(
                TEXT("Unable to build the test presentation snapshot: %s: %s"),
                UTF8_TO_TCHAR(Fault.Code.c_str()),
                UTF8_TO_TCHAR(Fault.Message.c_str()));
            return false;
        }

        Context = MakeUnique<FGV2PresentationPrepareContext>(Snapshot);
        return true;
    }

    const FGV2PresentationPrepareContext* Get() const { return Context.Get(); }

private:
    FGV2SessionContentSnapshot Snapshot;
    TUniquePtr<FGV2PresentationPrepareContext> Context;
};

// PSC-10C: the image catalog a session builds, as a plain instance. The process-global
// session catalog it replaces is gone: production resolves resources through the snapshot,
// so a test that needs a catalog constructs the same object the candidate builder does
// rather than publishing one into a global nothing reads any more.
inline UGV2ImageResourceCatalog* BuildImageCatalogForClosure(const TArray<FString>& PackageIds, FString& OutError)
{
    UGV2ImageResourceCatalog* Catalog = NewObject<UGV2ImageResourceCatalog>(GetTransientPackage());
    return Catalog->BuildFromPackageClosure(PackageIds, OutError) ? Catalog : nullptr;
}

inline UGV2ImageResourceCatalog* BuildGameDataImageCatalog(FString& OutError)
{
    TArray<FString> PackageIds;
    for (const GV2PackageClosure::FEntry& Entry : GV2PackageClosure::DiscoverFromGameData())
    {
        PackageIds.Add(Entry.PackageId);
    }
    return BuildImageCatalogForClosure(PackageIds, OutError);
}

// PSC-10B: the CONFIGURED Theme, with no core-minimal substitution. Production refuses to
// start a session whose configured Theme cannot be resolved, so a fixture that quietly ran
// on the minimal Theme instead would be testing a configuration production rejects.
inline const UGV2UiTheme* LoadTheme()
{
    const UGV2UiThemeSettings* Settings = GetDefault<UGV2UiThemeSettings>();
    return Settings != nullptr && !Settings->ThemeAsset.IsNull()
        ? Settings->ThemeAsset.LoadSynchronous()
        : nullptr;
}

// Test-only constructor for values that normally come from the Screen Field materializer.
// It deliberately fills the complete prepared payload: tests of lower consumers must not
// revive the retired Apply-time Theme fallback merely for convenient literals.
// PSC-10B: delegates to the pipeline's single literal-resolution seam instead of
// re-implementing the resolved-presentation filling. A fixture that fills those fields
// itself can drift from what production Resolve() produces -- the earlier revision of this
// helper did exactly that, skipping the style-token validation the real resolver performs.
inline FGV2TextViewModel MakeResolvedText(
    const FString& Text,
    FName StyleToken = NAME_None
)
{
    FGV2TextViewModel Result;
    FString Error;
    UGV2TextPipeline::ResolveLiteralForAutomationTest(LoadTheme(), Text, StyleToken, Result, Error);
    return Result;
}
}
