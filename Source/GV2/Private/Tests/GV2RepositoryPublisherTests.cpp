#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Application/GV2FilesystemContentSourceProvider.h"
#include "Application/GV2RepositoryPublisher.h"

#include "Misc/Paths.h"

namespace
{
FString FixtureRoot(const TCHAR* RelativePath)
{
    return FPaths::Combine(
        FPaths::ProjectDir(), TEXT("Tests/Fixtures/PortableContentCore"), RelativePath);
}
}

// PCC-37: atomic publication - Game-Thread-only, same-hash candidate keeps
// the current handle/version unchanged, failed/non-publishable candidate
// never replaces current.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2RepositoryPublisherAtomicPublicationTest,
    "GV2.Runtime.ContentCore.RepositoryPublisherAtomicPublication",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2RepositoryPublisherAtomicPublicationTest::RunTest(const FString& Parameters)
{
    FGV2RepositoryPublisher Publisher;
    TestFalse(TEXT("Publisher starts with no current repository"), Publisher.HasCurrent());
    TestEqual(TEXT("Publisher starts at version 0"), Publisher.GetVersion(), int64(0));

    // A failed candidate never becomes current.
    TestFalse(
        TEXT("Missing root candidate is rejected"),
        Publisher.PublishCandidate(BuildGV2RepositoryFromDirectory(FixtureRoot(TEXT("does_not_exist")))));
    TestFalse(TEXT("Publisher still has no current repository"), Publisher.HasCurrent());
    TestEqual(TEXT("Version is unchanged after a failed candidate"), Publisher.GetVersion(), int64(0));

    // First successful publish bumps the version and becomes current.
    TestTrue(
        TEXT("Valid core candidate publishes"),
        Publisher.PublishCandidate(BuildGV2RepositoryFromDirectory(FixtureRoot(TEXT("valid/core")))));
    TestTrue(TEXT("Publisher now has a current repository"), Publisher.HasCurrent());
    TestEqual(TEXT("First publish bumps version to 1"), Publisher.GetVersion(), int64(1));
    const FString FirstHash = UTF8_TO_TCHAR(Publisher.GetCurrent().GetContentHash().c_str());
    TestTrue(TEXT("Content hash is a non-empty hex string"), !FirstHash.IsEmpty());

    // Re-publishing an identical candidate keeps the same hash and does not
    // bump the version (GameDataRepositoryContract.md "Snapshot identity").
    TestTrue(
        TEXT("Re-publishing the same content succeeds"),
        Publisher.PublishCandidate(BuildGV2RepositoryFromDirectory(FixtureRoot(TEXT("valid/core")))));
    TestEqual(TEXT("Same-hash candidate does not bump version"), Publisher.GetVersion(), int64(1));
    TestEqual(
        TEXT("Same-hash candidate keeps the same content hash"),
        UTF8_TO_TCHAR(Publisher.GetCurrent().GetContentHash().c_str()),
        FirstHash);

    // A subsequent failed/invalid candidate must not disturb the still-valid current snapshot.
    TestFalse(
        TEXT("Invalid candidate after a valid publish is rejected"),
        Publisher.PublishCandidate(
            BuildGV2RepositoryFromDirectory(FixtureRoot(TEXT("invalid/duplicate_key/core")))));
    TestTrue(TEXT("Publisher retains its current repository"), Publisher.HasCurrent());
    TestEqual(TEXT("Version stays at 1 after a rejected candidate"), Publisher.GetVersion(), int64(1));
    TestEqual(
        TEXT("Current content hash is unchanged after a rejected candidate"),
        UTF8_TO_TCHAR(Publisher.GetCurrent().GetContentHash().c_str()),
        FirstHash);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
