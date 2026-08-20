#pragma once

#include "Bridge/GV2RuntimeIngressQueue.h"
#include "Bridge/GV2UiBindingRegistry.h"
#include "GV2ContentCore/RepositorySnapshot.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"

class FGV2SessionCoordinator
{
public:
    using FInteractionSink = TFunction<void(const FGV2UiIngressItem&)>;
    using FScreenSink = TFunction<bool(const FGV2ScreenViewModel&)>;
    using FDocumentSink = TFunction<bool(const FGV2UiDocumentViewModel&)>;

    explicit FGV2SessionCoordinator(int32 InIngressCapacity = 256);

    void SetInteractionSink(FInteractionSink InSink);
    void ClearInteractionSink();
    void SetScreenSink(FScreenSink InSink);
    void ClearScreenSink();
    void SetDocumentSink(FDocumentSink InSink);
    void ClearDocumentSink();

    // PCC-36: PinnedRepository must be a valid read handle obtained from the
    // Application-scope FGV2RepositoryPublisher current snapshot at the time
    // of this call. It is held for the whole session lifetime and is never
    // swapped for a later Application-level republish (BootstrapAndSessionLifecycle.md
    // "Active session никогда не переключает pinned handle").
    bool StartSession(
        const GV2ContentCore::FRepositoryReadHandle& PinnedRepository,
        int64 RepositoryVersion);
    void FailBootstrap(const FString& Code, const FString& Message);
    void EndSession(EGV2SessionState FinalState = EGV2SessionState::Destroyed);

    const FGV2SessionStatus& GetStatus() const;
    const GV2ContentCore::FRepositoryReadHandle& GetPinnedRepository() const { return PinnedRepository; }

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
    bool PrepareScreenRequest(
        const GV2RuntimeCore::FScreenRequest& Request,
        FGV2ScreenViewModel& OutModel,
        FGV2PreparedBindingSet& OutBindings);
    bool PrepareDocumentRequest(
        const GV2RuntimeCore::FUiDocument& Document,
        FGV2UiDocumentViewModel& OutModel,
        FGV2PreparedBindingSet& OutBindings);
    void PumpIngress();
    void FailRuntime(const GV2RuntimeCore::FRuntimeFault& Fault);

    FGV2SessionStatus Status;
    GV2ContentCore::FRepositoryReadHandle PinnedRepository;
    FGV2UiBindingRegistry BindingRegistry;
    FGV2RuntimeIngressQueue IngressQueue;
    GV2RuntimeCore::FRuntimeSession RuntimeSession;
    FInteractionSink InteractionSink;
    FScreenSink ScreenSink;
    FDocumentSink DocumentSink;
    TMap<FString, FString> ActiveTabsByContainerPath;
    int64 NextInputSequence = 1;
    int64 UiRevision = 0;
    bool bPumpingIngress = false;
    bool bExecutingRuntime = false;
};
