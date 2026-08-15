#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Application/GV2FilesystemContentSourceProvider.h"
#include "GV2RuntimeCore/GV2RunDigest.h"
#include "GV2RuntimeCore/GV2RunManifest.h"
#include "GV2RuntimeCore/GV2RunReplay.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2RuntimeCoreCrossHostDigestTest,
    "GV2.Runtime.Session.CrossHostDigestParity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2RuntimeCoreCrossHostDigestTest::RunTest(const FString& Parameters)
{
    // 1. Build the repository the golden run is pinned against. TAS-08: the
    // golden manifest/digest are generated from the frozen test corpus
    // (Tests/Fixtures/PortableContentCore/valid/core), not the live
    // GameData/core, so adding gameplay content never perturbs this test.
    const FString CoreRoot = FPaths::Combine(
        FPaths::ProjectDir(), TEXT("Tests/Fixtures/PortableContentCore/valid/core"));
    const GV2ContentCore::FBuildResult RepoBuild = BuildGV2RepositoryFromDirectory(CoreRoot);
    if (!TestTrue(TEXT("UE host builds the frozen test corpus repository"), RepoBuild.IsSuccess()))
    {
        return false;
    }
    const GV2ContentCore::FRepositoryReadHandle RepoHandle = RepoBuild.GetCandidate().GetReadHandle();

    // 2. Load Scripts/ sources
    std::vector<GV2RuntimeCore::FRuntimeSource> RuntimeSources;
    FString ScriptsDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Scripts"));
    FPaths::NormalizeDirectoryName(ScriptsDir);
    const FString ScriptsPrefix = ScriptsDir + TEXT("/");
    TArray<FString> SourceFiles;
    IFileManager::Get().FindFilesRecursive(SourceFiles, *ScriptsDir, TEXT("*.lua"), true, false, false);
    SourceFiles.Sort();
    for (const FString& FullPath : SourceFiles)
    {
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *FullPath))
        {
            TestTrue(TEXT("Load lua script"), false);
            return false;
        }
        FString Norm = FullPath;
        FPaths::NormalizeFilename(Norm);
        const FString Rel = Norm.RightChop(ScriptsPrefix.Len());
        const FTCHARToUTF8 Utf8(*Text);
        RuntimeSources.push_back({
            "@Scripts/" + std::string(TCHAR_TO_UTF8(*Rel)),
            std::string(Utf8.Get(), Utf8.Length())});
    }

    // 3. Load golden manifest fixture
    const FString GoldenManifestPath = FPaths::Combine(
        FPaths::ProjectDir(), TEXT("Tests/Fixtures/GoldenRuns/golden_headless_10_seed_42.manifest.json5"));
    FString GoldenManifestJson;
    if (!TestTrue(TEXT("Read golden manifest fixture"), FFileHelper::LoadFileToString(GoldenManifestJson, *GoldenManifestPath)))
    {
        return false;
    }
    const FTCHARToUTF8 ManifestUtf8(*GoldenManifestJson);
    GV2RuntimeCore::FRunManifest Manifest;
    std::string ManifestError;
    if (!TestTrue(
            FString::Printf(TEXT("Deserialize golden manifest: %s"), UTF8_TO_TCHAR(ManifestError.c_str())),
            GV2RuntimeCore::DeserializeRunManifest(
                std::string_view(ManifestUtf8.Get(), ManifestUtf8.Length()), Manifest, ManifestError)))
    {
        return false;
    }

    // 4. Load golden digest fixture
    const FString GoldenDigestPath = FPaths::Combine(
        FPaths::ProjectDir(), TEXT("Tests/Fixtures/GoldenRuns/golden_headless_10_seed_42.digest.json5"));
    FString GoldenDigestJson;
    if (!TestTrue(TEXT("Read golden digest fixture"), FFileHelper::LoadFileToString(GoldenDigestJson, *GoldenDigestPath)))
    {
        return false;
    }
    const FTCHARToUTF8 DigestUtf8(*GoldenDigestJson);
    GV2RuntimeCore::FRunDigest GoldenDigest;
    std::string DigestError;
    if (!TestTrue(
            FString::Printf(TEXT("Deserialize golden digest: %s"), UTF8_TO_TCHAR(DigestError.c_str())),
            GV2RuntimeCore::DeserializeRunDigest(
                std::string_view(DigestUtf8.Get(), DigestUtf8.Length()), GoldenDigest, DigestError)))
    {
        return false;
    }

    // 5. Replay in UE
    GV2RuntimeCore::FRunResult RunResult;
    GV2RuntimeCore::FRuntimeFault Fault;
    const bool bReplayOk = GV2RuntimeCore::ReplayRunManifest(Manifest, RepoHandle, RuntimeSources, RunResult, Fault);
    if (!TestTrue(FString::Printf(TEXT("ReplayRunManifest in UE succeeds: code=%s message=%s"),
        UTF8_TO_TCHAR(Fault.Code.c_str()), UTF8_TO_TCHAR(Fault.Message.c_str())), bReplayOk))
    {
        return false;
    }

    TestTrue(TEXT("RunResult success is true"), RunResult.bSuccess);
    TestEqual(TEXT("RunResult executed commands count matches manifest"), RunResult.ExecutedCommandsCount, static_cast<std::uint64_t>(Manifest.AcceptedCommands.size()));

    // 6. Verify computed digest against golden digest
    const GV2RuntimeCore::FRunDigest ComputedDigest = GV2RuntimeCore::ComputeRunDigest(Manifest, RunResult);

    TestTrue(
        FString::Printf(TEXT("UE replay digest matches golden digest: actual=%s golden=%s repo_hash=%s executed=%llu"),
            UTF8_TO_TCHAR(ComputedDigest.DigestHash.c_str()),
            UTF8_TO_TCHAR(GoldenDigest.DigestHash.c_str()),
            UTF8_TO_TCHAR(RepoHandle.GetContentHash().c_str()),
            static_cast<unsigned long long>(RunResult.ExecutedCommandsCount)),
        ComputedDigest.DigestHash == GoldenDigest.DigestHash);

    TestTrue(TEXT("Computed digest struct matches golden digest struct"), ComputedDigest == GoldenDigest);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
