#pragma once

#include "Application/GV2SessionContentSnapshot.h"
#include "Bridge/GV2RuntimeIngressQueue.h"
#include "Bridge/GV2UiBindingRegistry.h"
#include "GV2ContentCore/RepositorySnapshot.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"

class FGV2SessionCoordinator
{
public:
    using FInteractionSink = TFunction<void(const FGV2UiIngressItem&)>;
    using FDocumentSink = TFunction<bool(const FGV2UiDocumentViewModel&)>;

    explicit FGV2SessionCoordinator(int32 InIngressCapacity = 256);

    void SetInteractionSink(FInteractionSink InSink);
    void ClearInteractionSink();
    void SetDocumentSink(FDocumentSink InSink);
    void ClearDocumentSink();

    // PCC-36: PinnedRepository must be a valid read handle obtained from the
    // Application-scope FGV2RepositoryPublisher current snapshot at the time
    // of this call. It is held for the whole session lifetime and is never
    // swapped for a later Application-level republish (BootstrapAndSessionLifecycle.md
    // "Active session никогда не переключает pinned handle").
    // PSC-02 (ADR-0043 D1/D5): ResolvedPackageSet is the caller's single already-resolved
    // package set (UGV2RuntimeSubsystem::Initialize) -- when given, Lua/schema source
    // loading reads it directly instead of re-discovering the package closure. It is
    // required: an Application-layer fallback would be a second package authority.
    bool StartSession(
        const GV2ContentCore::FRepositoryReadHandle& PinnedRepository,
        int64 RepositoryVersion,
        const GV2ContentHostSupport::FResolvedPackageSet& ResolvedPackageSet);
#if WITH_DEV_AUTOMATION_TESTS
    // Test convenience only. Resolves the shipped fixture closure and delegates to the
    // production overload above; this declaration is absent from non-test builds.
    bool StartSession(
        const GV2ContentCore::FRepositoryReadHandle& PinnedRepository,
        int64 RepositoryVersion);
#endif
    void FailBootstrap(const FString& Code, const FString& Message);
    void EndSession(EGV2SessionState FinalState = EGV2SessionState::Destroyed);

    const FGV2SessionStatus& GetStatus() const;
    const GV2ContentCore::FRepositoryReadHandle& GetPinnedRepository() const { return PinnedRepository; }

    // PSC-04/05 (ADR-0043 D1): null before a successful StartSession() and after
    // EndSession()/a failed StartSession() -- this is the EXTERNAL-facing contract: only
    // observable atomically with Ready, never for a session whose bootstrap ultimately
    // fails, even if the failure happens after the candidate itself was already valid.
    const FGV2SessionContentSnapshot* GetContentSnapshot() const { return ContentSnapshot.Get(); }

    // PSC-06 (ADR-0043 D1): for this coordinator's OWN internal Prepare-phase machinery
    // ONLY (the ScreenFactory callback UGV2RuntimeSubsystem passes into the Reconciler,
    // reached synchronously from DocumentSink -- which StartSession() itself invokes,
    // before Ready, to prepare/commit the initial document). Returns the published
    // snapshot once Ready (same value as GetContentSnapshot()), or the in-progress
    // candidate while StartSession() is still executing its own bootstrap -- the initial
    // document's own screen/resource resolution legitimately needs the candidate that
    // will become this session's snapshot, before that candidate is externally observable.
    // Never use this from outside the coordinator's own callback machinery -- any other
    // caller must use GetContentSnapshot(), which stays strictly Ready-gated.
    const FGV2SessionContentSnapshot* GetContentSnapshotForPrepare() const
    {
        return ContentSnapshot ? ContentSnapshot.Get() : InProgressCandidate;
    }

    bool PublishUiBindings(
        const FString& UiInstanceId,
        int64 Revision,
        const TArray<FGV2UiBindingDefinition>& Definitions,
        TArray<FGV2UiBindingHandle>& OutHandles);

    bool PublishScreenBindings(
        const TArray<FGV2UiBindingDefinition>& Definitions,
        TArray<FGV2UiBindingHandle>& OutHandles);

    EGV2SubmitUiInteractionResult SubmitUiInteraction(
        const FGV2UiBindingHandle& BindingHandle,
        const TArray<FGV2UiControlValue>& InputValues);

    void SetActiveTab(const FString& ContainerPath, const FString& TabKey);
    FString GetActiveTab(const FString& ContainerPath) const;

    int64 GetUiRevision() const { return UiRevision; }
    const FGV2UiBindingRegistry& GetBindingRegistry() const { return BindingRegistry; }
    EGV2BindingResolveResult ResolveBinding(
        const FGV2UiBindingHandle& Handle,
        FGV2UiBindingRecord& OutRecord) const
    {
        return BindingRegistry.Resolve(Handle, OutRecord);
    }

    bool IsExecutingRuntime() const;
    bool IsLuaVmStarted() const;
    int32 GetQueuedIngressCount() const;

#if WITH_DEV_AUTOMATION_TESTS
    // Test-only hook (CBM-03: "runs that need the demo screen connect sample
    // explicitly"): GameData/sample is deliberately excluded from
    // mods.lock.json5's default package set, so UI automation tests that
    // need its demo screen/debug-start module opt in here instead of
    // mutating the shipped lock file.
    static bool bTestForceIncludeSamplePackage;
#endif

private:
    static bool ValidateInputValues(
        const FGV2UiBindingRecord& Binding,
        const TArray<FGV2UiControlValue>& InputValues);
    bool PrepareDocumentRequest(
        const GV2RuntimeCore::FUiDocument& Document,
        FGV2UiDocumentViewModel& OutModel,
        FGV2PreparedBindingSet& OutBindings);
    void PumpIngress();
    void FailRuntime(const GV2RuntimeCore::FRuntimeFault& Fault);

    // PSC-05 (ADR-0042/BootstrapAndSessionLifecycle.md "Session states"): used only for a
    // failure that occurs BEFORE StartSession commits to tearing down whatever session was
    // previously active (repository validity, Lua source loading, content candidate build --
    // none of these have touched RuntimeSession/BindingRegistry/PinnedRepository/
    // ContentSnapshot yet). When bHadPriorReadySession is true, this leaves every one of
    // those completely untouched -- the prior active session/snapshot stays observably
    // unchanged, per PSC-05's invariant. When false (nothing valid to preserve), it still
    // transitions Status to Failed, matching the state diagram's "Any build phase -> Failed"
    // -- a failed attempt is never silently indistinguishable from "no session was ever
    // started".
    void FailReplacementAttempt(const GV2RuntimeCore::FRuntimeFault& Fault, bool bHadPriorReadySession);

    FGV2SessionStatus Status;
    GV2ContentCore::FRepositoryReadHandle PinnedRepository;
    TUniquePtr<FGV2SessionContentSnapshot> ContentSnapshot;
    // PSC-06: non-owning, set to the in-progress Candidate right after RuntimeSession::Start
    // succeeds (before the document pipeline runs), cleared the instant this attempt is
    // either published (ownership moves to ContentSnapshot) or fails (FailRuntime). See
    // GetContentSnapshotForPrepare()'s doc comment.
    const FGV2SessionContentSnapshot* InProgressCandidate = nullptr;
    FGV2UiBindingRegistry BindingRegistry;
    FGV2RuntimeIngressQueue IngressQueue;
    GV2RuntimeCore::FRuntimeSession RuntimeSession;
    FInteractionSink InteractionSink;
    FDocumentSink DocumentSink;
    TMap<FString, FString> ActiveTabsByContainerPath;
    int64 NextInputSequence = 1;
    int64 UiRevision = 0;
    bool bPumpingIngress = false;
    bool bExecutingRuntime = false;
};
