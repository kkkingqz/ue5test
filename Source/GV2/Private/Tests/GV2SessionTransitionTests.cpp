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
#include "Runtime/GV2RuntimeSubsystem.h"
#include "Tests/GV2PresentationTestFixtures.h"
#include "UI/GV2UiTheme.h"
#include "UI/GV2ScreenRegistry.h"

#include <vector>
#include <string>
#include <type_traits>

static_assert(
    !std::is_default_constructible_v<FGV2RequiredOperationFault>,
    "The Failed construction token must require a declared fault code.");
static_assert(
    std::is_constructible_v<FGV2RequiredOperationFault, EGV2SessionFaultCode, FString>,
    "The Failed construction token must be constructible only with typed fault data.");
static_assert(
    !std::is_constructible_v<FGV2RequiredOperationFault, FString, FString>,
    "Arbitrary strings must not bypass the declared session fault code enum.");

namespace
{

class FTestTraceSaveSlotStorage : public GV2RuntimeCore::ISaveSlotStorage
{
public:
    std::vector<std::string> TracedHooks;

    virtual GV2RuntimeCore::FSaveSlotReadResult ReadSlot(
        const std::string& SlotId,
        GV2RuntimeCore::ESaveSlotRevision Revision = GV2RuntimeCore::ESaveSlotRevision::Current) const override
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
    const TOptional<FGV2SessionOperationResult> OutcomeB = Policy.GetOutcome(OpB);
    TestTrue(TEXT("Outcome for OpB exists"), OutcomeB.IsSet());
    TestEqual(TEXT("Outcome for OpB is Superseded"), OutcomeB->Outcome, ESessionOperationOutcome::Superseded);

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
    TestEqual(TEXT("OpB outcome is Cancelled"), Policy.GetOutcome(OpB)->Outcome, ESessionOperationOutcome::Cancelled);

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
    TestEqual(TEXT("Pending OpB superseded by shutdown"), Policy.GetOutcome(OpB)->Outcome, ESessionOperationOutcome::Superseded);

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
            Coordinator.GetSessionOperationOutcome(OpMenu1)->Outcome, ESessionOperationOutcome::Completed);
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
            Coordinator.GetSessionOperationOutcome(OpGame)->Outcome, ESessionOperationOutcome::Completed);
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
            Coordinator.GetSessionOperationOutcome(OpMenu3)->Outcome, ESessionOperationOutcome::Completed);
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
    TestEqual(TEXT("Session A completed"), Coordinator.GetSessionOperationOutcome(OpA)->Outcome, ESessionOperationOutcome::Completed);
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

// SAC-05: Typed fault propagation to terminal operation outcomes
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionOperationFaultPropagationTest,
    "GV2.Runtime.SessionTransition.FaultPropagation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionOperationFaultPropagationTest::RunTest(const FString& Parameters)
{
    // 1. Non-failure outcomes never carry a fault
    {
        FGV2SessionTransitionPolicy Policy;
        FSessionStartDescriptor Desc;
        Desc.Mode = ESessionStartMode::Menu;
        Desc.RepositoryVersion = TEXT("1");
        Desc.RepositoryContentHash = TEXT("testhash");
        Desc.SeedHex = TEXT("0123456789abcdef");

        bool bJoined = false;
        uint64 JoinedId = 0;
        const uint64 Op1 = Policy.EnqueueRequest(Desc, bJoined, JoinedId);
        Policy.RecordOutcome(Op1, ESessionNonFailureOutcome::Completed);
        const TOptional<FGV2SessionOperationResult> Res1 = Policy.GetOutcome(Op1);
        TestTrue(TEXT("Op1 outcome is set"), Res1.IsSet());
        TestEqual(TEXT("Op1 is Completed"), Res1->Outcome, ESessionOperationOutcome::Completed);
        TestFalse(TEXT("Completed has no fault"), Res1->Fault.IsSet());
        TestTrue(TEXT("Completed fault code empty"), Res1->Fault.Code.IsEmpty());

        const uint64 Op2 = Policy.EnqueueRequest(Desc, bJoined, JoinedId);
        const ESessionCancellationResult CancelRes = Policy.CancelRequest(Op2);
        TestEqual(TEXT("Cancel accepted"), CancelRes, ESessionCancellationResult::Accepted);
        const TOptional<FGV2SessionOperationResult> Res2 = Policy.GetOutcome(Op2);
        TestTrue(TEXT("Op2 outcome is set"), Res2.IsSet());
        TestEqual(TEXT("Op2 is Cancelled"), Res2->Outcome, ESessionOperationOutcome::Cancelled);
        TestFalse(TEXT("Cancelled has no fault"), Res2->Fault.IsSet());
        TestTrue(TEXT("Cancelled fault code empty"), Res2->Fault.Code.IsEmpty());

        // Test Superseded
        const uint64 Op3 = Policy.EnqueueRequest(Desc, bJoined, JoinedId);
        FSessionStartDescriptor DescOther = Desc;
        DescOther.SeedHex = TEXT("fedcba9876543210");
        const uint64 Op4 = Policy.EnqueueRequest(DescOther, bJoined, JoinedId);
        const TOptional<FGV2SessionOperationResult> Res3 = Policy.GetOutcome(Op3);
        TestTrue(TEXT("Op3 outcome is set"), Res3.IsSet());
        TestEqual(TEXT("Op3 is Superseded"), Res3->Outcome, ESessionOperationOutcome::Superseded);
        TestFalse(TEXT("Superseded has no fault"), Res3->Fault.IsSet());
        TestTrue(TEXT("Superseded fault code empty"), Res3->Fault.Code.IsEmpty());
    }

    // 2. Fault catalog enumerator: GetAllDeclaredFaultCodes() provides all declared codes
    const TArray<EGV2SessionFaultCode> DeclaredKinds = FGV2SessionFaultCodes::GetAllDeclaredFaultKinds();
    TestTrue(TEXT("Declared fault code enum is non-empty"), DeclaredKinds.Num() > 0);
    TSet<FString> UniqueCodes;
    for (const EGV2SessionFaultCode Kind : DeclaredKinds)
    {
        const FString Code = FGV2SessionFaultCodes::ToString(Kind);
        TestFalse(TEXT("Code is not empty"), Code.IsEmpty());
        TestFalse(*FString::Printf(TEXT("Code %s is unique"), *Code), UniqueCodes.Contains(Code));
        UniqueCodes.Add(Code);

        // Every declared code recorded via RecordFailure preserves code and message
        FGV2SessionTransitionPolicy Policy;
        const uint64 TestOp = Policy.AllocateOperationId();
        const FString TestMsg = FString::Printf(TEXT("Error detail for %s"), *Code);
        Policy.RecordFailure(TestOp, FGV2RequiredOperationFault{Kind, TestMsg});
        const TOptional<FGV2SessionOperationResult> FaultRes = Policy.GetOutcome(TestOp);
        TestTrue(TEXT("FaultRes is set"), FaultRes.IsSet());
        TestEqual(TEXT("Outcome is Failed"), FaultRes->Outcome, ESessionOperationOutcome::Failed);
        TestEqual(*FString::Printf(TEXT("Fault code matches %s"), *Code), FaultRes->Fault.Code, Code);
        TestEqual(*FString::Printf(TEXT("Fault message matches %s"), *Code), FaultRes->Fault.Message, TestMsg);
    }

    // Raw runtime/Lua diagnostics cannot become undeclared top-level operation codes.
    // The source code remains observable as CauseCode.
    {
        FGV2SessionTransitionPolicy Policy;
        const uint64 TestOp = Policy.AllocateOperationId();
        Policy.RecordRuntimeFailure(TestOp, GV2RuntimeCore::FRuntimeFault{"LuaCustomFailure", "runtime detail"});
        const TOptional<FGV2SessionOperationResult> FaultRes = Policy.GetOutcome(TestOp);
        TestTrue(TEXT("Raw runtime fault is recorded"), FaultRes.IsSet());
        TestEqual(TEXT("Raw runtime fault uses declared RuntimeFault code"), FaultRes->Fault.Code, FGV2SessionFaultCodes::RuntimeFault);
        TestEqual(TEXT("Raw runtime fault preserves distinguishable cause code"), FaultRes->Fault.CauseCode, TEXT("LuaCustomFailure"));
    }

    // 3. Coordinator achievable failure paths: each path produces typed fault matching declared codes
    {
        FGV2SessionCoordinator Coordinator;

        // Path A: RequestSave when !Status.bIsReady -> SessionNotReady
        const uint64 UnreadySaveOp = Coordinator.RequestSave(TEXT("myslot"));
        const TOptional<FGV2SessionOperationResult> UnreadySaveRes = Coordinator.GetSessionOperationOutcome(UnreadySaveOp);
        TestTrue(TEXT("UnreadySaveRes is set"), UnreadySaveRes.IsSet());
        TestEqual(TEXT("Unready save is Failed"), UnreadySaveRes->Outcome, ESessionOperationOutcome::Failed);
        TestEqual(TEXT("Fault code is SessionNotReady"), UnreadySaveRes->Fault.Code, FGV2SessionFaultCodes::SessionNotReady);

        // Path B: RequestLoad when Repo/Pkg is not set -> RepositoryNotReady
        const uint64 UnreadyLoadOp = Coordinator.RequestLoad(TEXT("myslot"), ESaveSlotRevision::Current);
        const TOptional<FGV2SessionOperationResult> UnreadyLoadRes = Coordinator.GetSessionOperationOutcome(UnreadyLoadOp);
        TestTrue(TEXT("UnreadyLoadRes is set"), UnreadyLoadRes.IsSet());
        TestEqual(TEXT("Unready load is Failed"), UnreadyLoadRes->Outcome, ESessionOperationOutcome::Failed);
        TestEqual(TEXT("Fault code is RepositoryNotReady"), UnreadyLoadRes->Fault.Code, FGV2SessionFaultCodes::RepositoryNotReady);

        // Path C: RequestSession with invalid descriptor -> InvalidSessionDescriptor
        AddExpectedError(
            TEXT("GV2 Lua runtime fault: code=InvalidSessionDescriptor"),
            EAutomationExpectedErrorFlags::Contains,
            1);
        FSessionStartDescriptor BadDesc;
        BadDesc.Mode = ESessionStartMode::Menu;
        BadDesc.RepositoryVersion = TEXT(""); // invalid
        GV2ContentCore::FRepositoryReadHandle DummyHandle;
        GV2ContentHostSupport::FResolvedPackageSet DummySet;
        const uint64 BadDescOp = Coordinator.RequestSession(BadDesc, DummyHandle, 1, DummySet);
        const TOptional<FGV2SessionOperationResult> BadDescRes = Coordinator.GetSessionOperationOutcome(BadDescOp);
        TestTrue(TEXT("BadDescRes is set"), BadDescRes.IsSet());
        TestEqual(TEXT("Bad desc is Failed"), BadDescRes->Outcome, ESessionOperationOutcome::Failed);
        TestEqual(TEXT("Fault code is InvalidSessionDescriptor"), BadDescRes->Fault.Code, FGV2SessionFaultCodes::InvalidSessionDescriptor);

        // Path D: RequestSession with valid descriptor but invalid repo handle -> RepositoryNotReady
        AddExpectedError(
            TEXT("GV2 Lua runtime fault: code=RepositoryNotReady"),
            EAutomationExpectedErrorFlags::Contains,
            1);
        FSessionStartDescriptor ValidDesc;
        ValidDesc.Mode = ESessionStartMode::Menu;
        ValidDesc.RepositoryVersion = TEXT("1");
        ValidDesc.RepositoryContentHash = TEXT("testhash");
        ValidDesc.SeedHex = FSessionStartDescriptor::GenerateFreshSeedHex();
        const uint64 BadRepoOp = Coordinator.RequestSession(ValidDesc, DummyHandle, 1, DummySet);
        const TOptional<FGV2SessionOperationResult> BadRepoRes = Coordinator.GetSessionOperationOutcome(BadRepoOp);
        TestTrue(TEXT("BadRepoRes is set"), BadRepoRes.IsSet());
        TestEqual(TEXT("Bad repo is Failed"), BadRepoRes->Outcome, ESessionOperationOutcome::Failed);
        TestEqual(TEXT("Fault code is RepositoryNotReady"), BadRepoRes->Fault.Code, FGV2SessionFaultCodes::RepositoryNotReady);
    }

    return true;
}

// SAC-05: Verifies that reachable session failure paths produce typed fault codes
// observable through the public UGV2RuntimeSubsystem API, and all fault codes
// belong to the structural FGV2SessionFaultCodes single source of truth.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2PublicSubsystemReachableFaultCodesTest,
    "GV2.Runtime.Session.PublicSubsystemReachableFaultCodes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2PublicSubsystemReachableFaultCodesTest::RunTest(const FString& Parameters)
{
    // 1. The public DTO has an empty state for non-failure/out parameters, while the
    // compile-time assertions above prove that the Failed construction token does not.
    {
        FGV2OperationFault DefaultFault;
        TestFalse(TEXT("Default constructed fault is not set"), DefaultFault.IsSet());
        TestTrue(TEXT("Default constructed fault Code is empty"), DefaultFault.Code.IsEmpty());

        const FGV2OperationFault ValidFault = FGV2RequiredOperationFault(
            EGV2SessionFaultCode::RuntimeFault,
            TEXT("Custom message"),
            TEXT("CustomTestCode")).ToDto();
        TestTrue(TEXT("Explicitly constructed fault is set"), ValidFault.IsSet());
        TestEqual(TEXT("Fault Code comes from the declared enum"), ValidFault.Code, FGV2SessionFaultCodes::RuntimeFault);
        TestEqual(TEXT("Runtime cause code is preserved"), ValidFault.CauseCode, TEXT("CustomTestCode"));
    }

    // 2. Structural single source of truth: verify all declared fault codes
    const TArray<EGV2SessionFaultCode> AllKinds = FGV2SessionFaultCodes::GetAllDeclaredFaultKinds();
    const TArray<FString> AllCodes = FGV2SessionFaultCodes::GetAllDeclaredFaultCodes();
    TestEqual(TEXT("Every declared enum kind produces one public code"), AllCodes.Num(), AllKinds.Num());
    TSet<FString> UniquePublicCodes;
    for (int32 Index = 0; Index < AllKinds.Num(); ++Index)
    {
        const FString& Code = AllCodes[Index];
        TestEqual(TEXT("Public code is derived from the enumerated kind"), Code, FGV2SessionFaultCodes::ToString(AllKinds[Index]));
        TestTrue(*FString::Printf(TEXT("Code '%s' is recognized by IsDeclared"), *Code),
            FGV2SessionFaultCodes::IsDeclared(Code));
        TestFalse(*FString::Printf(TEXT("Code '%s' is unique"), *Code), UniquePublicCodes.Contains(Code));
        UniquePublicCodes.Add(Code);
    }
    TestTrue(TEXT("UiSchemaNotReady is declared"), FGV2SessionFaultCodes::IsDeclared(FGV2SessionFaultCodes::UiSchemaNotReady));
    TestTrue(TEXT("ScreenRegistryNotReady is declared"), FGV2SessionFaultCodes::IsDeclared(FGV2SessionFaultCodes::ScreenRegistryNotReady));
    TestTrue(TEXT("ImageCatalogNotReady is declared"), FGV2SessionFaultCodes::IsDeclared(FGV2SessionFaultCodes::ImageCatalogNotReady));
    TestTrue(TEXT("ThemeNotReady is declared"), FGV2SessionFaultCodes::IsDeclared(FGV2SessionFaultCodes::ThemeNotReady));

    // 3. Test reachable codes through public UGV2RuntimeSubsystem paths
    const GV2PresentationTestFixtures::FGV2ScopedSamplePackageOverride SampleOverride;
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();

    UGV2RuntimeSubsystem* Runtime = GameInstance ? GameInstance->GetSubsystem<UGV2RuntimeSubsystem>() : nullptr;
    TestNotNull(TEXT("Runtime subsystem exists"), Runtime);
    if (Runtime == nullptr)
    {
        return false;
    }

    // Path 1: RequestSave when unready -> SessionNotReady
    {
        const int64 OpId = Runtime->RequestSave(TEXT("unready_slot"));
        TestTrue(TEXT("RequestSave returns valid operation id"), OpId > 0);
        ESessionOperationOutcome Outcome;
        FGV2OperationFault Fault;
        TestTrue(TEXT("Outcome available"), Runtime->GetSessionOperationOutcome(OpId, Outcome, Fault));
        TestEqual(TEXT("Outcome is Failed"), Outcome, ESessionOperationOutcome::Failed);
        TestEqual(TEXT("Fault code is SessionNotReady"), Fault.Code, FGV2SessionFaultCodes::SessionNotReady);
        TestTrue(TEXT("Fault code is declared"), FGV2SessionFaultCodes::IsDeclared(Fault.Code));

        FGV2SessionOperationResult QueryRes;
        const ESessionOperationQueryStatus QStatus = Runtime->QuerySessionOperation(OpId, QueryRes);
        TestEqual(TEXT("Query status is Found"), QStatus, ESessionOperationQueryStatus::Found);
        TestTrue(TEXT("Query result is failed"), QueryRes.IsFailed());
        TestEqual(TEXT("Query result fault code is SessionNotReady"), QueryRes.Fault.Code, FGV2SessionFaultCodes::SessionNotReady);
    }

    // Start session A so subsystem becomes Ready
    FWorldDelegates::OnStartGameInstance.Broadcast(GameInstance);
    TestTrue(TEXT("Session A is ready"), Runtime->GetSessionState().bIsReady);

    // Path 2: RequestSave with invalid slot id -> InvalidSaveSlotId
    {
        const int64 OpId = Runtime->RequestSave(TEXT("invalid/slash/slot"));
        TestTrue(TEXT("RequestSave returns valid operation id"), OpId > 0);
        ESessionOperationOutcome Outcome;
        FGV2OperationFault Fault;
        TestTrue(TEXT("Outcome available"), Runtime->GetSessionOperationOutcome(OpId, Outcome, Fault));
        TestEqual(TEXT("Outcome is Failed"), Outcome, ESessionOperationOutcome::Failed);
        TestEqual(TEXT("Fault code is InvalidSaveSlotId"), Fault.Code, FGV2SessionFaultCodes::InvalidSaveSlotId);
        TestTrue(TEXT("Fault code is declared"), FGV2SessionFaultCodes::IsDeclared(Fault.Code));
    }

    // Path 3: RequestLoad with non-existent slot id -> SaveSlotNotFound
    {
        AddExpectedErrorPlain(
            TEXT("GV2 Lua runtime fault: code=SaveSlotNotFound"),
            EAutomationExpectedErrorFlags::Contains,
            1);

        const int64 OpId = Runtime->RequestLoad(TEXT("nonexistent_slot_404"));
        TestTrue(TEXT("RequestLoad returns valid operation id"), OpId > 0);
        ESessionOperationOutcome Outcome;
        FGV2OperationFault Fault;
        TestTrue(TEXT("Outcome available"), Runtime->GetSessionOperationOutcome(OpId, Outcome, Fault));
        TestEqual(TEXT("Outcome is Failed"), Outcome, ESessionOperationOutcome::Failed);
        TestEqual(TEXT("Fault code is SaveSlotNotFound"), Fault.Code, FGV2SessionFaultCodes::SaveSlotNotFound);
        TestTrue(TEXT("Fault code is declared"), FGV2SessionFaultCodes::IsDeclared(Fault.Code));
    }

    // Path 4: RequestSession with invalid descriptor -> InvalidSessionDescriptor
    {
        AddExpectedErrorPlain(
            TEXT("GV2 Lua runtime fault: code=InvalidSessionDescriptor"),
            EAutomationExpectedErrorFlags::Contains,
            1);
        AddExpectedErrorPlain(
            TEXT("Failed to start GV2 session"),
            EAutomationExpectedErrorFlags::Contains,
            1);

        FSessionStartDescriptor BadDesc;
        BadDesc.Mode = ESessionStartMode::NewGame;
        BadDesc.SeedHex = TEXT("not_valid_hex_string");

        const int64 OpId = Runtime->RequestSession(BadDesc);
        TestTrue(TEXT("RequestSession returns valid operation id"), OpId > 0);
        ESessionOperationOutcome Outcome;
        FGV2OperationFault Fault;
        TestTrue(TEXT("Outcome available"), Runtime->GetSessionOperationOutcome(OpId, Outcome, Fault));
        TestEqual(TEXT("Outcome is Failed"), Outcome, ESessionOperationOutcome::Failed);
        TestEqual(TEXT("Fault code is InvalidSessionDescriptor"), Fault.Code, FGV2SessionFaultCodes::InvalidSessionDescriptor);
        TestTrue(TEXT("Fault code is declared"), FGV2SessionFaultCodes::IsDeclared(Fault.Code));
    }

    // Path 5: RequestSession with repository forced not ready -> RepositoryNotReady
    {
        AddExpectedErrorPlain(
            TEXT("GV2 Lua runtime fault: code=RepositoryNotReady"),
            EAutomationExpectedErrorFlags::Contains,
            1);
        AddExpectedErrorPlain(
            TEXT("Failed to start GV2 session"),
            EAutomationExpectedErrorFlags::Contains,
            1);

        UGV2RuntimeSubsystem::bTestForceRepositoryNotReady = true;

        FSessionStartDescriptor ValidDesc;
        ValidDesc.Mode = ESessionStartMode::NewGame;
        ValidDesc.SeedHex = FSessionStartDescriptor::GenerateFreshSeedHex();

        const int64 OpId = Runtime->RequestSession(ValidDesc);
        UGV2RuntimeSubsystem::bTestForceRepositoryNotReady = false;

        TestTrue(TEXT("RequestSession returns valid operation id"), OpId > 0);
        ESessionOperationOutcome Outcome;
        FGV2OperationFault Fault;
        TestTrue(TEXT("Outcome available"), Runtime->GetSessionOperationOutcome(OpId, Outcome, Fault));
        TestEqual(TEXT("Outcome is Failed"), Outcome, ESessionOperationOutcome::Failed);
        TestEqual(TEXT("Fault code is RepositoryNotReady"), Fault.Code, FGV2SessionFaultCodes::RepositoryNotReady);
        TestTrue(TEXT("Fault code is declared"), FGV2SessionFaultCodes::IsDeclared(Fault.Code));
    }

    // Path 6: RequestSession with missing ThemeAsset -> ThemeNotReady
    {
        AddExpectedErrorPlain(
            TEXT("GV2 Lua runtime fault: code=ThemeNotReady"),
            EAutomationExpectedErrorFlags::Contains,
            1);
        AddExpectedErrorPlain(
            TEXT("Failed to start GV2 session"),
            EAutomationExpectedErrorFlags::Contains,
            1);

        UGV2UiThemeSettings* ThemeSettings = GetMutableDefault<UGV2UiThemeSettings>();
        const TSoftObjectPtr<UGV2UiTheme> SavedTheme = ThemeSettings->ThemeAsset;
        ThemeSettings->ThemeAsset = nullptr;

        FSessionStartDescriptor ValidDesc;
        ValidDesc.Mode = ESessionStartMode::NewGame;
        ValidDesc.SeedHex = FSessionStartDescriptor::GenerateFreshSeedHex();

        const int64 OpId = Runtime->RequestSession(ValidDesc);
        ThemeSettings->ThemeAsset = SavedTheme;

        TestTrue(TEXT("RequestSession returns valid operation id"), OpId > 0);
        ESessionOperationOutcome Outcome;
        FGV2OperationFault Fault;
        TestTrue(TEXT("Outcome available"), Runtime->GetSessionOperationOutcome(OpId, Outcome, Fault));
        TestEqual(TEXT("Outcome is Failed"), Outcome, ESessionOperationOutcome::Failed);
        TestEqual(TEXT("Fault code is ThemeNotReady"), Fault.Code, FGV2SessionFaultCodes::ThemeNotReady);
        TestTrue(TEXT("Fault code is declared"), FGV2SessionFaultCodes::IsDeclared(Fault.Code));
    }

    // Path 7: RequestSession with missing RegistryAsset -> ScreenRegistryNotReady
    {
        AddExpectedErrorPlain(
            TEXT("GV2 Lua runtime fault: code=ScreenRegistryNotReady"),
            EAutomationExpectedErrorFlags::Contains,
            1);
        AddExpectedErrorPlain(
            TEXT("Failed to start GV2 session"),
            EAutomationExpectedErrorFlags::Contains,
            1);

        UGV2ScreenRegistrySettings* RegSettings = GetMutableDefault<UGV2ScreenRegistrySettings>();
        const TSoftObjectPtr<UGV2ScreenRegistry> SavedReg = RegSettings->RegistryAsset;
        RegSettings->RegistryAsset = nullptr;

        FSessionStartDescriptor ValidDesc;
        ValidDesc.Mode = ESessionStartMode::NewGame;
        ValidDesc.SeedHex = FSessionStartDescriptor::GenerateFreshSeedHex();

        const int64 OpId = Runtime->RequestSession(ValidDesc);
        RegSettings->RegistryAsset = SavedReg;

        TestTrue(TEXT("RequestSession returns valid operation id"), OpId > 0);
        ESessionOperationOutcome Outcome;
        FGV2OperationFault Fault;
        TestTrue(TEXT("Outcome available"), Runtime->GetSessionOperationOutcome(OpId, Outcome, Fault));
        TestEqual(TEXT("Outcome is Failed"), Outcome, ESessionOperationOutcome::Failed);
        TestEqual(TEXT("Fault code is ScreenRegistryNotReady"), Fault.Code, FGV2SessionFaultCodes::ScreenRegistryNotReady);
        TestTrue(TEXT("Fault code is declared"), FGV2SessionFaultCodes::IsDeclared(Fault.Code));
    }

    Runtime->EndSession();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionOperationRetentionAndEvictionTest,
    "GV2.Runtime.Session.OperationRetentionAndEviction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionOperationRetentionAndEvictionTest::RunTest(const FString& Parameters)
{
    // 1. Verify default capacity constant is 160 based on 2-hour session profile calculation
    TestEqual(TEXT("DefaultMaxRetainedOutcomes is 160"), FGV2SessionTransitionPolicy::DefaultMaxRetainedOutcomes, 160);
    {
        FGV2SessionTransitionPolicy DefaultPolicy;
        TestEqual(TEXT("Default constructed policy capacity is 160"), DefaultPolicy.GetMaxRetainedOutcomes(), 160);
        TestEqual(TEXT("Initial retained count is 0"), DefaultPolicy.GetRetainedOutcomesCount(), 0);
        TestEqual(TEXT("Initial highest evicted ID is 0"), DefaultPolicy.GetHighestEvictedOperationId(), static_cast<uint64>(0));
    }

    // 2. Bounded retention with capacity = 3
    FGV2SessionTransitionPolicy Policy(3);
    TestEqual(TEXT("Configured capacity is 3"), Policy.GetMaxRetainedOutcomes(), 3);

    // Initial state: unknown operations
    TestEqual(TEXT("Op 0 is Unknown"), Policy.QueryOutcome(0), ESessionOperationQueryStatus::Unknown);
    TestEqual(TEXT("Op 1 before allocation is Unknown"), Policy.QueryOutcome(1), ESessionOperationQueryStatus::Unknown);
    TestEqual(TEXT("Op 999 is Unknown"), Policy.QueryOutcome(999), ESessionOperationQueryStatus::Unknown);
    TestFalse(TEXT("Op 0 is not evicted"), Policy.IsOperationEvicted(0));
    TestFalse(TEXT("Op 1 is not evicted"), Policy.IsOperationEvicted(1));
    TestFalse(TEXT("Op 999 is not evicted"), Policy.IsOperationEvicted(999));
    TestFalse(TEXT("Op 0 is not known"), Policy.IsOperationKnown(0));
    TestFalse(TEXT("Op 1 is not known"), Policy.IsOperationKnown(1));

    // Allocate Op 1, 2, 3
    const uint64 Op1 = Policy.AllocateOperationId();
    const uint64 Op2 = Policy.AllocateOperationId();
    const uint64 Op3 = Policy.AllocateOperationId();
    TestEqual(TEXT("Op1 is 1"), Op1, static_cast<uint64>(1));
    TestEqual(TEXT("Op2 is 2"), Op2, static_cast<uint64>(2));
    TestEqual(TEXT("Op3 is 3"), Op3, static_cast<uint64>(3));

    // Allocated but not recorded -> InProgress
    TestEqual(TEXT("Op1 before recording is InProgress"), Policy.QueryOutcome(Op1), ESessionOperationQueryStatus::InProgress);
    TestEqual(TEXT("Op2 before recording is InProgress"), Policy.QueryOutcome(Op2), ESessionOperationQueryStatus::InProgress);
    TestEqual(TEXT("Op3 before recording is InProgress"), Policy.QueryOutcome(Op3), ESessionOperationQueryStatus::InProgress);
    TestTrue(TEXT("Op1 is known"), Policy.IsOperationKnown(Op1));
    TestFalse(TEXT("Op1 is not evicted"), Policy.IsOperationEvicted(Op1));

    // Record outcomes for Op1, Op2, Op3
    Policy.RecordOutcome(Op1, ESessionNonFailureOutcome::Completed);
    Policy.RecordFailure(Op2, FGV2RequiredOperationFault{EGV2SessionFaultCode::InvalidSessionDescriptor, TEXT("Bad descriptor")});
    Policy.RecordOutcome(Op3, ESessionNonFailureOutcome::Cancelled);

    TestEqual(TEXT("Retained count is 3"), Policy.GetRetainedOutcomesCount(), 3);
    TestEqual(TEXT("Highest evicted ID is still 0"), Policy.GetHighestEvictedOperationId(), static_cast<uint64>(0));

    // Check all 3 are Found and readable
    FGV2SessionOperationResult Res1, Res2, Res3;
    TestEqual(TEXT("Op1 is Found"), Policy.QueryOutcome(Op1, &Res1), ESessionOperationQueryStatus::Found);
    TestEqual(TEXT("Res1 is Completed"), Res1.Outcome, ESessionOperationOutcome::Completed);
    TestFalse(TEXT("Res1 has no fault"), Res1.Fault.IsSet());

    TestEqual(TEXT("Op2 is Found"), Policy.QueryOutcome(Op2, &Res2), ESessionOperationQueryStatus::Found);
    TestEqual(TEXT("Res2 is Failed"), Res2.Outcome, ESessionOperationOutcome::Failed);
    TestEqual(TEXT("Res2 has fault code"), Res2.Fault.Code, FGV2SessionFaultCodes::InvalidSessionDescriptor);

    TestEqual(TEXT("Op3 is Found"), Policy.QueryOutcome(Op3, &Res3), ESessionOperationQueryStatus::Found);
    TestEqual(TEXT("Res3 is Cancelled"), Res3.Outcome, ESessionOperationOutcome::Cancelled);
    TestFalse(TEXT("Res3 has no fault"), Res3.Fault.IsSet());

    // 3. Overflow boundary: record Op4 -> Op1 (earliest by OperationId) MUST be evicted
    const uint64 Op4 = Policy.AllocateOperationId();
    Policy.RecordOutcome(Op4, ESessionNonFailureOutcome::Completed);

    TestEqual(TEXT("Retained count remains capped at 3"), Policy.GetRetainedOutcomesCount(), 3);
    TestEqual(TEXT("Highest evicted ID is Op1"), Policy.GetHighestEvictedOperationId(), Op1);

    // Op1 is now Evicted (observable distinction!)
    TestTrue(TEXT("Op1 is evicted"), Policy.IsOperationEvicted(Op1));
    TestTrue(TEXT("Op1 is still known"), Policy.IsOperationKnown(Op1));
    TestEqual(TEXT("Op1 query status is Evicted"), Policy.QueryOutcome(Op1), ESessionOperationQueryStatus::Evicted);
    TestFalse(TEXT("Op1 GetOutcome is unset"), Policy.GetOutcome(Op1).IsSet());

    // Op2, Op3, Op4 remain Found and readable
    TestFalse(TEXT("Op2 is not evicted"), Policy.IsOperationEvicted(Op2));
    TestEqual(TEXT("Op2 query status is Found"), Policy.QueryOutcome(Op2, &Res2), ESessionOperationQueryStatus::Found);
    TestEqual(TEXT("Res2 is Failed"), Res2.Outcome, ESessionOperationOutcome::Failed);

    TestFalse(TEXT("Op3 is not evicted"), Policy.IsOperationEvicted(Op3));
    TestEqual(TEXT("Op3 query status is Found"), Policy.QueryOutcome(Op3, &Res3), ESessionOperationQueryStatus::Found);
    TestEqual(TEXT("Res3 is Cancelled"), Res3.Outcome, ESessionOperationOutcome::Cancelled);

    TestFalse(TEXT("Op4 is not evicted"), Policy.IsOperationEvicted(Op4));
    FGV2SessionOperationResult Res4;
    TestEqual(TEXT("Op4 query status is Found"), Policy.QueryOutcome(Op4, &Res4), ESessionOperationQueryStatus::Found);
    TestEqual(TEXT("Res4 is Completed"), Res4.Outcome, ESessionOperationOutcome::Completed);

    // Unknown operation comparison: Op999 is NOT evicted, it is Unknown
    TestFalse(TEXT("Op999 is not evicted"), Policy.IsOperationEvicted(999));
    TestFalse(TEXT("Op999 is not known"), Policy.IsOperationKnown(999));
    TestEqual(TEXT("Op999 query status is Unknown"), Policy.QueryOutcome(999), ESessionOperationQueryStatus::Unknown);

    // 4. Out-of-order completion: allocate Op5, Op6. Complete Op6 first, then Op5.
    const uint64 Op5 = Policy.AllocateOperationId();
    const uint64 Op6 = Policy.AllocateOperationId();

    // Map currently holds {2, 3, 4}. Capacity is 3.
    // Recording Op6 will evict lowest OpId in map, which is Op2!
    Policy.RecordOutcome(Op6, ESessionNonFailureOutcome::Completed);
    TestEqual(TEXT("Retained count remains capped at 3"), Policy.GetRetainedOutcomesCount(), 3);
    TestTrue(TEXT("Op2 is evicted"), Policy.IsOperationEvicted(Op2));
    TestEqual(TEXT("Op2 is Evicted status"), Policy.QueryOutcome(Op2), ESessionOperationQueryStatus::Evicted);
    TestEqual(TEXT("Highest evicted ID is Op2"), Policy.GetHighestEvictedOperationId(), Op2);

    // Now record Op5. Map currently holds {3, 4, 6}.
    // Adding Op5 (ID 5) will evict lowest OpId in map, which is Op3!
    Policy.RecordOutcome(Op5, ESessionNonFailureOutcome::Completed);
    TestEqual(TEXT("Retained count remains capped at 3"), Policy.GetRetainedOutcomesCount(), 3);
    TestTrue(TEXT("Op3 is evicted"), Policy.IsOperationEvicted(Op3));
    TestEqual(TEXT("Op3 is Evicted status"), Policy.QueryOutcome(Op3), ESessionOperationQueryStatus::Evicted);
    TestEqual(TEXT("Highest evicted ID is Op3"), Policy.GetHighestEvictedOperationId(), Op3);

    // Retained must be {4, 5, 6}
    TestEqual(TEXT("Op4 is Found"), Policy.QueryOutcome(Op4), ESessionOperationQueryStatus::Found);
    TestEqual(TEXT("Op5 is Found"), Policy.QueryOutcome(Op5), ESessionOperationQueryStatus::Found);
    TestEqual(TEXT("Op6 is Found"), Policy.QueryOutcome(Op6), ESessionOperationQueryStatus::Found);

    // 5. Out-of-order counterexample demonstrating the fix for the HighestEvictedOperationId watermark flaw:
    // When operations are allocated 1..5, but completed in order 5, 4, 3, 2, operation 1 is STILL in progress.
    // The previous watermark logic (OpId <= HighestEvictedOperationId) incorrectly classified Op1 as Evicted.
    // With InProgressOperations, Op1 is correctly classified as InProgress.
    {
        FGV2SessionTransitionPolicy OooPolicy(2); // Capacity = 2
        const uint64 P1 = OooPolicy.AllocateOperationId(); // 1
        const uint64 P2 = OooPolicy.AllocateOperationId(); // 2
        const uint64 P3 = OooPolicy.AllocateOperationId(); // 3
        const uint64 P4 = OooPolicy.AllocateOperationId(); // 4
        const uint64 P5 = OooPolicy.AllocateOperationId(); // 5

        TestEqual(TEXT("P1 is InProgress"), OooPolicy.QueryOutcome(P1), ESessionOperationQueryStatus::InProgress);
        TestFalse(TEXT("P1 is not evicted"), OooPolicy.IsOperationEvicted(P1));
        TestTrue(TEXT("P1 is in progress set"), OooPolicy.IsOperationInProgress(P1));
        TestEqual(TEXT("5 in progress operations"), OooPolicy.GetInProgressOperationsCount(), 5);

        // Complete 5, 4, 3, 2 out-of-order
        OooPolicy.RecordOutcome(P5, ESessionNonFailureOutcome::Completed);
        TestEqual(TEXT("P5 is Found"), OooPolicy.QueryOutcome(P5), ESessionOperationQueryStatus::Found);
        TestFalse(TEXT("P5 no longer in progress"), OooPolicy.IsOperationInProgress(P5));

        OooPolicy.RecordOutcome(P4, ESessionNonFailureOutcome::Completed);
        TestEqual(TEXT("P4 is Found"), OooPolicy.QueryOutcome(P4), ESessionOperationQueryStatus::Found);

        // Capacity is 2: map had {4, 5}. Recording P3 evicts P4 (earliest in map).
        OooPolicy.RecordOutcome(P3, ESessionNonFailureOutcome::Completed);
        TestEqual(TEXT("P4 was evicted"), OooPolicy.QueryOutcome(P4), ESessionOperationQueryStatus::Evicted);
        TestTrue(TEXT("P4 is evicted"), OooPolicy.IsOperationEvicted(P4));

        // Recording P2 evicts P3.
        OooPolicy.RecordOutcome(P2, ESessionNonFailureOutcome::Completed);
        TestEqual(TEXT("P3 was evicted"), OooPolicy.QueryOutcome(P3), ESessionOperationQueryStatus::Evicted);
        TestTrue(TEXT("P3 is evicted"), OooPolicy.IsOperationEvicted(P3));

        // CRITICAL CHECK: P1 was NEVER recorded. It must still be InProgress, NOT Evicted!
        TestEqual(TEXT("P1 remains InProgress despite P3 and P4 being evicted"), OooPolicy.QueryOutcome(P1), ESessionOperationQueryStatus::InProgress);
        TestFalse(TEXT("P1 is NOT evicted"), OooPolicy.IsOperationEvicted(P1));
        TestTrue(TEXT("P1 is still in progress"), OooPolicy.IsOperationInProgress(P1));

        // P2 and P5 are Found
        TestEqual(TEXT("P2 is Found"), OooPolicy.QueryOutcome(P2), ESessionOperationQueryStatus::Found);
        TestEqual(TEXT("P5 is Found"), OooPolicy.QueryOutcome(P5), ESessionOperationQueryStatus::Found);

        // Finally complete P1
        OooPolicy.RecordOutcome(P1, ESessionNonFailureOutcome::Completed);
        TestEqual(TEXT("P1 is now Found"), OooPolicy.QueryOutcome(P1), ESessionOperationQueryStatus::Found);
        TestFalse(TEXT("P1 no longer in progress"), OooPolicy.IsOperationInProgress(P1));
        TestEqual(TEXT("0 in progress operations"), OooPolicy.GetInProgressOperationsCount(), 0);
    }

    // 6. Coordinator integration
    {
        FGV2SessionCoordinator Coordinator;
        const uint64 UnreadyOp = Coordinator.RequestSave(TEXT("any_slot"));
        TestTrue(TEXT("Coordinator recorded outcome"), Coordinator.GetSessionOperationOutcome(UnreadyOp).IsSet());
        TestFalse(TEXT("Coordinator op is not evicted"), Coordinator.IsSessionOperationEvicted(UnreadyOp));
        FGV2SessionOperationResult CoordRes;
        TestEqual(TEXT("Coordinator query status is Found"), Coordinator.QuerySessionOperationOutcome(UnreadyOp, &CoordRes), ESessionOperationQueryStatus::Found);
        TestEqual(TEXT("Coordinator op is Failed"), CoordRes.Outcome, ESessionOperationOutcome::Failed);

        TestFalse(TEXT("Unknown op 999 is not evicted on coordinator"), Coordinator.IsSessionOperationEvicted(999));
        TestEqual(TEXT("Unknown op 999 is Unknown on coordinator"), Coordinator.QuerySessionOperationOutcome(999), ESessionOperationQueryStatus::Unknown);
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
