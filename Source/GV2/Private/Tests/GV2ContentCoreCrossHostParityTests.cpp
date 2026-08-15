#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Application/GV2FilesystemContentSourceProvider.h"

#include "Misc/Paths.h"

// PCC-38: cross-host parity. The `valid/core` fixture package must produce
// the exact same normalized content hash regardless of which host built it.
// This constant is independently reproduced by:
//   - `gv2-content validate|hash Tests/Fixtures/PortableContentCore/valid/core`
// TAS-06/07: this fixture corpus is frozen and independent of GameData/core
// (Tests/Fixtures/PortableContentCore/README.md); its pinned hash lives as
// a sibling of the corpus (PCC-01 forbids "expected*" files inside it).
// `gv2-headless` does not reproduce this constant: it runs against the live
// GameData/core repository, which pins no content hash of its own.
// All three hosts route through GV2ContentCore::BuildRepository() with a
// discovery convention (self-describing schema resources under
// definitions/*.json5 + schemas/*.json5) that is identical across the CLI
// (Tools/Content/Source/main.cpp), headless (Headless/Source/main.cpp) and this UE
// adapter (Source/GV2/Private/Application/GV2FilesystemContentSourceProvider.cpp).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreCrossHostParityTest,
    "GV2.Runtime.ContentCore.CrossHostParity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreCrossHostParityTest::RunTest(const FString& Parameters)
{
    const FString PackageRoot = FPaths::Combine(
        FPaths::ProjectDir(), TEXT("Tests/Fixtures/PortableContentCore/valid/core"));

    const GV2ContentCore::FBuildResult Result = BuildGV2RepositoryFromDirectory(PackageRoot);
    if (!TestTrue(TEXT("UE host builds the shared core fixture"), Result.IsSuccess()))
    {
        return false;
    }

    const FString ExpectedHashPath = FPaths::Combine(
        FPaths::ProjectDir(), TEXT("Tests/Fixtures/expected_core_content_hash.txt"));
    FString ExpectedHash;
    if (!TestTrue(
            TEXT("UE host can read expected_content_hash.txt"),
            FFileHelper::LoadFileToString(ExpectedHash, *ExpectedHashPath)))
    {
        return false;
    }
    ExpectedHash.TrimStartAndEndInline();

    const FString ContentHash = UTF8_TO_TCHAR(
        Result.GetCandidate().GetReadHandle().GetContentHash().c_str());
    TestEqual(
        TEXT("UE-built content hash matches expected_content_hash.txt for valid/core"),
        ContentHash,
        ExpectedHash);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
