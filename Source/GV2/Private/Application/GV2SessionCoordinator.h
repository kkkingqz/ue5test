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

    explicit FGV2SessionCoordinator(int32 InIngressCapacity = 256);

    void SetInteractionSink(FInteractionSink InSink);
    void ClearInteractionSink();
    void SetScreenSink(FScreenSink InSink);
    void ClearScreenSink();

    // PCC-36: PinnedRepository must be a valid read handle obtained from the
    // Application-scope FGV2RepositoryPublisher current snapshot at the time
    // of this call. It is held for the whole session lifetime and is never
    // swapped for a later Application-level republish (BootstrapAndSessionLifecycle.md
    // "Active session никогда не переключает pinned handle").
    bool StartSession(
        const GV2ContentCore::FRepositoryReadHandle& PinnedRepository,
        int64 RepositoryVersion = 0);
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

    bool BuildDebugStartButtonModel(FGV2ButtonViewModel& OutModel);

    EGV2SubmitUiInteractionResult SubmitUiInteraction(
        const FGV2UiBindingHandle& BindingHandle,
        const TArray<FGV2UiControlValue>& InputValues);

    bool IsExecutingRuntime() const;
    bool IsLuaVmStarted() const;
    int32 GetQueuedIngressCount() const;

private:
    static bool ValidateInputValues(
        const FGV2UiBindingRecord& Binding,
        const TArray<FGV2UiControlValue>& InputValues);
    bool PrepareScreenRequest(
        const GV2RuntimeCore::FScreenRequest& Request,
        FGV2ScreenViewModel& OutModel,
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
    int64 NextInputSequence = 1;
    int64 UiRevision = 0;
    bool bPumpingIngress = false;
    bool bExecutingRuntime = false;
};
