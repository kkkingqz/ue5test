#pragma once

#include "Bridge/GV2BridgeTypes.h"
#include "CommonUserWidget.h"
#include "UI/GV2ScreenAnchorHost.h"
#include "UI/GV2UiMutationPlan.h"
#include "UI/GV2UiPropertyHost.h"
#include "UI/GV2UiHostSemanticState.h"
#include "UI/GV2TextPipelineHost.h"
#include "GV2ScreenWidgetBase.generated.h"

class UCanvasPanel;

struct FGV2ScreenFieldPlan
{
    TWeakObjectPtr<UUserWidget> HostWidget;
    FGV2UiHostMutationPlan MutationPlan;
    TSharedPtr<const FGV2PreparedUiObject> CommittedValue;
    std::shared_ptr<const GV2ContentCore::FCompiledUiFieldSpec> CommittedSchema;
    FString CommittedSchemaId;
    FGV2UiHostCommittedSnapshot PreviousCommittedSnapshot;
    // GBH-10 (ADR-0041): prepared off-tree alongside MutationPlan, against this host's
    // previous committed value -- restores the host to its pre-transaction state if
    // Commit fails on this host or a sibling host/screen in the same transaction.
    FGV2UiHostMutationPlan RollbackPlan;
};

struct FGV2ScreenMutationPlan
{
    TArray<FGV2ScreenFieldPlan> FieldPlans;
};

// GBF-04 (ADR-0041): restores every host in FieldPlans to its mandatory RollbackPlan, in reverse
// order, via the ordinary CommitUiHostProperties path -- not a bespoke undo per host.
// Used both to self-heal a screen whose own CommitScreenFields failed partway, and by a
// caller one level up (document reconciliation, a tab container's nested screens) that
// must undo an entire already-committed screen because a *sibling* screen/tab failed.
//
// PAH-01: best-effort across all hosts in FieldPlans (a failure on one host does not skip
// restoring the rest); [[nodiscard]] result names the first host whose restoration failed,
// so a caller can no longer call this and discard the outcome the way the old `void`
// signature let four call sites do. RollbackFailureInjector (test-only) is forwarded to
// each host's CommitUiHostProperties call.
[[nodiscard]] GV2_API FGV2UiRollbackResult RollbackFieldPlans(
    TArrayView<const FGV2ScreenFieldPlan> FieldPlans,
    TFunction<bool(const FString& PropertyPath)> RollbackFailureInjector = nullptr);

UCLASS(Blueprintable)
class GV2_API UGV2ScreenWidgetBase
    : public UCommonUserWidget
    , public IGV2TextPipelineHost
    , public IGV2ScreenAnchorHost
{
    GENERATED_BODY()

public:
    // PEP-06B/06C: implemented unconditionally on every screen, not just the ones a
    // host-local participant actually anchors -- the same "generic capability, no-op where
    // unused" shape GetScreenFieldIds() already has. Repositions this screen's own root
    // canvas' single child if its WidgetTree->RootWidget is a UCanvasPanel with exactly one
    // child; any other root shape (the overwhelming majority of screens, which are never
    // anchored) makes this a documented no-op, not a silent failure -- a screen author who
    // wants anchoring authors a UCanvasPanel root with one child, same as any other UMG
    // canvas layout; nothing else changes for a screen that doesn't. PEP-06C: the position
    // is clamped to this screen's own live size minus the content's own live size, so the
    // content never sits partly off-screen -- the property the old Slate tooltip window
    // gave for free and PEP-06B silently lost.
    virtual void SetAnchoredContentPosition(const FVector2D& LocalPosition) override;
    // UPP-27 / UPP-28: Prepares every field's full mutation plan without modifying any widget.
    // Predicts deep child failures (STATUS-004) before commit.
    // PCC-06: [[nodiscard]] -- a discarded result is exactly the swallowed-failure shape
    // this task exists to make impossible; see GV2LayeredUiReconciler.cpp for why.
    // DUC-11: ActiveCompositionChain is the ordered list of screen_ids already being
    // prepared on this call stack (root screen first). A nested screen (DUC-09's
    // FGV2TabContainerTabsPropertyConsumer) forwards its own chain plus the tab's
    // screen_id into this same parameter on the recursive call, so a screen_id
    // reappearing on its own composition path -- direct (a tab pointing at its own
    // screen) or indirect (through an intermediate screen) -- is caught as a cycle.
    // nullptr (the default, used by ApplyScreenFields/CanApplyScreenFields below and
    // by any standalone caller) simply means "not tracking a chain here": screen_id
    // is a runtime string lookup via Screen Registry, not a Blueprint class
    // reference, so UMG's own circular-dependency detection has no visibility into
    // it -- this is the only guard against it.
    [[nodiscard]] bool PrepareScreenFields(
        const TArray<FGV2ScreenFieldValue>& ScreenFields,
        FGV2ScreenMutationPlan& OutPlan,
        FString& OutError,
        const TArray<FString>* ActiveCompositionChain = nullptr,
        const FGV2PresentationPrepareContext* PrepareContext = nullptr) const;

    // Commits a prepared mutation plan. FailureInjector mirrors CommitUiHostProperties'
    // own injector (PCC-06/07 fault-injection tests only; production always omits it).
    // PAH-01: OutError carries GGV2UiRollbackFailedDiagnosticCode (GV2UiMutationPlan.h)
    // when a Commit failure's compensating rollback -- this host's own self-heal, or a
    // sibling host restored via RollbackFieldPlans -- itself fails, distinct from an
    // ordinary clean-rollback commit failure. RollbackFailureInjector is test-only
    // (production always omits it), mirroring FailureInjector but for the rollback replay.
    [[nodiscard]] bool CommitScreenFields(
        const FGV2ScreenMutationPlan& Plan,
        FString& OutError,
        TFunction<bool(const FString& PropertyPath)> FailureInjector = nullptr,
        TFunction<bool(const FString& PropertyPath)> RollbackFailureInjector = nullptr);

    // One-shot Prepare + Commit for C++ orchestration/tests. Presentation authority is
    // explicit: there is no Blueprint/no-context overload that could reconstruct style
    // by reaching back into process-global settings.
    [[nodiscard]] bool ApplyScreenFields(
        const TArray<FGV2ScreenFieldValue>& ScreenFields,
        const FGV2PresentationPrepareContext& PrepareContext);

    // Public C++ preflight: runs Prepare with the supplied immutable session snapshot
    // context and discards the plan.
    [[nodiscard]] bool CanApplyScreenFields(
        const TArray<FGV2ScreenFieldValue>& ScreenFields,
        const FGV2PresentationPrepareContext& PrepareContext) const;

    // Screen Field ids this screen's tree currently declares (via
    // IGV2ScreenFieldHost), for contract introspection/tests.
    UFUNCTION(BlueprintPure, Category = "GV2|UI|Screen")
    TArray<FName> GetScreenFieldIds() const;

};
