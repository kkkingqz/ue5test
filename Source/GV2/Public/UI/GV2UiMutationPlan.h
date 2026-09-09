#pragma once

#include "CoreMinimal.h"
#include "UI/GV2PreparedUiValue.h"
#include "UI/GV2UiCapability.h"
#include "UI/GV2PropertyConsumers.h"

class UWidget;
class UUserWidget;

/**
 * Single prepared mutation step targeting a property and its widget control.
 */
struct GV2_API FGV2UiPropertyMutation
{
    FString PropertyName;
    FString PropertyPath;
    EGV2PreparedUiValueKind Kind = EGV2PreparedUiValueKind::Null;
    TSharedPtr<IGV2PropertyConsumer> Consumer;
    TWeakObjectPtr<UWidget> TargetWidget;
    bool bIsReset = false;
    FGV2PreparedUiValue PreparedValue;
};

/**
 * Immutable/pre-calculated host mutation plan produced during Prepare.
 * Consumed strictly by Commit to execute infallible mutations.
 */
class GV2_API FGV2UiHostMutationPlan
{
public:
    void AddMutation(FGV2UiPropertyMutation InMutation)
    {
        Mutations.Add(MoveTemp(InMutation));
    }

    const TArray<FGV2UiPropertyMutation>& GetMutations() const { return Mutations; }
    int32 Num() const { return Mutations.Num(); }
    bool IsEmpty() const { return Mutations.IsEmpty(); }
    void Reset() { Mutations.Reset(); }

private:
    TArray<FGV2UiPropertyMutation> Mutations;
};

/**
 * PAH-01 (ADR-0042, INV-P4): outcome of a compensating restoration attempt -- replaying a
 * RollbackPlan, or the equivalent structural undo, after a forward Commit failed partway
 * through. Restoration itself can fail (the same engine-level fault that broke the forward
 * Commit can break its inverse too); [[nodiscard]] on every function returning this means
 * no caller can silently drop that outcome the way `void RollbackFieldPlans` and its
 * siblings did before this task -- physical state that may not match either revision must
 * be observed by the caller, not left as a log line nobody reads.
 */
struct GV2_API FGV2UiRollbackResult
{
    bool bRestored = true;
    FString FailedPropertyPath;
    FString Diagnostic;

    static FGV2UiRollbackResult Restored() { return FGV2UiRollbackResult(); }
    static FGV2UiRollbackResult RestorationFailed(FString InFailedPropertyPath, FString InDiagnostic)
    {
        FGV2UiRollbackResult Result;
        Result.bRestored = false;
        Result.FailedPropertyPath = MoveTemp(InFailedPropertyPath);
        Result.Diagnostic = MoveTemp(InDiagnostic);
        return Result;
    }
};

// PAH-01: a restoration failure is reported to its rollback boundary (GV2UiRollbackBoundary.h)
// as this diagnostic code, embedded into the boundary's own OutError -- typed and
// grep/parseable like every other `core:diagnostic.*` code in this codebase, and distinct
// from an ordinary forward-commit failure code so a test (or a future caller) can tell
// "this transaction rejected cleanly" apart from "this transaction's own undo also failed."
GV2_API extern const TCHAR* const GGV2UiRollbackFailedDiagnosticCode;

/**
 * The authoritative set of presentation value kinds that can produce a direct widget
 * mutation and therefore require an inverse entry in every rollback plan. Tests iterate
 * this set; a new applicable kind cannot silently bypass the rollback-pair gate.
 */
GV2_API TConstArrayView<EGV2PreparedUiValueKind> GetUiMutationKindsRequiringInverse();

/**
 * Verifies the mandatory one-to-one correspondence between a forward plan and its
 * inverse. The forward plan itself enumerates every mutation that must be undoable.
 */
GV2_API bool ValidateUiRollbackPlan(
    const FGV2UiHostMutationPlan& ForwardPlan,
    const FGV2UiHostMutationPlan& RollbackPlan,
    FString& OutError);

/** Builds one inverse entry for every entry in ForwardPlan from previous and candidate schemas. */
GV2_API bool PrepareUiHostRollbackPlan(
    UUserWidget* HostWidget,
    const FGV2UiCapabilityTree& Capabilities,
    const FGV2UiHostMutationPlan& ForwardPlan,
    const FGV2PreparedUiObject& PreviousCommittedProperties,
    const GV2ContentCore::FCompiledUiFieldSpec& PreviousSchema,
    const FString& PreviousSchemaId,
    const GV2ContentCore::FCompiledUiFieldSpec& CandidateSchema,
    const FString& CandidateSchemaId,
    const FString& PropertyPathPrefix,
    FGV2UiHostMutationPlan& OutPlan,
    TArray<FGV2UiSchemaCompatibilityDiagnostic>& OutDiagnostics,
    const TArray<FString>* ActiveCompositionChain = nullptr,
    const FGV2PresentationPrepareContext* PrepareContext = nullptr);

/**
 * Prepares all property mutations for a host widget off-tree without mutating physical widget state.
 * Validates capabilities, target presence, styles, formats, assets, and builds mutation plan.
 */
// DUC-11: ActiveCompositionChain (default nullptr) is forwarded unchanged to the
// NestedScreen (tab container) consumer -- see FGV2TabContainerTabsPropertyConsumer's
// composition-cycle guard and UGV2ScreenWidgetBase::PrepareScreenFields' own doc
// comment for the full picture. Every other consumer ignores it.
// PSC-06 (ADR-0043 D1): PrepareContext (default nullptr) is forwarded unchanged to the
// NestedScreen (tab container) consumer, same non-owning injection shape as
// ActiveCompositionChain -- null means "no session snapshot available to this caller",
// which the tab consumer treats as a legitimate fallback to its pre-PSC-06 behavior, not
// an error. Every other consumer ignores it.
GV2_API bool PrepareUiHostProperties(
    UUserWidget* HostWidget,
    const FGV2UiCapabilityTree& Capabilities,
    const FGV2PreparedUiObject& Candidate,
    const GV2ContentCore::FCompiledUiFieldSpec& Schema,
    const FString& SchemaId,
    const FString& PropertyPathPrefix,
    const FGV2PreparedUiObject& LastCommittedProperties,
    FGV2UiHostMutationPlan& OutPlan,
    TArray<FGV2UiSchemaCompatibilityDiagnostic>& OutDiagnostics,
    const TArray<FString>* ActiveCompositionChain = nullptr,
    const FGV2PresentationPrepareContext* PrepareContext = nullptr);

/**
 * Commits a prepared mutation plan to the physical widget hierarchy.
 * Supports failure injection to verify atomicity and error diagnostics.
 *
 * GBF-04 (ADR-0041): RollbackPlan, when given, is a validated plan prepared against the
 * host's previously committed value and schema. If Commit fails partway through Plan,
 * every mutation already applied is undone by replaying the corresponding RollbackPlan
 * mutation, in reverse order, through the same consumer Commit/Reset path -- before this
 * function returns false. Callers that are themselves already inside a self-healing
 * scope (rolling back a mutation built purely for that purpose) pass nullptr to avoid
 * recursing into a rollback of a rollback.
 *
 * PAH-01: if that self-heal replay itself fails on any mutation, OutError is overwritten
 * with GGV2UiRollbackFailedDiagnosticCode plus both the original forward failure and the
 * restoration failure -- never just the forward failure, which would silently discard
 * that the physical state may now match neither revision. RollbackFailureInjector (test-
 * only; production always omits it) mirrors FailureInjector but applies to the
 * restoration replay specifically, letting a test force that replay to fail without
 * needing a genuinely broken widget.
 */
GV2_API bool CommitUiHostProperties(
    UUserWidget* HostWidget,
    const FGV2UiHostMutationPlan& Plan,
    FString& OutFailedPropertyPath,
    FString& OutError,
    TFunction<bool(const FString& PropertyPath)> FailureInjector = nullptr,
    const FGV2UiHostMutationPlan* RollbackPlan = nullptr,
    TFunction<bool(const FString& PropertyPath)> RollbackFailureInjector = nullptr);
