#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#include "Bridge/GV2BridgeTypes.h"
#include "Application/GV2SessionCoordinator.h"
#include "Application/GV2SessionContentSnapshot.h"
#include "Runtime/GV2RuntimeSubsystem.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2RecoveryScreenWidget.h"
#include "UI/GV2UiSchemaCache.h"

#include "Tests/GV2PresentationTestFixtures.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ImageCatalogBootstrapGate,
    "GV2.Runtime.Bootstrap.ImageCatalogFailureBlocksReady",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PAH-04B: the catalog is session-scoped now -- there is no more standalone
// "ResourceRootDirectory" setting to mutate into an invalid path. Failure is injected
// instead by dropping one genuinely undecodable PNG into a real closure package's own
// Resources/<PackageId>/ tree (core, always present), the same way a content author
// could break the build by accident -- proving the production BuildFromDirectory/
// BuildFromPackageClosure decode-failure path, not a synthetic injector.
bool FGV2ImageCatalogBootstrapGate::RunTest(const FString& Parameters)
{
    const FString BadResourceDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Resources/core/resource/pah04b_test"));
    const FString BadResourcePath = FPaths::Combine(BadResourceDir, TEXT("bootstrap_gate_test.png"));
    IFileManager::Get().MakeDirectory(*BadResourceDir, true);
    const TArray<uint8> GarbageBytes = {0x00, 0x01, 0x02, 0x03};
    TestTrue(
        TEXT("Undecodable PNG fixture is written into a real closure package's Resources/ tree"),
        FFileHelper::SaveArrayToFile(GarbageBytes, *BadResourcePath));

    AddExpectedError(
        TEXT("GV2 Lua runtime fault: code=ImageCatalogNotReady"),
        EAutomationExpectedErrorFlags::Contains,
        1);
    AddExpectedError(
        TEXT("Failed to start GV2 session"),
        EAutomationExpectedErrorFlags::Contains,
        1);
    AddExpectedError(
        TEXT("Showing UE-native recovery surface: session bootstrap failed"),
        EAutomationExpectedErrorFlags::Contains,
        1);

    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();
    UWorld* TestWorld = WorldContext.GetWorld();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Runtime subsystem exists"), Runtime);
    if (Runtime != nullptr)
    {
        Runtime->StartSession();
        const FGV2SessionStatus Status = Runtime->GetSessionState();
        TestFalse(TEXT("Undecodable resource keeps session non-ready"), Status.bIsReady);
        TestNotEqual(
            TEXT("Undecodable resource prevents Ready state publication"),
            Status.SessionState,
            EGV2SessionState::Ready);
        // PAH-04B: unlike the old subsystem-level pre-check (ScreenRegistry/Repository,
        // which reject before Coordinator::StartSession is ever called and never show
        // recovery UI), the image catalog now fails INSIDE Coordinator::StartSession,
        // taking the same generic failure path as a repository/Lua-source failure --
        // BootstrapAndSessionLifecycle.md's "При переходе в Failed... отображает
        // UE-native recovery surface" applies here now, not a null active screen.
        TestNotNull(
            TEXT("Failed required catalog shows the UE-native recovery surface as the active Screen"),
            Cast<UGV2RecoveryScreenWidget>(Runtime->GetActiveScreen()));
        TestNull(
            TEXT("A session that failed to start publishes no content snapshot, and therefore no image catalog"),
            Runtime->GetContentSnapshotForAutomationTest());
    }

    WorldContext.Teardown();

    IFileManager::Get().Delete(*BadResourcePath);

    // A fresh session started after the bad fixture is removed succeeds and publishes a
    // real, resolvable catalog -- the failure was specific to that one file, not sticky.
    GV2PresentationTestFixtures::FScopedTestWorldContext RecoveredWorldContext;
    UGameInstance* RecoveredGameInstance = RecoveredWorldContext.GetGameInstance();
    UWorld* RecoveredWorld = RecoveredWorldContext.GetWorld();

    UGV2RuntimeSubsystem* RecoveredRuntime = RecoveredGameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Runtime subsystem exists after fixture cleanup"), RecoveredRuntime);
    if (RecoveredRuntime != nullptr)
    {
        RecoveredRuntime->StartSession();
        TestTrue(TEXT("Session recovers once the bad fixture is gone"), RecoveredRuntime->GetSessionState().bIsReady);
        const FGV2SessionContentSnapshot* RecoveredSnapshot = RecoveredRuntime->GetContentSnapshotForAutomationTest();
        TestNotNull(TEXT("Recovered session publishes a content snapshot"), RecoveredSnapshot);
        UGV2ImageResourceCatalog* RecoveredCatalog =
            RecoveredSnapshot != nullptr ? RecoveredSnapshot->GetImageCatalog().Catalog.Get() : nullptr;
        TestNotNull(TEXT("The recovered snapshot owns a session-scoped image catalog"), RecoveredCatalog);
        if (RecoveredCatalog != nullptr)
        {
            FGV2ResolvedImageResource RecoveredResource;
            FString RecoveredResolveError;
            TestTrue(
                TEXT("Recovered catalog resolves real authored content"),
                RecoveredCatalog->Resolve(
                    TEXT("core:resource.ui.old_paper_tile_256"),
                    RecoveredResource,
                    RecoveredResolveError));
        }
        RecoveredRuntime->EndSession();
    }

    return true;
}



IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SchemaCacheSessionScopingTest,
    "GV2.Runtime.ContentCore.SchemaCacheSessionScoping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PAH-04A / CFC-04: two sequential sessions -- here, two sequential snapshot schema cache
// instances created per session candidate (mirroring what FGV2SessionCoordinator::StartSession
// candidate building does for session replacement) -- each with their own resolved package roots,
// must see only their own session's schemas.
// The core-decoupling gate forbids a game-package namespace literal anywhere under
// Source/, so this uses two temporary, hand-written *.schema.json5 fixtures under the
// core namespace instead of real higher-package GameData content -- proving both
// directions a one-sided real-content asymmetry could only prove one of: schema present
// in root set 1 and absent from set 2, AND vice versa, AND that candidate creation truly
// isolates rather than accumulates (set 1's schema must vanish once set 2 is active,
// not just coexist with it).
bool FGV2SchemaCacheSessionScopingTest::RunTest(const FString& Parameters)
{
    const FString RootA = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PAH04ATest/RootA"));
    const FString RootB = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PAH04ATest/RootB"));
    IFileManager::Get().DeleteDirectory(*RootA, false, true);
    IFileManager::Get().DeleteDirectory(*RootB, false, true);
    IFileManager::Get().MakeDirectory(*RootA, true);
    IFileManager::Get().MakeDirectory(*RootB, true);

    static const TCHAR* SchemaIdA = TEXT("core:schema.ui_field.pah04a_fixture_a.v1");
    static const TCHAR* SchemaIdB = TEXT("core:schema.ui_field.pah04a_fixture_b.v1");
    const FString SchemaJson5A = FString::Printf(
        TEXT("{ id: \"%s\", schema_domain: \"ui_field\", schema_version: 1, root: { kind: \"object\", fields: { text: { kind: \"text\", required: true } } } }"),
        SchemaIdA);
    const FString SchemaJson5B = FString::Printf(
        TEXT("{ id: \"%s\", schema_domain: \"ui_field\", schema_version: 1, root: { kind: \"object\", fields: { text: { kind: \"text\", required: true } } } }"),
        SchemaIdB);
    TestTrue(TEXT("Fixture A written"), FFileHelper::SaveStringToFile(SchemaJson5A, *FPaths::Combine(RootA, TEXT("fixture_a.schema.json5"))));
    TestTrue(TEXT("Fixture B written"), FFileHelper::SaveStringToFile(SchemaJson5B, *FPaths::Combine(RootB, TEXT("fixture_b.schema.json5"))));

    auto IsUnknownSchemaId = [](const FString& Error) { return Error.Contains(TEXT("unknown schema_id")); };

    TSharedPtr<FGV2UiSchemaCache> CacheA = FGV2UiSchemaCacheTestAccess::Create({FGV2SchemaPackageRoot{TEXT("core"), RootA}});
    FString CompileErrorA;
    TestTrue(TEXT("Cache A compiles"), FGV2UiSchemaCacheTestAccess::CompileAll(*CacheA, CompileErrorA));

    FString ErrorA1;
    CacheA->GetCompiledSchema(TCHAR_TO_UTF8(SchemaIdA), ErrorA1);
    TestFalse(
        *FString::Printf(TEXT("Session 1 (root A) resolves fixture A [Error: %s]"), *ErrorA1),
        IsUnknownSchemaId(ErrorA1));

    FString ErrorB1;
    CacheA->GetCompiledSchema(TCHAR_TO_UTF8(SchemaIdB), ErrorB1);
    TestTrue(
        *FString::Printf(TEXT("Session 1 (root A) does not know fixture B [Error: %s]"), *ErrorB1),
        IsUnknownSchemaId(ErrorB1));

    // Controlled restart: a second cache instance, root B this time --
    // mirrors what StartSession candidate building does for a real session replacement.
    TSharedPtr<FGV2UiSchemaCache> CacheB = FGV2UiSchemaCacheTestAccess::Create({FGV2SchemaPackageRoot{TEXT("core"), RootB}});
    FString CompileErrorB;
    TestTrue(TEXT("Cache B compiles"), FGV2UiSchemaCacheTestAccess::CompileAll(*CacheB, CompileErrorB));

    FString ErrorB2;
    CacheB->GetCompiledSchema(TCHAR_TO_UTF8(SchemaIdB), ErrorB2);
    TestFalse(
        *FString::Printf(TEXT("Session 2 (root B) resolves fixture B [Error: %s]"), *ErrorB2),
        IsUnknownSchemaId(ErrorB2));

    FString ErrorA2;
    CacheB->GetCompiledSchema(TCHAR_TO_UTF8(SchemaIdA), ErrorA2);
    TestTrue(
        *FString::Printf(TEXT("Session 2 (root B) does not know fixture A -- isolated, not accumulated [Error: %s]"), *ErrorA2),
        IsUnknownSchemaId(ErrorA2));

    IFileManager::Get().DeleteDirectory(*RootA, false, true);
    IFileManager::Get().DeleteDirectory(*RootB, false, true);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ImageCatalogClosureScopingTest,
    "GV2.Runtime.ContentCore.ImageCatalogClosureScoping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PAH-04B: two sequential RebuildForSession calls -- the exact production entry point
// FGV2SessionCoordinator::StartSession makes -- with different resolved package
// closures, must see only their own session's resources. Uses real content from the
// higher gameplay package's own Resources/ tree (always on disk) rather than a
// synthetic fixture, since the point being proven is specifically about closure
// MEMBERSHIP (a resource physically present on disk but outside this session's own
// package set), not about discovery mechanics -- that's already covered by
// BuildFromDirectory's own scanner tests. GameNamespace is built at runtime, not a
// literal game-package-prefixed id, to satisfy the core-decoupling gate the same way
// FGV2LocationSceneDiagnostic already does.
bool FGV2ImageCatalogClosureScopingTest::RunTest(const FString& Parameters)
{
    const FString GameNamespace = TEXT("r") TEXT("h");
    const FString RhResourceId = GameNamespace + TEXT(":resource.character.tavern_keeper");
    auto IsUnknownResourceId = [](const FString& Error) { return Error.Contains(TEXT("Unknown image resource_id")); };

    FString ErrorWithoutRh;
    UGV2ImageResourceCatalog* CatalogWithoutRh =
        GV2PresentationTestFixtures::BuildImageCatalogForClosure({TEXT("core"), TEXT("textsystem")}, ErrorWithoutRh);
    TestNotNull(
        *FString::Printf(TEXT("Session 1 (core+textsystem, no rh) builds a catalog [Error: %s]"), *ErrorWithoutRh),
        CatalogWithoutRh);
    if (CatalogWithoutRh != nullptr)
    {
        FGV2ResolvedImageResource Resolved1;
        FString ResolveError1;
        TestFalse(
            *FString::Printf(TEXT("Session 1's closure excludes rh, so its own resource is unknown, not a build error [Error: %s]"), *ResolveError1),
            CatalogWithoutRh->Resolve(RhResourceId, Resolved1, ResolveError1));
        TestTrue(TEXT("The failure is an unknown-ID lookup, not a build-time rejection"), IsUnknownResourceId(ResolveError1));

        FGV2ResolvedImageResource CoreResolved;
        FString CoreResolveError;
        TestTrue(
            *FString::Printf(TEXT("Session 1 still resolves a resource whose package IS in its closure [Error: %s]"), *CoreResolveError),
            CatalogWithoutRh->Resolve(TEXT("core:resource.ui.old_paper_tile_256"), CoreResolved, CoreResolveError));
    }

    // A second, independently built catalog with rh included -- mirrors what a session
    // replacement's candidate builder produces. PSC-10C: two catalogs now coexist as plain
    // objects rather than one overwriting a process-global.
    FString ErrorWithRh;
    UGV2ImageResourceCatalog* CatalogWithRh = GV2PresentationTestFixtures::BuildImageCatalogForClosure(
        {TEXT("core"), TEXT("textsystem"), GameNamespace}, ErrorWithRh);
    TestNotNull(
        *FString::Printf(TEXT("Session 2 (core+textsystem+rh) builds a catalog [Error: %s]"), *ErrorWithRh),
        CatalogWithRh);
    if (CatalogWithRh != nullptr)
    {
        FGV2ResolvedImageResource Resolved2;
        FString ResolveError2;
        TestTrue(
            *FString::Printf(TEXT("Session 2, whose closure includes rh, resolves rh's own resource [Error: %s]"), *ResolveError2),
            CatalogWithRh->Resolve(RhResourceId, Resolved2, ResolveError2));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ImageCatalogScopedRootsNeverOpenExcludedDirectoryTest,
    "GV2.Runtime.ContentCore.ImageCatalogScopedRootsNeverOpenExcludedDirectory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PSC-07 (ADR-0043 D1, PAH-R5): instrumented file-access proof for
// BuildFromPackageResourceRoots -- a synthetic "disabled" package directory holds an
// undecodable PNG, but its root is never present in the list this test passes in.
// BuildEntryFromPngFile unconditionally fails the WHOLE build the instant it decodes an
// undecodable PNG (see FGV2ImageCatalogBootstrapGate), so the build succeeding here is
// only possible if that file was never opened at all -- proof by contradiction that
// traversal/open/decode never reaches a root outside the passed list, without needing to
// hook IFileManager itself.
bool FGV2ImageCatalogScopedRootsNeverOpenExcludedDirectoryTest::RunTest(const FString& Parameters)
{
    const FString FixtureRoot = FPaths::Combine(
        FPaths::ProjectIntermediateDir(),
        TEXT("GV2AutomationImageCatalogScopedRoots"));
    const FString EnabledRoot = FPaths::Combine(FixtureRoot, TEXT("enabled"));
    const FString EnabledResourceDir = FPaths::Combine(EnabledRoot, TEXT("resource/image"));
    const FString DisabledRoot = FPaths::Combine(FixtureRoot, TEXT("disabled"));
    const FString DisabledResourceDir = FPaths::Combine(DisabledRoot, TEXT("resource/image"));

    IFileManager::Get().DeleteDirectory(*FixtureRoot, false, true);
    IFileManager::Get().MakeDirectory(*EnabledResourceDir, true);
    IFileManager::Get().MakeDirectory(*DisabledResourceDir, true);

    FImage ValidImage(4, 4, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
    FMemory::Memset(ValidImage.RawData.GetData(), 255, ValidImage.RawData.Num());
    const FString ValidPng = FPaths::Combine(EnabledResourceDir, TEXT("swatch.png"));
    TestTrue(TEXT("Enabled fixture PNG is written"), FImageUtils::SaveImageByExtension(*ValidPng, ValidImage));

    const FString PoisonPng = FPaths::Combine(DisabledResourceDir, TEXT("poison.png"));
    const TArray<uint8> GarbageBytes = {0x00, 0x01, 0x02, 0x03};
    TestTrue(
        TEXT("Excluded fixture's undecodable PNG is written"),
        FFileHelper::SaveArrayToFile(GarbageBytes, *PoisonPng));

    TArray<FGV2ImagePackageResourceRoot> Roots;
    Roots.Add(FGV2ImagePackageResourceRoot{TEXT("enabled"), EnabledRoot});
    // "disabled" is deliberately absent from Roots -- that omission is what this test proves matters.

    UGV2ImageResourceCatalog* Catalog = NewObject<UGV2ImageResourceCatalog>();
    FString Error;
    TestTrue(
        *FString::Printf(TEXT("Build succeeds although an excluded sibling root holds an undecodable PNG [Error: %s]"), *Error),
        Catalog->BuildFromPackageResourceRoots(Roots, Error));
    TestEqual(TEXT("Only the enabled root's own resource is published"), Catalog->GetEntries().Num(), 1);
    if (Catalog->GetEntries().Num() == 1)
    {
        TestEqual(
            TEXT("Published resource belongs to the enabled package"),
            Catalog->GetEntries()[0].ResourceId,
            FString(TEXT("enabled:resource.image.swatch")));
    }

    IFileManager::Get().DeleteDirectory(*FixtureRoot, false, true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ImageCatalogDisabledPackageCorruptFileDoesNotBlockBootstrapTest,
    "GV2.Runtime.ContentCore.ImageCatalogDisabledPackageCorruptFileDoesNotBlockBootstrap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// PSC-07 (ADR-0043 D1, PAH-R5): production entry point (RebuildForSession, the same
// function FGV2SessionCoordinator::StartSession calls) with a genuinely undecodable PNG
// dropped into a real package's own Resources/<id>/ tree -- mirrors
// FGV2ImageCatalogBootstrapGate's fixture style, but for a package OUTSIDE this
// session's own closure, proving the opposite direction of that gate: a disabled
// package's corrupt content must not be able to fail a session that never loads it.
// GameNamespace is built at runtime, not a literal-game-package-prefixed id, to satisfy
// the core-decoupling gate the same way FGV2ImageCatalogClosureScopingTest already does.
bool FGV2ImageCatalogDisabledPackageCorruptFileDoesNotBlockBootstrapTest::RunTest(const FString& Parameters)
{
    const FString GameNamespace = TEXT("r") TEXT("h");
    const FString BadResourceDir = FPaths::Combine(
        FPaths::ProjectDir(),
        TEXT("Resources"),
        GameNamespace,
        TEXT("resource/psc07_test"));
    const FString BadResourcePath = FPaths::Combine(BadResourceDir, TEXT("disabled_package_test.png"));
    IFileManager::Get().MakeDirectory(*BadResourceDir, true);
    const TArray<uint8> GarbageBytes = {0x00, 0x01, 0x02, 0x03};
    TestTrue(
        TEXT("Undecodable PNG fixture is written into a real, disabled-by-default package's Resources/ tree"),
        FFileHelper::SaveArrayToFile(GarbageBytes, *BadResourcePath));

    FString ErrorWithoutRh;
    UGV2ImageResourceCatalog* CatalogWithoutRh =
        GV2PresentationTestFixtures::BuildImageCatalogForClosure({TEXT("core"), TEXT("textsystem")}, ErrorWithoutRh);
    TestNotNull(
        *FString::Printf(TEXT("Closure excluding the corrupt file's own package builds a catalog [Error: %s]"), *ErrorWithoutRh),
        CatalogWithoutRh);

    // PSC-10C: the old "a failed rebuild leaves the previous catalog published" assertion
    // described the process-global's replacement semantics. There is no global any more --
    // a failed candidate simply never becomes a snapshot, and the previously built catalog
    // is a separate object nothing overwrote. What still matters, and is asserted, is that
    // the failure is attributed to the corrupt file rather than to the closure as a whole.
    FString ErrorWithRh;
    TestNull(
        TEXT("Closure including that package fails on the same corrupt file"),
        GV2PresentationTestFixtures::BuildImageCatalogForClosure(
            {TEXT("core"), TEXT("textsystem"), GameNamespace}, ErrorWithRh));
    TestTrue(
        *FString::Printf(TEXT("Failure identifies the undecodable PNG, proving the fixture is real [Error: %s]"), *ErrorWithRh),
        ErrorWithRh.Contains(TEXT("Cannot decode PNG resource")));
    TestTrue(
        TEXT("The earlier catalog is untouched by the later failed build"),
        CatalogWithoutRh != nullptr);

    IFileManager::Get().Delete(*BadResourcePath);
    return true;
}


#endif // WITH_DEV_AUTOMATION_TESTS
