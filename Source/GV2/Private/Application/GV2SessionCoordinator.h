#pragma once

#include "Application/GV2SessionContentSnapshot.h"
#include "Application/GV2SessionTransition.h"
#include "Bridge/GV2RuntimeIngressQueue.h"
#include "Bridge/GV2UiBindingRegistry.h"
#include "GV2ContentCore/RepositorySnapshot.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"

class FGV2SessionCoordinator
{
public:
    using FInteractionSink = TFunction<void(const FGV2UiIngressItem&)>;
    using FDocumentSink = TFunction<bool(
        const FGV2UiDocumentViewModel&,
        const FGV2PresentationPrepareContext&)>;
    using FProjectionTeardownSink = TFunction<void()>;
    using FProjectionPublishSink = TFunction<void()>;

    explicit FGV2SessionCoordinator(int32 InIngressCapacity = 256);

    void SetInteractionSink(FInteractionSink InSink);
    void ClearInteractionSink();
    void SetDocumentSink(FDocumentSink InSink);
    void ClearDocumentSink();
    void SetProjectionTeardownSink(FProjectionTeardownSink InSink);
    void ClearProjectionTeardownSink();
    void SetProjectionPublishSink(FProjectionPublishSink InSink);
    void ClearProjectionPublishSink();

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
    // CFC-07: Public lifecycle requests
    uint64 RequestSession(
        const FSessionStartDescriptor& Descriptor,
        const GV2ContentCore::FRepositoryReadHandle& InPinnedRepository,
        int64 InRepositoryVersion,
        const GV2ContentHostSupport::FResolvedPackageSet& ResolvedPackageSet);

    ESessionCancellationResult CancelSessionRequest(uint64 OperationId);

    TOptional<ESessionOperationOutcome> GetSessionOperationOutcome(uint64 OperationId) const;

    FGV2SessionTransitionPolicy& GetTransitionPolicy() { return TransitionPolicy; }
    const FGV2SessionTransitionPolicy& GetTransitionPolicy() const { return TransitionPolicy; }

    void FailBootstrap(const FString& Code, const FString& Message);
    void EndSession(EGV2SessionState FinalState = EGV2SessionState::Destroyed);

    const FGV2SessionStatus& GetStatus() const;
    const GV2ContentCore::FRepositoryReadHandle& GetPinnedRepository() const { return PinnedRepository; }

    // PSC-04/05 (ADR-0043 D1): null before a successful StartSession() and after
    // EndSession()/a failed StartSession() -- this is the EXTERNAL-facing contract: only
    // observable atomically with Ready, never for a session whose bootstrap ultimately
    // fails, even if the failure happens after the candidate itself was already valid.
    const FGV2SessionContentSnapshot* GetContentSnapshot() const { return ContentSnapshot.Get(); }

    enum class EReplacementStage
    {
        Preflight,
        Replacing,
        Preparing,
        Committed,
        Aborted
    };

    class FSessionReplacementToken
    {
    public:
        ~FSessionReplacementToken() = default;
        FSessionReplacementToken(FSessionReplacementToken&&) = default;
        FSessionReplacementToken& operator=(FSessionReplacementToken&&) = default;
        FSessionReplacementToken(const FSessionReplacementToken&) = delete;
        FSessionReplacementToken& operator=(const FSessionReplacementToken&) = delete;

        EReplacementStage GetStage() const { return Stage; }
        const FGV2SessionContentSnapshot& GetCandidate() const { check(Candidate.IsValid()); return *Candidate; }
        FGV2SessionContentSnapshot& GetCandidate() { check(Candidate.IsValid()); return *Candidate; }

    private:
        friend class FGV2SessionCoordinator;
        explicit FSessionReplacementToken(TUniquePtr<FGV2SessionContentSnapshot> InCandidate)
            : Candidate(MoveTemp(InCandidate))
            , Stage(EReplacementStage::Preflight)
        {
            check(Candidate.IsValid());
        }

        void TransitionTo(EReplacementStage NewStage)
        {
            Stage = NewStage;
        }

        TUniquePtr<FGV2SessionContentSnapshot> TakeCandidate()
        {
            check(Stage == EReplacementStage::Preparing || Stage == EReplacementStage::Committed);
            return MoveTemp(Candidate);
        }

        TUniquePtr<FGV2SessionContentSnapshot> Candidate;
        EReplacementStage Stage = EReplacementStage::Preflight;
    };

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
        FGV2PreparedBindingSet& OutBindings,
        const FGV2PresentationPrepareContext& PrepareContext);
    void PumpIngress();
    void FailRuntime(const GV2RuntimeCore::FRuntimeFault& Fault);

    bool BeginReplace(
        FSessionReplacementToken& Token,
        const GV2ContentCore::FRepositoryReadHandle& InPinnedRepository,
        int64 InRepositoryVersion,
        GV2RuntimeCore::FRuntimeFault& OutFault,
        ESessionTransitionKind TransitionKind);

    bool PublishReady(
        FSessionReplacementToken&& Token,
        int64 InUiRevision,
        ESessionTransitionKind TransitionKind);

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

    struct FPendingStartContext
    {
        GV2ContentCore::FRepositoryReadHandle PinnedRepository;
        int64 RepositoryVersion = 0;
        GV2ContentHostSupport::FResolvedPackageSet ResolvedPackageSet;
    };

    bool ExecuteSessionStart(
        const FSessionOperationRecord& Op,
        const GV2ContentCore::FRepositoryReadHandle& InPinnedRepository,
        int64 InRepositoryVersion,
        const GV2ContentHostSupport::FResolvedPackageSet& InResolvedPackageSet);

    void ExecuteShutdown(const FSessionOperationRecord& Op, EGV2SessionState FinalState = EGV2SessionState::Destroyed);
    void ProcessNextTransition();

    FGV2SessionStatus Status;
    GV2ContentCore::FRepositoryReadHandle PinnedRepository;
    TUniquePtr<FGV2SessionContentSnapshot> ContentSnapshot;
    FGV2UiBindingRegistry BindingRegistry;
    FGV2RuntimeIngressQueue IngressQueue;
    GV2RuntimeCore::FRuntimeSession RuntimeSession;
    FGV2SessionTransitionPolicy TransitionPolicy;
    TOptional<FPendingStartContext> PendingStartContext;
    FInteractionSink InteractionSink;
    FDocumentSink DocumentSink;
    FProjectionTeardownSink ProjectionTeardownSink;
    FProjectionPublishSink ProjectionPublishSink;
    TMap<FString, FString> ActiveTabsByContainerPath;
    int64 NextInputSequence = 1;
    int64 UiRevision = 0;
    bool bPumpingIngress = false;
    bool bExecutingRuntime = false;
    bool bProcessingTransition = false;
    ESessionTransitionKind CurrentTransitionKind = ESessionTransitionKind::NewGame;
};
