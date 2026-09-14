#pragma once

#include "Application/GV2FilesystemContentSourceProvider.h"
#include "Application/GV2SessionCoordinator.h"
#include "Application/GV2SessionContentSnapshot.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"
#include "Bridge/GV2BridgeTypes.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"
#include "HAL/PlatformTime.h"
#include "Layout/ArrangedChildren.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Styling/WidgetStyle.h"
#include "Application/GV2PackageClosure.h"
#include "UI/GV2ImagePresentation.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2RichTextPopoverWidgetBase.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2UiSchemaCache.h"
#include "UI/GV2UiTheme.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SVirtualWindow.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

// CFC-04: Test-only accessor allowing tests under Tests/ to construct and compile isolated
// FGV2UiSchemaCache instances via friended private constructor.
class FGV2UiSchemaCacheTestAccess
{
public:
    static TSharedPtr<FGV2UiSchemaCache> Create(TArray<FGV2SchemaPackageRoot> InPackageRoots)
    {
        return TSharedPtr<FGV2UiSchemaCache>(new FGV2UiSchemaCache(MoveTemp(InPackageRoots)));
    }

    static bool CompileAll(const FGV2UiSchemaCache& Cache, FString& OutError)
    {
        return Cache.CompileAll(OutError);
    }
};

class FGV2ImageResourceCatalogTestAccess
{
public:
    static void AddResolvedResource(UGV2ImageResourceCatalog* Catalog, const FString& Id, FGV2ResolvedImageResource Resource)
    {
        if (Catalog != nullptr)
        {
            Catalog->ResolvedById.Add(Id, MoveTemp(Resource));
        }
    }
};

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
            GameInstance->Shutdown();
            if (World != nullptr)
            {
                if (GEngine != nullptr)
                {
                    GEngine->DestroyWorldContext(World);
                }
                World->DestroyWorld(false);
                World = nullptr;
            }
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

// CFC-02A / ADR-0040: Scoped RAII owner for any UObject rooted during a test.
// Guarantees that Object->RemoveFromRoot() is called on exit from scope.
template <typename T = UObject>
class TScopedRootObject final
{
public:
    TScopedRootObject()
        : Object(nullptr)
    {
    }

    explicit TScopedRootObject(T* InObject)
        : Object(InObject)
    {
        if (Object != nullptr)
        {
            Object->AddToRoot();
        }
    }

    ~TScopedRootObject()
    {
        Reset();
    }

    TScopedRootObject(const TScopedRootObject&) = delete;
    TScopedRootObject& operator=(const TScopedRootObject&) = delete;

    TScopedRootObject(TScopedRootObject&& Other) noexcept
        : Object(Other.Object)
    {
        Other.Object = nullptr;
    }

    TScopedRootObject& operator=(TScopedRootObject&& Other) noexcept
    {
        if (this != &Other)
        {
            Reset();
            Object = Other.Object;
            Other.Object = nullptr;
        }
        return *this;
    }

    void Reset(T* NewObject = nullptr)
    {
        if (Object != nullptr)
        {
            Object->RemoveFromRoot();
        }
        Object = NewObject;
        if (Object != nullptr)
        {
            Object->AddToRoot();
        }
    }

    T* Get() const { return Object; }
    T* operator->() const { return Object; }
    T& operator*() const { check(Object != nullptr); return *Object; }
    operator T*() const { return Object; }
    explicit operator bool() const { return Object != nullptr; }

private:
    T* Object = nullptr;
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
        SchemaRoots.Reserve(static_cast<int32>(ResolvedSet->OrderedSources.size()) + 1);
        for (const GV2ContentHostSupport::FResolvedPackageSource& Source : ResolvedSet->OrderedSources)
        {
            SchemaRoots.Add(FGV2SchemaPackageRoot{
                UTF8_TO_TCHAR(Source.Descriptor.GetPackageId().c_str()),
                UTF8_TO_TCHAR(Source.Root.string().c_str())});
        }
        SchemaRoots.Add(FGV2SchemaPackageRoot{
            TEXT("core"),
            FPaths::Combine(FPaths::ProjectDir(), TEXT("Tests/Fixtures/SyntheticMechanicalFixture/schemas"))
        });

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

inline void RegisterTestImageResource(
    UGV2ImageResourceCatalog* Catalog,
    const FString& ResourceId,
    EGV2ImageRenderMode RenderMode = EGV2ImageRenderMode::FixedAspect,
    const FMargin& Margin = FMargin())
{
    if (Catalog == nullptr)
    {
        return;
    }
    UTexture2D* Tex = UTexture2D::CreateTransient(64, 64);
    FGV2ImageResourceDefinition Def;
    Def.ResourceId = ResourceId;
    Def.RenderMode = RenderMode;
    Def.Texture = Tex;
    Def.NineSliceBorderPixels = Margin;
    FGV2ResolvedImageResource Resolved;
    FString Error;
    if (UGV2ImageResourceCatalog::ResolveDefinition(Def, Resolved, Error))
    {
        FGV2ImageResourceCatalogTestAccess::AddResolvedResource(Catalog, ResourceId, MoveTemp(Resolved));
    }
}

inline UGV2ImageResourceCatalog* BuildGameDataImageCatalog(FString& OutError)
{
    TArray<FString> PackageIds;
    for (const GV2PackageClosure::FEntry& Entry : GV2PackageClosure::DiscoverFromGameData())
    {
        PackageIds.Add(Entry.PackageId);
    }
    UGV2ImageResourceCatalog* Catalog = BuildImageCatalogForClosure(PackageIds, OutError);
    if (Catalog != nullptr)
    {
        RegisterTestImageResource(Catalog, TEXT("core:resource.ui.test_fixed_aspect"), EGV2ImageRenderMode::FixedAspect);
        RegisterTestImageResource(Catalog, TEXT("core:resource.ui.test_character"), EGV2ImageRenderMode::FixedAspect);
    }
    return Catalog;
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

// TSR-03: Shared loader for configured Theme with minimal fallback, used across presentation and UI tests.
inline UGV2UiTheme* LoadConfiguredThemeForTest()
{
    const UGV2UiThemeSettings* Settings = GetDefault<UGV2UiThemeSettings>();
    UGV2UiTheme* Theme = Settings != nullptr && !Settings->ThemeAsset.IsNull()
        ? Settings->ThemeAsset.LoadSynchronous()
        : nullptr;
    return Theme != nullptr ? Theme : UGV2UiTheme::GetCoreMinimalTheme();
}

// TSR-03 / PSC-10B: Shared literal-resolution seam delegating to UGV2TextPipeline's implementation.
inline FGV2TextViewModel MakeResolvedLiteralTextForTest(
    const UGV2UiTheme& Theme,
    const FString& Text,
    FName StyleToken = NAME_None)
{
    FGV2TextViewModel Result;
    FString Error;
    UGV2TextPipeline::ResolveLiteralForAutomationTest(&Theme, Text, StyleToken, Result, Error);
    return Result;
}

// TSR-03: Shared adapter converting FGV2ResolvedImageResource to prepared presentation value.
inline GV2PresentationApply::FPreparedResolvedImageValue MakePreparedResolvedImageForTest(
    const FGV2ResolvedImageResource& Resolved)
{
    GV2PresentationApply::FPreparedResolvedImageValue Result;
    Result.ResourceId = Resolved.ResourceId;
    Result.RenderMode = FGV2ImagePresentation::ToPreparedRenderMode(Resolved.RenderMode);
    Result.FixedAspectRatio = Resolved.FixedAspectRatio;
    Result.Brush = Resolved.Brush;
    return Result;
}

// TSR-03 / CBM-03 / CFC-02A: Scoped RAII override for tests requiring the sample package.
// Preserves and restores previous session coordinator and theme catalog state on ANY exit
// (including early return or nested scopes), fulfilling test isolation requirements.
class FGV2ScopedSamplePackageOverride final
{
public:
    FGV2ScopedSamplePackageOverride()
    {
        bPreviousForceIncludeSamplePackage = FGV2SessionCoordinator::bTestForceIncludeSamplePackage;
        FGV2SessionCoordinator::bTestForceIncludeSamplePackage = true;

        if (UGV2UiTheme* Theme = LoadConfiguredThemeForTest())
        {
            TargetTheme = Theme;
            const FString Pkg = TEXT("sample");
            const TArray<TTuple<FString, FText>> EntriesToApply = {
                { FString::Printf(TEXT("%s:text.location.hub.title"), *Pkg), FText::FromString(TEXT("Central Hub")) },
                { FString::Printf(TEXT("%s:text.screen.hub.description"), *Pkg), FText::FromString(TEXT("You are standing in the central hub.")) },
                { FString::Printf(TEXT("%s:text.location.east.title"), *Pkg), FText::FromString(TEXT("East Wing")) },
                { FString::Printf(TEXT("%s:text.screen.east.description"), *Pkg), FText::FromString(TEXT("You are in the quiet east wing.")) },
                { FString::Printf(TEXT("%s:text.location.west.title"), *Pkg), FText::FromString(TEXT("West Wing")) },
                { FString::Printf(TEXT("%s:text.screen.west.description"), *Pkg), FText::FromString(TEXT("You are in the windy west wing.")) },
                { FString::Printf(TEXT("%s:text.action.scout"), *Pkg), FText::FromString(TEXT("Scout Area")) }
            };

            for (const auto& Entry : EntriesToApply)
            {
                const FString& Key = Entry.Get<0>();
                const FText& Val = Entry.Get<1>();
                if (const FText* Existing = Theme->FallbackTextCatalog.Find(Key))
                {
                    SavedCatalogEntries.Add(Key, *Existing);
                }
                else
                {
                    SavedCatalogEntries.Add(Key, TOptional<FText>());
                }
                Theme->FallbackTextCatalog.Add(Key, Val);
            }
        }
    }

    ~FGV2ScopedSamplePackageOverride()
    {
        Teardown();
    }

    FGV2ScopedSamplePackageOverride(const FGV2ScopedSamplePackageOverride&) = delete;
    FGV2ScopedSamplePackageOverride& operator=(const FGV2ScopedSamplePackageOverride&) = delete;

    void Teardown()
    {
        FGV2SessionCoordinator::bTestForceIncludeSamplePackage = bPreviousForceIncludeSamplePackage;

        if (UGV2UiTheme* Theme = TargetTheme.Get())
        {
            for (const auto& Pair : SavedCatalogEntries)
            {
                if (Pair.Value.IsSet())
                {
                    Theme->FallbackTextCatalog.Add(Pair.Key, Pair.Value.GetValue());
                }
                else
                {
                    Theme->FallbackTextCatalog.Remove(Pair.Key);
                }
            }
            SavedCatalogEntries.Empty();
            TargetTheme = nullptr;
        }
    }

private:
    bool bPreviousForceIncludeSamplePackage = false;
    TWeakObjectPtr<UGV2UiTheme> TargetTheme;
    TMap<FString, TOptional<FText>> SavedCatalogEntries;
};

// TSR-03 / DCA-13: walks the Slate tree and calls Tick() directly on every widget that
// still wants one, using the geometry PaintWindow just cached for it.
inline void GV2TickWidgetSubtreeRecursively(const TSharedRef<SWidget>& Widget, double CurrentTime, float DeltaTime)
{
    if (Widget->GetCanTick())
    {
        Widget->Tick(Widget->GetTickSpaceGeometry(), CurrentTime, DeltaTime);
    }
    if (FChildren* Children = Widget->GetAllChildren())
    {
        const int32 NumChildren = Children->Num();
        for (int32 Index = 0; Index < NumChildren; ++Index)
        {
            const TSharedRef<SWidget> Child = Children->GetChildAt(Index);
            if (Child != SNullWidget::NullWidget)
            {
                GV2TickWidgetSubtreeRecursively(Child, CurrentTime, DeltaTime);
            }
        }
    }
}

// TSR-03 / DCA-13: one full simulated frame on an off-screen SVirtualWindow honest
// about dynamic (Tick-driven) layouts.
inline void GV2SimulateResponsiveFrame(const TSharedRef<SVirtualWindow>& Window, const FVector2D& Size)
{
    Window->Resize(Size);
    Window->SlatePrepass(1.0f);
    {
        FSlateWindowElementList SeedElementList(Window);
        Window->PaintWindow(FPlatformTime::Seconds(), 0.016f, SeedElementList, FWidgetStyle(), true);
    }
    GV2TickWidgetSubtreeRecursively(Window, FPlatformTime::Seconds(), 0.016f);
    Window->SlatePrepass(1.0f);
    FSlateWindowElementList WindowElementList(Window);
    Window->PaintWindow(FPlatformTime::Seconds(), 0.016f, WindowElementList, FWidgetStyle(), true);
}
}

// TSR-06 (ADR-0046, Plan TestSuiteRestructuring):
// Minimal synthetic mechanical fixture under core: namespace, covering location mechanics,
// scene with characters, command panel with transitions, and screen with field slots.
namespace GV2SyntheticMechanicalFixture
{
    // Stable IDs for the synthetic fixture (all strictly under core:)
    inline const TCHAR* const LocationAlphaId = TEXT("core:location.test.alpha");
    inline const TCHAR* const LocationBetaId = TEXT("core:location.test.beta");
    inline const TCHAR* const SyntheticScreenId = TEXT("core:screen.test.location");

    // UI Field Schema IDs
    inline const TCHAR* const SceneSchemaId = TEXT("core:schema.ui_field.synthetic_scene.v1");
    inline const TCHAR* const CommandsSchemaId = TEXT("core:schema.ui_field.synthetic_commands.v1");
    inline const TCHAR* const TopBarSchemaId = TEXT("core:schema.ui_field.synthetic_top_bar.v1");
    inline const TCHAR* const PlayerStatusSchemaId = TEXT("core:schema.ui_field.synthetic_player_status.v1");

    // Semantic tokens / keys
    inline const FName CharacterKey = FName(TEXT("guide"));
    inline const TCHAR* const CharacterResourceId = TEXT("core:resource.character.test_guide");
    inline const FName TravelBetaKey = FName(TEXT("travel_beta"));
    inline const TCHAR* const TravelBetaCommandId = TEXT("core:command.test.travel_beta");
    inline const FName TravelAlphaKey = FName(TEXT("travel_alpha"));
    inline const TCHAR* const TravelAlphaCommandId = TEXT("core:command.test.travel_alpha");

    // Paths to synthetic fixture files
    inline FString GetSyntheticFixtureDir()
    {
        return FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectDir(), TEXT("Tests/Fixtures/SyntheticMechanicalFixture")));
    }

    inline FString GetSyntheticSchemaDir()
    {
        return FPaths::Combine(GetSyntheticFixtureDir(), TEXT("schemas"));
    }

    inline FString GetSyntheticDefinitionsDir()
    {
        return FPaths::Combine(GetSyntheticFixtureDir(), TEXT("definitions"));
    }

    // Builders for synthetic FScreenRequest
    inline GV2RuntimeCore::FScreenRequest CreateLocationAlphaScreenRequest()
    {
        GV2RuntimeCore::FScreenRequest Request;
        Request.ScreenId = TCHAR_TO_UTF8(SyntheticScreenId);

        // 1. Scene field (1 character: guide)
        {
            GV2RuntimeCore::FScreenField SceneField;
            SceneField.FieldId = "scene";
            SceneField.SchemaId = TCHAR_TO_UTF8(SceneSchemaId);
            GV2RuntimeCore::FValue::FObject SceneObj;
            SceneObj["key"] = GV2RuntimeCore::FValue("scene_alpha");
            
            GV2RuntimeCore::FValue::FObject TextObj;
            TextObj["text_id"] = GV2RuntimeCore::FValue("core:text.location.test.alpha.desc");
            SceneObj["context_text"] = GV2RuntimeCore::FValue(MoveTemp(TextObj));
            SceneObj["background_resource_id"] = GV2RuntimeCore::FValue("core:resource.location.test_alpha_bg");

            GV2RuntimeCore::FValue::FObject CharObj;
            CharObj["key"] = GV2RuntimeCore::FValue(TCHAR_TO_UTF8(*CharacterKey.ToString()));
            CharObj["resource_id"] = GV2RuntimeCore::FValue(TCHAR_TO_UTF8(CharacterResourceId));
            SceneObj["characters"] = GV2RuntimeCore::FValue(GV2RuntimeCore::FValue::FArray{
                GV2RuntimeCore::FValue(MoveTemp(CharObj))
            });

            SceneField.Value = GV2RuntimeCore::FValue(MoveTemp(SceneObj));
            Request.Fields.push_back(MoveTemp(SceneField));
        }

        // 2. Commands field (1 travel command to beta)
        {
            GV2RuntimeCore::FScreenField CommandsField;
            CommandsField.FieldId = "commands";
            CommandsField.SchemaId = TCHAR_TO_UTF8(CommandsSchemaId);
            GV2RuntimeCore::FValue::FObject CmdsObj;
            CmdsObj["key"] = GV2RuntimeCore::FValue("commands_alpha");

            GV2RuntimeCore::FValue::FObject ItemObj;
            ItemObj["key"] = GV2RuntimeCore::FValue(TCHAR_TO_UTF8(*TravelBetaKey.ToString()));
            GV2RuntimeCore::FValue::FObject ItemTextObj;
            ItemTextObj["text_id"] = GV2RuntimeCore::FValue("core:text.command.test.travel_beta");
            ItemObj["text"] = GV2RuntimeCore::FValue(MoveTemp(ItemTextObj));
            GV2RuntimeCore::FValue::FObject BindingObj;
            BindingObj["command_id"] = GV2RuntimeCore::FValue(TCHAR_TO_UTF8(TravelBetaCommandId));
            ItemObj["binding"] = GV2RuntimeCore::FValue(MoveTemp(BindingObj));

            CmdsObj["items"] = GV2RuntimeCore::FValue(GV2RuntimeCore::FValue::FArray{
                GV2RuntimeCore::FValue(MoveTemp(ItemObj))
            });

            CommandsField.Value = GV2RuntimeCore::FValue(MoveTemp(CmdsObj));
            Request.Fields.push_back(MoveTemp(CommandsField));
        }

        // 3. TopBar field
        {
            GV2RuntimeCore::FScreenField TopBarField;
            TopBarField.FieldId = "top_bar";
            TopBarField.SchemaId = TCHAR_TO_UTF8(TopBarSchemaId);
            GV2RuntimeCore::FValue::FObject TopBarObj;
            TopBarObj["key"] = GV2RuntimeCore::FValue("top_bar_alpha");
            GV2RuntimeCore::FValue::FObject TitleTextObj;
            TitleTextObj["text_id"] = GV2RuntimeCore::FValue("core:text.location.test.alpha.title");
            TopBarObj["title"] = GV2RuntimeCore::FValue(MoveTemp(TitleTextObj));
            TopBarField.Value = GV2RuntimeCore::FValue(MoveTemp(TopBarObj));
            Request.Fields.push_back(MoveTemp(TopBarField));
        }

        // 4. PlayerStatus field
        {
            GV2RuntimeCore::FScreenField StatusField;
            StatusField.FieldId = "player_status";
            StatusField.SchemaId = TCHAR_TO_UTF8(PlayerStatusSchemaId);
            GV2RuntimeCore::FValue::FObject StatusObj;
            StatusObj["key"] = GV2RuntimeCore::FValue("status_alpha");
            GV2RuntimeCore::FValue::FObject NameTextObj;
            NameTextObj["text_id"] = GV2RuntimeCore::FValue("core:text.character.test_hero.name");
            StatusObj["name"] = GV2RuntimeCore::FValue(MoveTemp(NameTextObj));
            StatusField.Value = GV2RuntimeCore::FValue(MoveTemp(StatusObj));
            Request.Fields.push_back(MoveTemp(StatusField));
        }

        return Request;
    }

    inline GV2RuntimeCore::FScreenRequest CreateLocationBetaScreenRequest()
    {
        GV2RuntimeCore::FScreenRequest Request;
        Request.ScreenId = TCHAR_TO_UTF8(SyntheticScreenId);

        // 1. Scene field (0 characters)
        {
            GV2RuntimeCore::FScreenField SceneField;
            SceneField.FieldId = "scene";
            SceneField.SchemaId = TCHAR_TO_UTF8(SceneSchemaId);
            GV2RuntimeCore::FValue::FObject SceneObj;
            SceneObj["key"] = GV2RuntimeCore::FValue("scene_beta");
            
            GV2RuntimeCore::FValue::FObject TextObj;
            TextObj["text_id"] = GV2RuntimeCore::FValue("core:text.location.test.beta.desc");
            SceneObj["context_text"] = GV2RuntimeCore::FValue(MoveTemp(TextObj));
            SceneObj["background_resource_id"] = GV2RuntimeCore::FValue("core:resource.location.test_beta_bg");
            SceneObj["characters"] = GV2RuntimeCore::FValue(GV2RuntimeCore::FValue::FArray{});

            SceneField.Value = GV2RuntimeCore::FValue(MoveTemp(SceneObj));
            Request.Fields.push_back(MoveTemp(SceneField));
        }

        // 2. Commands field (1 travel command to alpha)
        {
            GV2RuntimeCore::FScreenField CommandsField;
            CommandsField.FieldId = "commands";
            CommandsField.SchemaId = TCHAR_TO_UTF8(CommandsSchemaId);
            GV2RuntimeCore::FValue::FObject CmdsObj;
            CmdsObj["key"] = GV2RuntimeCore::FValue("commands_beta");

            GV2RuntimeCore::FValue::FObject ItemObj;
            ItemObj["key"] = GV2RuntimeCore::FValue(TCHAR_TO_UTF8(*TravelAlphaKey.ToString()));
            GV2RuntimeCore::FValue::FObject ItemTextObj;
            ItemTextObj["text_id"] = GV2RuntimeCore::FValue("core:text.command.test.travel_alpha");
            ItemObj["text"] = GV2RuntimeCore::FValue(MoveTemp(ItemTextObj));
            GV2RuntimeCore::FValue::FObject BindingObj;
            BindingObj["command_id"] = GV2RuntimeCore::FValue(TCHAR_TO_UTF8(TravelAlphaCommandId));
            ItemObj["binding"] = GV2RuntimeCore::FValue(MoveTemp(BindingObj));

            CmdsObj["items"] = GV2RuntimeCore::FValue(GV2RuntimeCore::FValue::FArray{
                GV2RuntimeCore::FValue(MoveTemp(ItemObj))
            });

            CommandsField.Value = GV2RuntimeCore::FValue(MoveTemp(CmdsObj));
            Request.Fields.push_back(MoveTemp(CommandsField));
        }

        // 3. TopBar field
        {
            GV2RuntimeCore::FScreenField TopBarField;
            TopBarField.FieldId = "top_bar";
            TopBarField.SchemaId = TCHAR_TO_UTF8(TopBarSchemaId);
            GV2RuntimeCore::FValue::FObject TopBarObj;
            TopBarObj["key"] = GV2RuntimeCore::FValue("top_bar_beta");
            GV2RuntimeCore::FValue::FObject TitleTextObj;
            TitleTextObj["text_id"] = GV2RuntimeCore::FValue("core:text.location.test.beta.title");
            TopBarObj["title"] = GV2RuntimeCore::FValue(MoveTemp(TitleTextObj));
            TopBarField.Value = GV2RuntimeCore::FValue(MoveTemp(TopBarObj));
            Request.Fields.push_back(MoveTemp(TopBarField));
        }

        // 4. PlayerStatus field
        {
            GV2RuntimeCore::FScreenField StatusField;
            StatusField.FieldId = "player_status";
            StatusField.SchemaId = TCHAR_TO_UTF8(PlayerStatusSchemaId);
            GV2RuntimeCore::FValue::FObject StatusObj;
            StatusObj["key"] = GV2RuntimeCore::FValue("status_beta");
            GV2RuntimeCore::FValue::FObject NameTextObj;
            NameTextObj["text_id"] = GV2RuntimeCore::FValue("core:text.character.test_hero.name");
            StatusObj["name"] = GV2RuntimeCore::FValue(MoveTemp(NameTextObj));
            StatusField.Value = GV2RuntimeCore::FValue(MoveTemp(StatusObj));
            Request.Fields.push_back(MoveTemp(StatusField));
        }

        return Request;
    }

    // Scoped RAII owner for synthetic test texts in FallbackTextCatalog
    class FScopedSyntheticFallbackTexts final
    {
    public:
        explicit FScopedSyntheticFallbackTexts(UGV2UiTheme* InTheme)
            : TargetTheme(InTheme)
        {
            if (UGV2UiTheme* Theme = TargetTheme.Get())
            {
                const TArray<TTuple<FString, FText>> EntriesToApply = {
                    { TEXT("core:text.location.test.alpha.title"), FText::FromString(TEXT("Alpha Chamber")) },
                    { TEXT("core:text.location.test.alpha.desc"), FText::FromString(TEXT("You stand in the quiet alpha chamber.")) },
                    { TEXT("core:text.location.test.beta.title"), FText::FromString(TEXT("Beta Chamber")) },
                    { TEXT("core:text.location.test.beta.desc"), FText::FromString(TEXT("You stand in the echoing beta chamber.")) },
                    { TEXT("core:text.command.test.travel_alpha"), FText::FromString(TEXT("Travel to Alpha")) },
                    { TEXT("core:text.command.test.travel_beta"), FText::FromString(TEXT("Travel to Beta")) },
                    { TEXT("core:text.character.test_guide.name"), FText::FromString(TEXT("Synthetic Guide")) },
                    { TEXT("core:text.character.test_hero.name"), FText::FromString(TEXT("Synthetic Hero")) },
                };
                for (const auto& Entry : EntriesToApply)
                {
                    const FString& Key = Entry.Get<0>();
                    const FText& Val = Entry.Get<1>();
                    if (const FText* Existing = Theme->FallbackTextCatalog.Find(Key))
                    {
                        SavedCatalogEntries.Add(Key, *Existing);
                    }
                    else
                    {
                        SavedCatalogEntries.Add(Key, TOptional<FText>());
                    }
                    Theme->FallbackTextCatalog.Add(Key, Val);
                }
            }
        }

        ~FScopedSyntheticFallbackTexts()
        {
            Teardown();
        }

        FScopedSyntheticFallbackTexts(const FScopedSyntheticFallbackTexts&) = delete;
        FScopedSyntheticFallbackTexts& operator=(const FScopedSyntheticFallbackTexts&) = delete;

        void Teardown()
        {
            if (UGV2UiTheme* Theme = TargetTheme.Get())
            {
                for (const auto& Pair : SavedCatalogEntries)
                {
                    if (Pair.Value.IsSet())
                    {
                        Theme->FallbackTextCatalog.Add(Pair.Key, Pair.Value.GetValue());
                    }
                    else
                    {
                        Theme->FallbackTextCatalog.Remove(Pair.Key);
                    }
                }
                SavedCatalogEntries.Empty();
                TargetTheme = nullptr;
            }
        }

    private:
        TWeakObjectPtr<UGV2UiTheme> TargetTheme;
        TMap<FString, TOptional<FText>> SavedCatalogEntries;
    };

    // A test-only prepare context fixture that includes the synthetic schemas under core:
    class FSyntheticPrepareContextFixture final
    {
    public:
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
            };

            std::vector<GV2ContentCore::FDiagnostic> ResolveDiagnostics;
            const std::optional<GV2ContentHostSupport::FResolvedPackageSet> ResolvedSet =
                GV2ContentHostSupport::ResolvePackageSetFromDirectories(PackageRoots, ResolveDiagnostics);
            if (!ResolvedSet.has_value())
            {
                OutError = TEXT("Unable to resolve package set.");
                return false;
            }

            const GV2ContentCore::FBuildResult RepositoryBuild =
                BuildGV2RepositoryFromResolvedPackageSet(*ResolvedSet);
            if (!RepositoryBuild.IsSuccess())
            {
                OutError = TEXT("Unable to build repository snapshot.");
                return false;
            }

            TArray<FGV2SchemaPackageRoot> SchemaRoots;
            SchemaRoots.Reserve(static_cast<int32>(ResolvedSet->OrderedSources.size()) + 1);
            for (const GV2ContentHostSupport::FResolvedPackageSource& Source : ResolvedSet->OrderedSources)
            {
                SchemaRoots.Add(FGV2SchemaPackageRoot{
                    UTF8_TO_TCHAR(Source.Descriptor.GetPackageId().c_str()),
                    UTF8_TO_TCHAR(Source.Root.string().c_str())});
            }
            // Add the synthetic schema directory under core package
            SchemaRoots.Add(FGV2SchemaPackageRoot{
                TEXT("core"),
                GetSyntheticSchemaDir()
            });

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
                    TEXT("Unable to build test presentation snapshot: %s: %s"),
                    UTF8_TO_TCHAR(Fault.Code.c_str()),
                    UTF8_TO_TCHAR(Fault.Message.c_str()));
                return false;
            }

            Context = MakeUnique<FGV2PresentationPrepareContext>(Snapshot);
            return true;
        }

        const FGV2PresentationPrepareContext* Get() const { return Context.Get(); }
        const FGV2SessionContentSnapshot& GetSnapshot() const { return Snapshot; }

    private:
        FGV2SessionContentSnapshot Snapshot;
        TUniquePtr<FGV2PresentationPrepareContext> Context;
    };
}

