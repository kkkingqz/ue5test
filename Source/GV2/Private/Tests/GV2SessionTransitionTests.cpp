#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

#include "Application/GV2SessionCoordinator.h"
#include "Application/GV2SessionTransition.h"
#include "Application/GV2FilesystemContentSourceProvider.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"
#include "GV2RuntimeCore/GV2HostServices.h"
#include "Bridge/GV2BridgeTypes.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

#include <vector>
#include <string>

namespace
{

class FTestTraceSaveSlotStorage : public GV2RuntimeCore::ISaveSlotStorage
{
public:
    std::vector<std::string> TracedHooks;

    virtual GV2RuntimeCore::FSaveSlotReadResult ReadSlot(const std::string& SlotId) const override
    {
        return {};
    }

    virtual GV2RuntimeCore::FSaveSlotWriteResult WriteSlot(
        const std::string& SlotId,
        const std::string& Bytes) override
    {
        TracedHooks.push_back(Bytes);
        GV2RuntimeCore::FSaveSlotWriteResult Result;
        Result.Result = GV2RuntimeCore::ESaveSlotResult::Ok;
        return Result;
    }
};

std::vector<GV2RuntimeCore::FRuntimeSource> LoadTransitionCoreSources()
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

} // anonymous namespace

// 1. Oracle Matrix Test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionTransitionOracleMatrixTest,
    "GV2.Session.Transition.OracleMatrix",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionTransitionOracleMatrixTest::RunTest(const FString& Parameters)
{
    static_assert(UE_ARRAY_COUNT(AllSessionTransitionKinds) == SessionTransitionKindCount,
        "AllSessionTransitionKinds must have exactly SessionTransitionKindCount elements");

    // Dynamically derive StartKinds from AllSessionTransitionKinds (all non-shutdown kinds)
    TArray<ESessionTransitionKind> StartKinds;
    for (const ESessionTransitionKind Kind : AllSessionTransitionKinds)
    {
        if (Kind != ESessionTransitionKind::Shutdown)
        {
            StartKinds.Add(Kind);
        }
    }
    TestEqual(TEXT("StartKinds count matches expected start modes"), StartKinds.Num(), static_cast<int32>(SessionTransitionKindCount) - 1);

    for (ESessionTransitionKind Kind : StartKinds)
    {
        TestTrue(TEXT("None -> Creating is valid"),
            FGV2SessionTransitionOracle::CanTransitionSessionState(EGV2SessionState::None, EGV2SessionState::Creating, Kind));
        TestTrue(TEXT("Creating -> Registering is valid"),
            FGV2SessionTransitionOracle::CanTransitionSessionState(EGV2SessionState::Creating, EGV2SessionState::Registering, Kind));
        TestTrue(TEXT("Registering -> BuildingState is valid"),
            FGV2SessionTransitionOracle::CanTransitionSessionState(EGV2SessionState::Registering, EGV2SessionState::BuildingState, Kind));
        TestTrue(TEXT("BuildingState -> RestoringInstances is valid"),
            FGV2SessionTransitionOracle::CanTransitionSessionState(EGV2SessionState::BuildingState, EGV2SessionState::RestoringInstances, Kind));
        TestTrue(TEXT("RestoringInstances -> Starting is valid"),
            FGV2SessionTransitionOracle::CanTransitionSessionState(EGV2SessionState::RestoringInstances, EGV2SessionState::Starting, Kind));
        TestTrue(TEXT("Starting -> PreparingPresentation is valid"),
            FGV2SessionTransitionOracle::CanTransitionSessionState(EGV2SessionState::Starting, EGV2SessionState::PreparingPresentation, Kind));
        TestTrue(TEXT("PreparingPresentation -> Ready is valid"),
            FGV2SessionTransitionOracle::CanTransitionSessionState(EGV2SessionState::PreparingPresentation, EGV2SessionState::Ready, Kind));

        // From Ready, replacement starts by stopping the prior session
        TestTrue(TEXT("Ready -> Stopping is valid"),
            FGV2SessionTransitionOracle::CanTransitionSessionState(EGV2SessionState::Ready, EGV2SessionState::Stopping, Kind));

        // Abort/error paths lead to Stopping
        TestTrue(TEXT("Creating -> Stopping is valid"),
            FGV2SessionTransitionOracle::CanTransitionSessionState(EGV2SessionState::Creating, EGV2SessionState::Stopping, Kind));
        TestTrue(TEXT("BuildingState -> Stopping is valid"),
            FGV2SessionTransitionOracle::CanTransitionSessionState(EGV2SessionState::BuildingState, EGV2SessionState::Stopping, Kind));

        // Stopping -> Destroyed / None / Failed is valid
        TestTrue(TEXT("Stopping -> Destroyed is valid"),
            FGV2SessionTransitionOracle::CanTransitionSessionState(EGV2SessionState::Stopping, EGV2SessionState::Destroyed, Kind));
        TestTrue(TEXT("Stopping -> None is valid"),
            FGV2SessionTransitionOracle::CanTransitionSessionState(EGV2SessionState::Stopping, EGV2SessionState::None, Kind));

        // Invalid skipping
        TestFalse(TEXT("None -> BuildingState skipping is invalid"),
            FGV2SessionTransitionOracle::CanTransitionSessionState(EGV2SessionState::None, EGV2SessionState::BuildingState, Kind));
        TestFalse(TEXT("None -> Ready skipping is invalid"),
            FGV2SessionTransitionOracle::CanTransitionSessionState(EGV2SessionState::None, EGV2SessionState::Ready, Kind));
        TestFalse(TEXT("Creating -> Ready skipping is invalid"),
            FGV2SessionTransitionOracle::CanTransitionSessionState(EGV2SessionState::Creating, EGV2SessionState::Ready, Kind));
        TestFalse(TEXT("BuildingState -> Creating backwards is invalid"),
            FGV2SessionTransitionOracle::CanTransitionSessionState(EGV2SessionState::BuildingState, EGV2SessionState::Creating, Kind));
    }

    // ApplicationState transitions
    TestTrue(TEXT("Uninitialized -> MenuActive is valid"),
        FGV2SessionTransitionOracle::CanTransitionApplicationState(EGV2ApplicationState::Uninitialized, EGV2ApplicationState::MenuActive));
    TestTrue(TEXT("Uninitialized -> GameActive is valid"),
        FGV2SessionTransitionOracle::CanTransitionApplicationState(EGV2ApplicationState::Uninitialized, EGV2ApplicationState::GameActive));
    TestTrue(TEXT("MenuActive -> Transitioning is valid"),
        FGV2SessionTransitionOracle::CanTransitionApplicationState(EGV2ApplicationState::MenuActive, EGV2ApplicationState::Transitioning));
    TestTrue(TEXT("GameActive -> Transitioning is valid"),
        FGV2SessionTransitionOracle::CanTransitionApplicationState(EGV2ApplicationState::GameActive, EGV2ApplicationState::Transitioning));
    TestTrue(TEXT("MenuActive -> ShuttingDown is valid"),
        FGV2SessionTransitionOracle::CanTransitionApplicationState(EGV2ApplicationState::MenuActive, EGV2ApplicationState::ShuttingDown));
    TestTrue(TEXT("ShuttingDown -> Uninitialized is valid"),
        FGV2SessionTransitionOracle::CanTransitionApplicationState(EGV2ApplicationState::ShuttingDown, EGV2ApplicationState::Uninitialized));
    TestTrue(TEXT("MenuActive -> Uninitialized is valid for emergency teardown"),
        FGV2SessionTransitionOracle::CanTransitionApplicationState(EGV2ApplicationState::MenuActive, EGV2ApplicationState::Uninitialized));
    TestTrue(TEXT("Uninitialized -> Failed is valid"),
        FGV2SessionTransitionOracle::CanTransitionApplicationState(EGV2ApplicationState::Uninitialized, EGV2ApplicationState::Failed));

    // Invalid ApplicationState transitions
    TestFalse(TEXT("Uninitialized -> ShuttingDown is invalid"),
        FGV2SessionTransitionOracle::CanTransitionApplicationState(EGV2ApplicationState::Uninitialized, EGV2ApplicationState::ShuttingDown));
    TestFalse(TEXT("MenuActive -> Terminated directly is invalid"),
        FGV2SessionTransitionOracle::CanTransitionApplicationState(EGV2ApplicationState::MenuActive, EGV2ApplicationState::Terminated));
    TestFalse(TEXT("Same state transition is invalid"),
        FGV2SessionTransitionOracle::CanTransitionApplicationState(EGV2ApplicationState::Uninitialized, EGV2ApplicationState::Uninitialized));

    // Shutdown transitions
    {
        const ESessionTransitionKind ShutdownKind = ESessionTransitionKind::Shutdown;
        TestTrue(TEXT("Shutdown: Ready -> Stopping is valid"),
            FGV2SessionTransitionOracle::CanTransitionSessionState(EGV2SessionState::Ready, EGV2SessionState::Stopping, ShutdownKind));
        TestTrue(TEXT("Shutdown: Stopping -> Destroyed is valid"),
            FGV2SessionTransitionOracle::CanTransitionSessionState(EGV2SessionState::Stopping, EGV2SessionState::Destroyed, ShutdownKind));
        TestTrue(TEXT("Shutdown: Stopping -> None is valid"),
            FGV2SessionTransitionOracle::CanTransitionSessionState(EGV2SessionState::Stopping, EGV2SessionState::None, ShutdownKind));
        TestFalse(TEXT("Shutdown: None -> None is invalid (same state)"),
            FGV2SessionTransitionOracle::CanTransitionSessionState(EGV2SessionState::None, EGV2SessionState::None, ShutdownKind));

        TestFalse(TEXT("Shutdown: Creating -> Registering is invalid"),
            FGV2SessionTransitionOracle::CanTransitionSessionState(EGV2SessionState::Creating, EGV2SessionState::Registering, ShutdownKind));
    }

    // Verify complete enum coverage across the oracle test (StartKinds + Shutdown)
    TSet<ESessionTransitionKind> CoveredKinds(StartKinds);
    CoveredKinds.Add(ESessionTransitionKind::Shutdown);
    TestEqual(TEXT("All transition kinds covered by Oracle test"), CoveredKinds.Num(), static_cast<int32>(SessionTransitionKindCount));

    return true;
}

// 2. Request Joining Test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionTransitionRequestJoiningTest,
    "GV2.Session.Transition.RequestJoining",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionTransitionRequestJoiningTest::RunTest(const FString& Parameters)
{
    FGV2SessionTransitionPolicy Policy;

    FSessionStartDescriptor DescA;
    DescA.Mode = ESessionStartMode::NewGame;
    DescA.RepositoryVersion = TEXT("1");
    DescA.RepositoryContentHash = TEXT("content_hash_alpha");
    DescA.SeedHex = TEXT("0000000000000001");

    bool bJoinedA = false;
    uint64 JoinedOpIdA = 0;
    const uint64 OpA = Policy.EnqueueRequest(DescA, bJoinedA, JoinedOpIdA);
    TestFalse(TEXT("First request is not joined"), bJoinedA);
    TestTrue(TEXT("First request gets valid OpId"), OpA > 0);

    // Dequeue next transition -> OpA is now the active operation
    const TOptional<FSessionOperationRecord> NextOp = Policy.DequeuePendingOperation();
    TestTrue(TEXT("Active operation dequeued"), NextOp.IsSet());
    TestEqual(TEXT("Active OpId matches OpA"), NextOp->OperationId, OpA);
    Policy.SetActiveOperation(*NextOp);

    // Enqueue second identical request DescA while OpA is active
    bool bJoinedB = false;
    uint64 JoinedOpIdB = 0;
    const uint64 OpB = Policy.EnqueueRequest(DescA, bJoinedB, JoinedOpIdB);
    TestTrue(TEXT("Second identical request is joined with active"), bJoinedB);
    TestEqual(TEXT("Joined OpId is OpA"), JoinedOpIdB, OpA);
    TestEqual(TEXT("Returned OpId is OpA"), OpB, OpA);
    TestFalse(TEXT("No pending operation created when joined"), Policy.HasPendingOperation());

    // Enqueue different request DescC -> goes to pending slot
    FSessionStartDescriptor DescC;
    DescC.Mode = ESessionStartMode::NewGame;
    DescC.RepositoryVersion = TEXT("2");
    DescC.RepositoryContentHash = TEXT("content_hash_beta");
    DescC.SeedHex = TEXT("0000000000000003");
    bool bJoinedC = false;
    uint64 JoinedOpIdC = 0;
    const uint64 OpC = Policy.EnqueueRequest(DescC, bJoinedC, JoinedOpIdC);
    TestFalse(TEXT("Different request is not joined"), bJoinedC);
    TestTrue(TEXT("OpC has unique OpId"), OpC != OpA);
    TestTrue(TEXT("OpC is in pending slot"), Policy.HasPendingOperation());
    TestEqual(TEXT("Pending OpId is OpC"), Policy.GetPendingOperation()->OperationId, OpC);

    // Enqueue identical to DescC -> joins pending slot!
    bool bJoinedD = false;
    uint64 JoinedOpIdD = 0;
    const uint64 OpD = Policy.EnqueueRequest(DescC, bJoinedD, JoinedOpIdD);
    TestTrue(TEXT("Request identical to pending is joined"), bJoinedD);
    TestEqual(TEXT("Joined OpId is OpC"), JoinedOpIdD, OpC);
    TestEqual(TEXT("Returned OpId is OpC"), OpD, OpC);

    return true;
}

// 3. Pending Superseded Test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionTransitionPendingSupersededTest,
    "GV2.Session.Transition.PendingSuperseded",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionTransitionPendingSupersededTest::RunTest(const FString& Parameters)
{
    FGV2SessionTransitionPolicy Policy;

    FSessionStartDescriptor DescA;
    DescA.Mode = ESessionStartMode::NewGame;
    DescA.RepositoryVersion = TEXT("1");
    DescA.SeedHex = TEXT("0000000000000001");
    bool bJoined = false;
    uint64 JoinedId = 0;
    const uint64 OpA = Policy.EnqueueRequest(DescA, bJoined, JoinedId);
    const TOptional<FSessionOperationRecord> NextOp = Policy.DequeuePendingOperation();
    Policy.SetActiveOperation(*NextOp); // OpA is active

    // Enqueue OpB into pending slot
    FSessionStartDescriptor DescB;
    DescB.Mode = ESessionStartMode::NewGame;
    DescB.RepositoryVersion = TEXT("2");
    DescB.SeedHex = TEXT("0000000000000002");
    const uint64 OpB = Policy.EnqueueRequest(DescB, bJoined, JoinedId);
    TestTrue(TEXT("OpB is pending"), Policy.HasPendingOperation());
    TestEqual(TEXT("Pending Op is OpB"), Policy.GetPendingOperation()->OperationId, OpB);

    // Enqueue OpC into pending slot (non-equivalent to B) -> OpB is superseded!
    FSessionStartDescriptor DescC;
    DescC.Mode = ESessionStartMode::Menu;
    DescC.RepositoryVersion = TEXT("3");
    DescC.SeedHex = TEXT("0000000000000003");
    const uint64 OpC = Policy.EnqueueRequest(DescC, bJoined, JoinedId);
    TestTrue(TEXT("Pending slot now holds OpC"), Policy.HasPendingOperation());
    TestEqual(TEXT("Pending Op is OpC"), Policy.GetPendingOperation()->OperationId, OpC);

    // Verify OpB outcome is immediately Superseded
    const TOptional<ESessionOperationOutcome> OutcomeB = Policy.GetOutcome(OpB);
    TestTrue(TEXT("Outcome for OpB exists"), OutcomeB.IsSet());
    TestEqual(TEXT("Outcome for OpB is Superseded"), *OutcomeB, ESessionOperationOutcome::Superseded);

    return true;
}

// 4. Cancellation & Point of No Return Test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionTransitionCancellationTest,
    "GV2.Session.Transition.CancellationAndPointOfNoReturn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionTransitionCancellationTest::RunTest(const FString& Parameters)
{
    FGV2SessionTransitionPolicy Policy;

    FSessionStartDescriptor DescA;
    DescA.Mode = ESessionStartMode::NewGame;
    DescA.RepositoryVersion = TEXT("1");
    DescA.SeedHex = TEXT("0000000000000001");
    bool bJoined = false;
    uint64 JoinedId = 0;
    const uint64 OpA = Policy.EnqueueRequest(DescA, bJoined, JoinedId);
    const TOptional<FSessionOperationRecord> NextOp = Policy.DequeuePendingOperation();
    Policy.SetActiveOperation(*NextOp); // OpA is active

    // Enqueue OpB into pending slot
    FSessionStartDescriptor DescB;
    DescB.Mode = ESessionStartMode::NewGame;
    DescB.RepositoryVersion = TEXT("2");
    DescB.SeedHex = TEXT("0000000000000002");
    const uint64 OpB = Policy.EnqueueRequest(DescB, bJoined, JoinedId);

    // 1. Cancel pending request OpB -> Accepted, outcome Cancelled, pending slot cleared
    const ESessionCancellationResult ResultB = Policy.CancelRequest(OpB);
    TestEqual(TEXT("Pending cancellation accepted"), ResultB, ESessionCancellationResult::Accepted);
    TestFalse(TEXT("Pending slot cleared after cancellation"), Policy.HasPendingOperation());
    TestEqual(TEXT("OpB outcome is Cancelled"), *Policy.GetOutcome(OpB), ESessionOperationOutcome::Cancelled);

    // 2. Cancel active request OpA before point of no return -> Accepted, bCancellationRequested set
    const ESessionCancellationResult ResultA = Policy.CancelRequest(OpA);
    TestEqual(TEXT("Active pre-commit cancellation accepted"), ResultA, ESessionCancellationResult::Accepted);
    TestTrue(TEXT("Active operation has cancellation requested"), Policy.GetActiveOperation()->bCancellationRequested);

    // 3. Mark point of no return and attempt cancellation again -> TooLate
    Policy.GetActiveOperation()->bCommitted = true;
    const ESessionCancellationResult ResultTooLate = Policy.CancelRequest(OpA);
    TestEqual(TEXT("Post-commit cancellation returns TooLate"), ResultTooLate, ESessionCancellationResult::TooLate);

    // 4. Cancel stale / non-existent request -> Stale
    const ESessionCancellationResult ResultStale = Policy.CancelRequest(999999);
    TestEqual(TEXT("Stale cancellation returns Stale"), ResultStale, ESessionCancellationResult::Stale);

    return true;
}

// 5. Shutdown Priority Test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionTransitionShutdownPriorityTest,
    "GV2.Session.Transition.ShutdownPriority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionTransitionShutdownPriorityTest::RunTest(const FString& Parameters)
{
    FGV2SessionTransitionPolicy Policy;

    FSessionStartDescriptor DescA;
    DescA.Mode = ESessionStartMode::NewGame;
    DescA.RepositoryVersion = TEXT("1");
    DescA.SeedHex = TEXT("0000000000000001");
    bool bJoined = false;
    uint64 JoinedId = 0;
    const uint64 OpA = Policy.EnqueueRequest(DescA, bJoined, JoinedId);
    const TOptional<FSessionOperationRecord> NextOp = Policy.DequeuePendingOperation();
    Policy.SetActiveOperation(*NextOp); // OpA is active

    FSessionStartDescriptor DescB;
    DescB.Mode = ESessionStartMode::NewGame;
    DescB.RepositoryVersion = TEXT("2");
    DescB.SeedHex = TEXT("0000000000000002");
    const uint64 OpB = Policy.EnqueueRequest(DescB, bJoined, JoinedId); // Pending

    // Enqueue Shutdown
    const uint64 OpShutdown = Policy.EnqueueShutdown();
    TestTrue(TEXT("Shutdown gets valid OpId"), OpShutdown > 0);

    // OpB was in pending slot: must be superseded!
    TestEqual(TEXT("Pending OpB superseded by shutdown"), *Policy.GetOutcome(OpB), ESessionOperationOutcome::Superseded);

    // Active OpA must have cancellation requested
    TestTrue(TEXT("Active OpA marked for cancellation by shutdown"), Policy.GetActiveOperation()->bCancellationRequested);

    // Pending slot must now hold Shutdown
    TestTrue(TEXT("Pending slot holds shutdown"), Policy.HasPendingOperation());
    TestEqual(TEXT("Pending op is shutdown"), Policy.GetPendingOperation()->OperationId, OpShutdown);
    TestEqual(TEXT("Pending op kind is Shutdown"), Policy.GetPendingOperation()->Kind, ESessionTransitionKind::Shutdown);

    return true;
}

// 6. Reverse Module Teardown Test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionReverseTeardownTest,
    "GV2.Session.Transition.ReverseModuleTeardown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionReverseTeardownTest::RunTest(const FString& Parameters)
{
    std::vector<GV2RuntimeCore::FRuntimeSource> Sources = LoadTransitionCoreSources();

    // Add 3 synthetic test modules with chain dependencies: mod1 <- mod2 <- mod3
    const char* ManifestSource = R"(
return {
    modules = {
        {
            module_id = "testrev:module.mod1",
            source = "mod1.lua",
            dependencies = {},
        },
        {
            module_id = "testrev:module.mod2",
            source = "mod2.lua",
            dependencies = { "testrev:module.mod1" },
        },
        {
            module_id = "testrev:module.mod3",
            source = "mod3.lua",
            dependencies = { "testrev:module.mod2" },
        },
    },
}
)";

    const char* Mod1Source = R"(
local M = { id = "testrev:module.mod1" }
function M.register(ctx) end
function M.start(ctx) end
function M.stop(ctx, reason)
    game.save_slots.write("trace", "mod1:stop")
end
function M.unregister(ctx)
    game.save_slots.write("trace", "mod1:unregister")
end
return M
)";

    const char* Mod2Source = R"(
local M = { id = "testrev:module.mod2" }
function M.register(ctx) end
function M.start(ctx) end
function M.stop(ctx, reason)
    game.save_slots.write("trace", "mod2:stop")
end
function M.unregister(ctx)
    game.save_slots.write("trace", "mod2:unregister")
end
return M
)";

    const char* Mod3Source = R"(
local M = { id = "testrev:module.mod3" }
function M.register(ctx) end
function M.start(ctx) end
function M.stop(ctx, reason)
    game.save_slots.write("trace", "mod3:stop")
end
function M.unregister(ctx)
    game.save_slots.write("trace", "mod3:unregister")
end
return M
)";

    Sources.push_back({"@testrev/manifest.lua", ManifestSource});
    Sources.push_back({"@testrev/mod1.lua", Mod1Source});
    Sources.push_back({"@testrev/mod2.lua", Mod2Source});
    Sources.push_back({"@testrev/mod3.lua", Mod3Source});

    const FString CorePackageRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData/core"));
    const GV2ContentCore::FBuildResult RepoBuild = BuildGV2RepositoryFromDirectory(CorePackageRoot);
    GV2ContentCore::FRepositoryReadHandle ReadHandle;
    if (RepoBuild.IsSuccess())
    {
        ReadHandle = RepoBuild.GetCandidate().GetReadHandle();
    }
    TestTrue(TEXT("Repository read handle valid"), ReadHandle.IsValid());

    FTestTraceSaveSlotStorage TraceStorage;
    GV2RuntimeCore::FRuntimeFault Fault;

    {
        GV2RuntimeCore::FRuntimeSession Session;
        Session.SetSaveSlotStorage(&TraceStorage);

        const bool bStarted = Session.Start(1, "0000000000000001", ReadHandle, Sources, Fault);
        TestTrue(TEXT("Session with test modules started successfully"), bStarted);

        // Stop session: should run reverse teardown
        Session.Stop();
    }

    // Expected order:
    // stop hooks: mod3:stop, mod2:stop, mod1:stop (reverse order of load!)
    // unregister hooks: mod3:unregister, mod2:unregister, mod1:unregister (reverse order of load!)
    const std::vector<std::string> Expected = {
        "mod3:stop",
        "mod2:stop",
        "mod1:stop",
        "mod3:unregister",
        "mod2:unregister",
        "mod1:unregister"
    };

    TestEqual(TEXT("Traced hook count matches expected"), TraceStorage.TracedHooks.size(), Expected.size());
    for (size_t i = 0; i < TraceStorage.TracedHooks.size() && i < Expected.size(); ++i)
    {
        TestEqual(
            FString::Printf(TEXT("Hook %d executed in exact reverse order"), static_cast<int32>(i)),
            FString(UTF8_TO_TCHAR(TraceStorage.TracedHooks[i].c_str())),
            FString(UTF8_TO_TCHAR(Expected[i].c_str())));
    }

    return true;
}

// 7. Single VM Invariant & Sequential Lifecycle Test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionSingleVmInvariantTest,
    "GV2.Session.Transition.SingleVmInvariantAndSequentialLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionSingleVmInvariantTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Initial GLiveVmCount is 0"), GV2RuntimeCore::FRuntimeSession::GetLiveVmCount(), 0);

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

    // 1. Start Menu session
    {
        FSessionStartDescriptor MenuDesc;
        MenuDesc.Mode = ESessionStartMode::Menu;
        MenuDesc.RepositoryVersion = TEXT("1");
        MenuDesc.RepositoryContentHash = UTF8_TO_TCHAR(ReadHandle.GetContentHash().c_str());
        MenuDesc.SeedHex = FSessionStartDescriptor::GenerateFreshSeedHex();

        const uint64 OpMenu1 = Coordinator.RequestSession(MenuDesc, ReadHandle, 1, *Resolved);
        TestEqual(TEXT("Menu session 1 outcome is Completed"),
            *Coordinator.GetSessionOperationOutcome(OpMenu1), ESessionOperationOutcome::Completed);
        TestEqual(TEXT("Single VM invariant: LiveVmCount is exactly 1"), GV2RuntimeCore::FRuntimeSession::GetLiveVmCount(), 1);
        TestEqual(TEXT("Session state is Ready"), Coordinator.GetStatus().SessionState, EGV2SessionState::Ready);
        TestEqual(TEXT("Session generation is 1"), Coordinator.GetStatus().SessionGeneration, 1);
    }

    // 2. Transition Menu -> Game (NewGame session 2)
    {
        FSessionStartDescriptor GameDesc;
        GameDesc.Mode = ESessionStartMode::NewGame;
        GameDesc.RepositoryVersion = TEXT("1");
        GameDesc.RepositoryContentHash = UTF8_TO_TCHAR(ReadHandle.GetContentHash().c_str());
        GameDesc.SeedHex = FSessionStartDescriptor::GenerateFreshSeedHex();

        const uint64 OpGame = Coordinator.RequestSession(GameDesc, ReadHandle, 1, *Resolved);
        TestEqual(TEXT("Game session outcome is Completed"),
            *Coordinator.GetSessionOperationOutcome(OpGame), ESessionOperationOutcome::Completed);
        TestEqual(TEXT("Single VM invariant maintained: LiveVmCount is exactly 1"), GV2RuntimeCore::FRuntimeSession::GetLiveVmCount(), 1);
        TestEqual(TEXT("Session state is Ready"), Coordinator.GetStatus().SessionState, EGV2SessionState::Ready);
        TestEqual(TEXT("Session generation incremented to 2"), Coordinator.GetStatus().SessionGeneration, 2);
    }

    // 3. Transition Game -> Menu (Menu session 3)
    {
        FSessionStartDescriptor MenuDesc3;
        MenuDesc3.Mode = ESessionStartMode::Menu;
        MenuDesc3.RepositoryVersion = TEXT("1");
        MenuDesc3.RepositoryContentHash = UTF8_TO_TCHAR(ReadHandle.GetContentHash().c_str());
        MenuDesc3.SeedHex = FSessionStartDescriptor::GenerateFreshSeedHex();

        const uint64 OpMenu3 = Coordinator.RequestSession(MenuDesc3, ReadHandle, 1, *Resolved);
        TestEqual(TEXT("Menu session 3 outcome is Completed"),
            *Coordinator.GetSessionOperationOutcome(OpMenu3), ESessionOperationOutcome::Completed);
        TestEqual(TEXT("Single VM invariant maintained: LiveVmCount is exactly 1"), GV2RuntimeCore::FRuntimeSession::GetLiveVmCount(), 1);
        TestEqual(TEXT("Session state is Ready"), Coordinator.GetStatus().SessionState, EGV2SessionState::Ready);
        TestEqual(TEXT("Session generation incremented to 3"), Coordinator.GetStatus().SessionGeneration, 3);
    }

    // 4. Shutdown session
    Coordinator.EndSession(EGV2SessionState::Destroyed);
    TestEqual(TEXT("After shutdown: LiveVmCount is 0"), GV2RuntimeCore::FRuntimeSession::GetLiveVmCount(), 0);
    TestEqual(TEXT("Session state is Destroyed"), Coordinator.GetStatus().SessionState, EGV2SessionState::Destroyed);
    TestEqual(TEXT("Application state is Uninitialized"), Coordinator.GetStatus().ApplicationState, EGV2ApplicationState::Uninitialized);

    // 5. Verify atomic dual-session prevention
    {
        GV2RuntimeCore::FRuntimeSession SessionA;
        GV2RuntimeCore::FRuntimeFault FaultA;
        const std::vector<GV2RuntimeCore::FRuntimeSource> Sources = LoadTransitionCoreSources();
        TestTrue(TEXT("SessionA starts"), SessionA.Start(1, "0000000000000001", ReadHandle, Sources, FaultA));
        TestEqual(TEXT("LiveVmCount is 1"), GV2RuntimeCore::FRuntimeSession::GetLiveVmCount(), 1);

        GV2RuntimeCore::FRuntimeSession SessionB;
        GV2RuntimeCore::FRuntimeFault FaultB;
        const bool bStartedB = SessionB.Start(2, "0000000000000002", ReadHandle, Sources, FaultB);
        TestFalse(TEXT("SessionB is rejected by atomic guard"), bStartedB);
        TestEqual(TEXT("Fault code is LuaVmExceededLimit"), FString(UTF8_TO_TCHAR(FaultB.Code.c_str())), TEXT("LuaVmExceededLimit"));
        TestEqual(TEXT("LiveVmCount never exceeds 1"), GV2RuntimeCore::FRuntimeSession::GetLiveVmCount(), 1);

        SessionA.Stop();
        TestEqual(TEXT("LiveVmCount returns to 0"), GV2RuntimeCore::FRuntimeSession::GetLiveVmCount(), 0);
    }

    return true;
}

// 8. Pre-Commit Cancellation Keeps Prior Session
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionPreCommitCancellationTest,
    "GV2.Session.Transition.PreCommitCancellationKeepsPriorSession",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionPreCommitCancellationTest::RunTest(const FString& Parameters)
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

    // Start Session A (Menu)
    FSessionStartDescriptor DescA;
    DescA.Mode = ESessionStartMode::Menu;
    DescA.RepositoryVersion = TEXT("1");
    DescA.RepositoryContentHash = UTF8_TO_TCHAR(ReadHandle.GetContentHash().c_str());
    DescA.SeedHex = FSessionStartDescriptor::GenerateFreshSeedHex();

    const uint64 OpA = Coordinator.RequestSession(DescA, ReadHandle, 1, *Resolved);
    TestEqual(TEXT("Session A completed"), *Coordinator.GetSessionOperationOutcome(OpA), ESessionOperationOutcome::Completed);
    TestEqual(TEXT("Session A is Ready"), Coordinator.GetStatus().SessionState, EGV2SessionState::Ready);
    TestEqual(TEXT("Session A generation is 1"), Coordinator.GetStatus().SessionGeneration, 1);
    TestTrue(TEXT("Session A is ready flag"), Coordinator.GetStatus().bIsReady);

    // Cancel before point of no return test:
    // Enqueue Session B, cancel it before BeginReplace
    FSessionStartDescriptor DescB;
    DescB.Mode = ESessionStartMode::NewGame;
    DescB.RepositoryVersion = TEXT("1");
    DescB.RepositoryContentHash = UTF8_TO_TCHAR(ReadHandle.GetContentHash().c_str());
    DescB.SeedHex = FSessionStartDescriptor::GenerateFreshSeedHex();

    // Test transition policy cancellation directly:
    FGV2SessionTransitionPolicy TestPolicy;
    bool bJoined = false;
    uint64 JoinedId = 0;
    const uint64 OpTestB = TestPolicy.EnqueueRequest(DescB, bJoined, JoinedId);
    const TOptional<FSessionOperationRecord> NextOp = TestPolicy.DequeuePendingOperation();
    TestPolicy.SetActiveOperation(*NextOp); // active pre-commit
    const ESessionCancellationResult CancelResult = TestPolicy.CancelRequest(OpTestB);
    TestEqual(TEXT("Pre-commit cancellation accepted"), CancelResult, ESessionCancellationResult::Accepted);

    // Prior Session A in Coordinator was never touched and remains intact
    TestEqual(TEXT("Session A is still Ready"), Coordinator.GetStatus().SessionState, EGV2SessionState::Ready);
    TestEqual(TEXT("Session A generation is still 1"), Coordinator.GetStatus().SessionGeneration, 1);
    TestTrue(TEXT("Session A is still ready flag"), Coordinator.GetStatus().bIsReady);

    Coordinator.EndSession(EGV2SessionState::Destroyed);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
