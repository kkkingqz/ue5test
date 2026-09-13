#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

#include "Application/GV2SessionCoordinator.h"
#include "Application/GV2FilesystemContentSourceProvider.h"
#include "Bridge/GV2BridgeTypes.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"
#include "GV2RuntimeCore/GV2RunManifest.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

#include <vector>
#include <string>

namespace
{

std::vector<GV2RuntimeCore::FRuntimeSource> LoadMinimalCoreSources()
{
    std::vector<GV2RuntimeCore::FRuntimeSource> Sources;
    FString ScriptsDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Scripts"));
    FPaths::NormalizeDirectoryName(ScriptsDirectory);
    const FString ScriptsPrefix = ScriptsDirectory + TEXT("/");
    TArray<FString> SourceFiles;
    IFileManager::Get().FindFilesRecursive(
        SourceFiles,
        *ScriptsDirectory,
        TEXT("*.lua"),
        true,
        false,
        false);
    SourceFiles.Sort();
    for (const FString& FullPath : SourceFiles)
    {
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *FullPath))
        {
            return {};
        }
        FString NormalizedFullPath = FullPath;
        FPaths::NormalizeFilename(NormalizedFullPath);
        if (!NormalizedFullPath.StartsWith(ScriptsPrefix, ESearchCase::CaseSensitive))
        {
            return {};
        }
        const FString RelativePath = NormalizedFullPath.RightChop(ScriptsPrefix.Len());
        const FTCHARToUTF8 Utf8(*Text);
        Sources.push_back({
            "@core/" + std::string(TCHAR_TO_UTF8(*RelativePath)),
            std::string(Utf8.Get(), Utf8.Length())});
    }
    return Sources;
}

std::vector<GV2RuntimeCore::FRuntimeSource> LoadAllRuntimeSources(
    const GV2ContentHostSupport::FResolvedPackageSet& ResolvedPackageSet)
{
    std::vector<GV2RuntimeCore::FRuntimeSource> Sources = LoadMinimalCoreSources();
    for (const GV2ContentHostSupport::FResolvedPackageSource& Source : ResolvedPackageSet.OrderedSources)
    {
        if (Source.Descriptor.GetPackageId() == "core")
        {
            continue;
        }
        auto PkgSources = GV2ContentHostSupport::DiscoverPackageScripts(Source.Root, Source.Descriptor.GetPackageId());
        for (auto& Src : PkgSources)
        {
            Sources.push_back(GV2RuntimeCore::FRuntimeSource{std::move(Src.Name), std::move(Src.Text)});
        }
    }
    return Sources;
}

uint32 SampleSessionPrng(
    FAutomationTestBase& Test,
    GV2RuntimeCore::FRuntimeSession& Session,
    const FString& ExpectedSeedHex)
{
    std::vector<GV2RuntimeCore::FLuaSpecCaseResult> Results;
    GV2RuntimeCore::FRuntimeFault Fault;
    const std::string ExpectedSeedUtf8 = TCHAR_TO_UTF8(*ExpectedSeedHex);
    const std::string SpecSource = std::string(R"lua(
return {
    sample = function()
        local random = require("core:module.runtime.random")
        local seed = game.state.meta.seed_hex
        assert(type(seed) == "string" and #seed == 16, "canonical state meta.seed_hex must be 16-character string")
        assert(game.runtime.seed_hex == seed, "runtime.seed_hex must match state.meta.seed_hex")
        assert(seed == ")lua") + ExpectedSeedUtf8 + R"lua(", "state seed must match expected seed: " .. tostring(seed))
        local stream = random.derive_stream(seed, "core:random_stream.gameplay")
        local s0 = tonumber(stream.s0, 16)
        local s1 = tonumber(stream.s1, 16)
        local s2 = tonumber(stream.s2, 16)
        local s3 = tonumber(stream.s3, 16)
        local val = (random.rotl32((s1 * 5) & 0xffffffff, 7) * 9) & 0xffffffff
        error("PRNG_VAL:" .. tostring(val))
    end
}
)lua";

    Session.RunLuaSpec("@test_prng_sample", SpecSource, Results, Fault);
    if (Results.empty())
    {
        Test.AddError(FString::Printf(TEXT("RunLuaSpec returned no results: %s"), UTF8_TO_TCHAR(Fault.Message.c_str())));
        return 0;
    }

    const FString ErrorMessage = UTF8_TO_TCHAR(Results[0].ErrorMessage.c_str());
    const FString Prefix = TEXT("PRNG_VAL:");
    const int32 PrefixIdx = ErrorMessage.Find(Prefix);
    if (PrefixIdx == INDEX_NONE)
    {
        Test.AddError(FString::Printf(TEXT("Could not extract PRNG value from spec error: %s"), *ErrorMessage));
        return 0;
    }

    const FString NumStr = ErrorMessage.Mid(PrefixIdx + Prefix.Len());
    return static_cast<uint32>(FCString::Strtoui64(*NumStr, nullptr, 10));
}

} // anonymous namespace

// 1. Two NewGame sessions produce distinct seeds and distinct initial PRNG values
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionNewGameSeedsAndPrngTest,
    "GV2.Runtime.Session.NewGameSessionsProduceDistinctSeedsAndPrng",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionNewGameSeedsAndPrngTest::RunTest(const FString& Parameters)
{
    struct FSampleOverrideScope
    {
        FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = true; }
        ~FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = false; }
    } Scope;

    FGV2SessionCoordinator Coordinator;
    Coordinator.SetDocumentSink([](const FGV2UiDocumentViewModel&, const FGV2PresentationPrepareContext&) -> bool { return true; });

    const FString CorePackageRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData/core"));
    const GV2ContentCore::FBuildResult RepoBuild = BuildGV2RepositoryFromDirectory(CorePackageRoot);
    GV2ContentCore::FRepositoryReadHandle ReadHandle;
    if (RepoBuild.IsSuccess())
    {
        ReadHandle = RepoBuild.GetCandidate().GetReadHandle();
    }
    TestTrue(TEXT("RepoBuild succeeded"), ReadHandle.IsValid());

    const FString GameDataDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData"));
    const std::vector<std::filesystem::path> Roots = {
        std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, TEXT("core")))),
        std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, TEXT("textsystem")))),
        std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, TEXT("sample")))),
    };
    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    const std::optional<GV2ContentHostSupport::FResolvedPackageSet> Resolved =
        GV2ContentHostSupport::ResolvePackageSetFromDirectories(Roots, Diagnostics);
    TestTrue(TEXT("ResolvedPackageSet is valid"), Resolved.has_value());

    // Session 1: fresh entropy seed
    FSessionStartDescriptor Desc1;
    Desc1.Mode = ESessionStartMode::NewGame;
    Desc1.RepositoryVersion = TEXT("1");
    Desc1.RepositoryContentHash = UTF8_TO_TCHAR(ReadHandle.GetContentHash().c_str());
    Desc1.SeedHex = FSessionStartDescriptor::GenerateFreshSeedHex();

    const uint64 Op1 = Coordinator.RequestSession(Desc1, ReadHandle, 1, *Resolved);
    TestEqual(TEXT("Session 1 outcome is Completed"),
        *Coordinator.GetSessionOperationOutcome(Op1), ESessionOperationOutcome::Completed);
    TestEqual(TEXT("Session 1 active seed matches descriptor"), Coordinator.GetActiveSeedHex(), Desc1.SeedHex);

    const uint32 Val1 = SampleSessionPrng(*this, Coordinator.GetRuntimeSession(), Desc1.SeedHex);
    TestTrue(TEXT("Session 1 PRNG value sampled"), Val1 > 0);

    // End Session 1
    Coordinator.EndSession(EGV2SessionState::Destroyed);

    // Session 2: second fresh entropy seed
    FSessionStartDescriptor Desc2;
    Desc2.Mode = ESessionStartMode::NewGame;
    Desc2.RepositoryVersion = TEXT("1");
    Desc2.RepositoryContentHash = UTF8_TO_TCHAR(ReadHandle.GetContentHash().c_str());
    Desc2.SeedHex = FSessionStartDescriptor::GenerateFreshSeedHex();

    const uint64 Op2 = Coordinator.RequestSession(Desc2, ReadHandle, 2, *Resolved);
    TestEqual(TEXT("Session 2 outcome is Completed"),
        *Coordinator.GetSessionOperationOutcome(Op2), ESessionOperationOutcome::Completed);
    TestEqual(TEXT("Session 2 active seed matches descriptor"), Coordinator.GetActiveSeedHex(), Desc2.SeedHex);

    const uint32 Val2 = SampleSessionPrng(*this, Coordinator.GetRuntimeSession(), Desc2.SeedHex);
    TestTrue(TEXT("Session 2 PRNG value sampled"), Val2 > 0);

    Coordinator.EndSession(EGV2SessionState::Destroyed);

    // Invariants: distinct fresh seeds and distinct initial PRNG values
    TestNotEqual(TEXT("Fresh seeds across NewGame sessions are distinct"), Desc1.SeedHex, Desc2.SeedHex);
    TestNotEqual(TEXT("First PRNG stream values across distinct seeds are distinct"), Val1, Val2);

    return true;
}

// 2. Manifest replay with recorded seed reproduces session PRNG and canonical state
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionManifestReplayReproducesSessionPrngTest,
    "GV2.Runtime.Session.ManifestReplayReproducesSessionPrng",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionManifestReplayReproducesSessionPrngTest::RunTest(const FString& Parameters)
{
    struct FSampleOverrideScope
    {
        FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = true; }
        ~FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = false; }
    } Scope;

    FGV2SessionCoordinator Coordinator;
    Coordinator.SetDocumentSink([](const FGV2UiDocumentViewModel&, const FGV2PresentationPrepareContext&) -> bool { return true; });

    const FString CorePackageRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData/core"));
    const GV2ContentCore::FBuildResult RepoBuild = BuildGV2RepositoryFromDirectory(CorePackageRoot);
    GV2ContentCore::FRepositoryReadHandle ReadHandle;
    if (RepoBuild.IsSuccess())
    {
        ReadHandle = RepoBuild.GetCandidate().GetReadHandle();
    }
    TestTrue(TEXT("RepoBuild succeeded"), ReadHandle.IsValid());

    const FString GameDataDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData"));
    const std::vector<std::filesystem::path> Roots = {
        std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, TEXT("core")))),
        std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, TEXT("textsystem")))),
        std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, TEXT("sample")))),
    };
    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    const std::optional<GV2ContentHostSupport::FResolvedPackageSet> Resolved =
        GV2ContentHostSupport::ResolvePackageSetFromDirectories(Roots, Diagnostics);
    TestTrue(TEXT("ResolvedPackageSet is valid"), Resolved.has_value());

    const FString FixedSeedHex = TEXT("0123456789abcdef");

    FSessionStartDescriptor Desc;
    Desc.Mode = ESessionStartMode::NewGame;
    Desc.RepositoryVersion = TEXT("1");
    Desc.RepositoryContentHash = UTF8_TO_TCHAR(ReadHandle.GetContentHash().c_str());
    Desc.SeedHex = FixedSeedHex;

    const uint64 Op = Coordinator.RequestSession(Desc, ReadHandle, 1, *Resolved);
    TestEqual(TEXT("Session outcome is Completed"),
        *Coordinator.GetSessionOperationOutcome(Op), ESessionOperationOutcome::Completed);

    const uint32 OriginalPrngVal = SampleSessionPrng(*this, Coordinator.GetRuntimeSession(), FixedSeedHex);
    const std::string OriginalStateHash = Coordinator.GetRuntimeSession().GetCanonicalStateHash();

    Coordinator.EndSession(EGV2SessionState::Destroyed);

    // Replay the session with the exact same seed in a fresh runtime session
    const std::vector<GV2RuntimeCore::FRuntimeSource> Sources = LoadAllRuntimeSources(*Resolved);
    TestTrue(TEXT("Core and package sources loaded"), !Sources.empty());

    GV2RuntimeCore::FSessionStartInputs ReplayInputs;
    ReplayInputs.SessionGeneration = 1;
    ReplayInputs.SeedHex = TCHAR_TO_UTF8(*FixedSeedHex);
    ReplayInputs.Mode = "NewGame";
    ReplayInputs.RepositoryContentHash = ReadHandle.GetContentHash();

    GV2RuntimeCore::FRuntimeSession ReplaySession;
    GV2RuntimeCore::FRuntimeFault ReplayFault;
    const bool bReplayStarted = ReplaySession.Start(ReplayInputs, ReadHandle, Sources, ReplayFault);
    TestTrue(TEXT("Replay session started"), bReplayStarted);

    const uint32 ReplayPrngVal = SampleSessionPrng(*this, ReplaySession, FixedSeedHex);
    const std::string ReplayStateHash = ReplaySession.GetCanonicalStateHash();

    ReplaySession.Stop();

    TestEqual(TEXT("Replay reproduces exact same initial PRNG value"), ReplayPrngVal, OriginalPrngVal);
    TestEqual(TEXT("Replay reproduces exact same canonical state hash"),
        FString(UTF8_TO_TCHAR(ReplayStateHash.c_str())),
        FString(UTF8_TO_TCHAR(OriginalStateHash.c_str())));

    // Also replay in a fresh coordinator session
    FGV2SessionCoordinator ReplayCoordinator;
    ReplayCoordinator.SetDocumentSink([](const FGV2UiDocumentViewModel&, const FGV2PresentationPrepareContext&) -> bool { return true; });
    const uint64 ReplayOp = ReplayCoordinator.RequestSession(Desc, ReadHandle, 2, *Resolved);
    TestEqual(TEXT("Coordinator replay outcome is Completed"),
        *ReplayCoordinator.GetSessionOperationOutcome(ReplayOp), ESessionOperationOutcome::Completed);
    const uint32 CoordReplayVal = SampleSessionPrng(*this, ReplayCoordinator.GetRuntimeSession(), FixedSeedHex);
    const std::string CoordReplayStateHash = ReplayCoordinator.GetRuntimeSession().GetCanonicalStateHash();
    ReplayCoordinator.EndSession(EGV2SessionState::Destroyed);

    TestEqual(TEXT("Coordinator replay reproduces initial PRNG value"), CoordReplayVal, OriginalPrngVal);
    TestEqual(TEXT("Coordinator replay reproduces canonical state hash"),
        FString(UTF8_TO_TCHAR(CoordReplayStateHash.c_str())),
        FString(UTF8_TO_TCHAR(OriginalStateHash.c_str())));

    return true;
}

// 3. Uninitialized or invalid descriptor SeedHex is strictly rejected
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionUninitializedDescriptorSeedRejectedTest,
    "GV2.Runtime.Session.UninitializedDescriptorSeedRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionUninitializedDescriptorSeedRejectedTest::RunTest(const FString& Parameters)
{
    FSessionStartDescriptor BadDesc;
    BadDesc.Mode = ESessionStartMode::NewGame;
    BadDesc.RepositoryVersion = TEXT("1");
    BadDesc.RepositoryContentHash = TEXT("some_hash");
    BadDesc.SeedHex = TEXT(""); // Empty seed

    FString ErrorMsg;
    TestFalse(TEXT("Empty seed descriptor fails IsValid"), BadDesc.IsValid(&ErrorMsg));
    TestTrue(TEXT("Error message identifies SeedHex failure"), ErrorMsg.Contains(TEXT("SeedHex")));

    FGV2SessionCoordinator Coordinator;
    Coordinator.SetDocumentSink([](const FGV2UiDocumentViewModel&, const FGV2PresentationPrepareContext&) -> bool { return true; });

    GV2ContentCore::FRepositoryReadHandle EmptyHandle;
    GV2ContentHostSupport::FResolvedPackageSet EmptySet;

    AddExpectedErrorPlain(TEXT("InvalidSessionDescriptor"), EAutomationExpectedErrorFlags::Contains, 1);
    const uint64 Op = Coordinator.RequestSession(BadDesc, EmptyHandle, 1, EmptySet);
    TestEqual(TEXT("Coordinator rejects session with uninitialized seed"),
        *Coordinator.GetSessionOperationOutcome(Op), ESessionOperationOutcome::Failed);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
